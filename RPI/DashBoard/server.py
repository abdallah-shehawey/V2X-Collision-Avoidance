"""
V2X Dashboard - static web server + STM32 UART reader (AUTO MODE built in).

This file does two jobs in one process:

  1) SERVE the dashboard (HTTP + Server-Sent Events). The browser subscribes to
     `/events`; the server watches `data.json` and PUSHES the new content the
     instant the file changes → near-zero latency, no 1s polling.

  2) READ host-vehicle telemetry from the STM32 over UART and WRITE it into the
     SAME `data.json` (the old `uart.py` logic is now merged here, no separate
     process). A background daemon thread does a BLOCKING readline() on the UART
     (≈0% CPU while idle) and on every valid line updates ONLY the host-vehicle
     sections of data.json: drive.speedKmh/heading/pitch/roll, ultrasonic.*, adas.*.

Modes:
  * TEST MODE: stop the car / unplug the STM32, edit data.json by hand and save —
               the dashboard still updates the instant you save.
  * AUTO MODE: the STM32 is connected → the reader overwrites those same fields
               with live values. Nothing in the front-end changes either way.

The v2n / v2p sections are written by dashboard_bridge.py, NOT here. The reader
uses the exact same read → modify → atomic-replace pattern (and a lock) so the
two writers never clobber each other.

Run:
    python3 server.py
Then open the printed URL (e.g. http://localhost:8000).
"""

import json
import os
import socket
import threading
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

import serial  # pyserial — needed for the UART reader thread

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(HERE, "data.json")
PORT = 8000


# ======================================================================
#  HTTP server: serves the dashboard + pushes data.json over SSE.
#  (Unchanged — it never edits any value.)
# ======================================================================
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # never cache data.json so the dashboard always sees the latest edit
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *args):
        pass  # keep the console quiet

    def do_GET(self):
        if self.path.split("?")[0] == "/events":
            self.stream_events()
        else:
            super().do_GET()

    @staticmethod
    def read_merged():
        # Build the snapshot the browser sees = data.json (meta/v2n/v2p, written
        # by dashboard_bridge.py) with the LIVE STM32 sections (drive/ultrasonic/
        # adas, in memory) overlaid on top. Returns (payload_bytes, stm_seq) or
        # (None, seq) if the file is missing or mid-write (invalid JSON).
        try:
            with open(DATA_FILE, "rb") as f:
                data = json.load(f)
        except (OSError, ValueError):
            return None, _stm_seq

        # Overlay the live host-vehicle telemetry. The UART feed is the source of
        # truth for these keys, so it wins over whatever is in the file.
        #   • ultrasonic / adas: fully owned by the STM32 → replace outright.
        #   • drive: the STM32 owns speedKmh/heading/pitch/roll, but the FILE
        #     keeps the static config (maxGaugeKmh, vehicleTempC) → MERGE the
        #     live keys into drive instead of replacing the whole dict.
        with _stm_lock:
            seq = _stm_seq
            if _stm_state:
                data.setdefault("drive", {}).update(_stm_state["drive"])
                data["ultrasonic"] = _stm_state["ultrasonic"]
                data["adas"] = _stm_state["adas"]

        return json.dumps(data, ensure_ascii=False).encode("utf-8"), seq

    def stream_events(self):
        # Server-Sent Events: push a new snapshot the instant EITHER source
        # changes — the data.json file (mtime) OR the live STM32 state (_stm_seq).
        # Both checks are local & cheap; the network only carries data on change.
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        last_mtime = None
        last_seq = None
        last_payload = None
        ticks = 0
        try:
            while True:
                try:
                    mtime = os.path.getmtime(DATA_FILE)
                except OSError:
                    mtime = None

                # Re-render if the file changed OR a new UART packet arrived.
                payload = None
                if mtime != last_mtime or _stm_seq != last_seq:
                    payload, seq = self.read_merged()
                    last_mtime = mtime
                    last_seq = seq

                if payload is not None and payload != last_payload:
                    self.wfile.write(b"data: " + payload + b"\n\n")
                    self.wfile.flush()
                    last_payload = payload
                elif ticks % 150 == 0:
                    # keep-alive comment (~every 15s): also detects a
                    # disconnected client so this thread can exit.
                    self.wfile.write(b": ping\n\n")
                    self.wfile.flush()
                ticks += 1
                time.sleep(0.1)
        except (BrokenPipeError, ConnectionResetError):
            pass  # client closed the tab; end this stream thread


def lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


# ======================================================================
#  STM32 UART reader (merged from Rpi_com/uart.py).
#
#  The STM32 sends an ASCII, '\n'-delimited CSV line every 100ms (it switched
#  away from the old binary 0xAA..0x55 packet because that packet's zero-bytes
#  were being lost on this UART link). One line:
#
#      T,speed,heading,pitch,roll,FL,FC,FR,BL,BC,BR,flags\n
#      T,0.0,215.8,0.1,0.0,17,10,22,29,63,18,0
#
#      field 0  : "T"     line marker
#      field 1  : speed   cm/s
#      field 2  : heading degrees 0-360
#      field 3  : pitch   degrees
#      field 4  : roll    degrees
#      fields 5-10 : FrontLeft, FrontCenter, FrontRight, BackLeft, BackCenter,
#                    BackRight ultrasonic distances [cm]
#      field 11 : flags   G_u16SystemFlags (2 bits/module), as an unsigned int
#
#  Wiring (3.3V only): Pi RXD(pin10/GPIO15) <- STM32 TX,  GND <-> GND.
#  RPi5: ttyAMA0 is the GPIO UART natively. RPi4: after dtoverlay=disable-bt,
#  PL011 appears as ttyAMA0 on GPIO14/15. Both end up on /dev/ttyAMA0 — the
#  difference is ONLY the baud-setting method (see _open_serial_rpi4).
# ======================================================================

# RPi4/RPi5 detection — exactly the trick from uart.py: read the board model
# once at import so listen() picks the right baud-setting method automatically.
def _detect_rpi_model():
    """Return 4, 5, or 0 (unknown) by reading /proc/device-tree/model."""
    try:
        with open("/proc/device-tree/model", "r") as f:
            model = f.read()
        if "Raspberry Pi 5" in model:
            return 5
        if "Raspberry Pi 4" in model:
            return 4
    except OSError:
        pass
    return 0


_RPI_MODEL = _detect_rpi_model()

# Both boards use the GPIO UART at /dev/ttyAMA0.
UART_PORT = "/dev/ttyAMA0"

# The STM32 firmware is *configured* for 9600 but actually transmits at ~120000
# (baud-calc bug on its side). Measured working window on the wire: 116.5k-124k,
# center ~120k. Set the Pi to the center for max margin until the STM32 is fixed.
UART_BAUD = 120000

LINE_MARKER = "T"

# CSV column order matching the firmware's snprintf in main.c. Column 0 is the
# "T" marker, consumed separately, so this lists columns 1..11.
PACKET_FIELDS = ("speed", "heading", "pitch", "roll",
                 "FrontLeftUS", "FrontCenterUS", "FrontRightUS",
                 "BackLeftUS", "BackCenterUS", "BackRightUS",
                 "sys_flags")

# ── sys_flags decode (G_u16SystemFlags) ──────────────────────────────────
# 2 bits per ADAS module: 0b00 SAFE | 0b01 WARNING | 0b10 CRITICAL.
# Bit positions inside the u16 (must match main.c / SafetyEngine):
SYS_MASK = 0b11
SYS_MODULE_POS = (
    ("fcw",  0),   # bits 1:0  Forward Collision Warning
    ("eebl", 2),   # bits 3:2  Emergency Electronic Brake Light
    ("bsw",  4),   # bits 5:4  Blind Spot Warning
    ("dnpw", 6),   # bits 7:6  Do Not Pass Warning
    ("ima",  8),   # bits 9:8  Intersection Movement Assist
)


def decode_sys_flags(flags):
    """Split the 16-bit status word into {module: 0|1|2} (the numeric severity
    the dashboard's "adas" section expects — same shape as v2n/v2p flags)."""
    return {name: (flags >> pos) & SYS_MASK for name, pos in SYS_MODULE_POS}


def parse_line(line):
    """Parse one CSV telemetry line into a packet dict, or return None if the
    line is malformed (wrong marker, wrong column count, or non-numeric field).
    No checksum — the STM32 sends none; structure is the only validity check."""
    parts = line.strip().split(",")
    if len(parts) != len(PACKET_FIELDS) + 1 or parts[0] != LINE_MARKER:
        return None
    try:
        # all values are floats except sys_flags which is an integer bitfield
        values = [float(p) for p in parts[1:-1]]
        values.append(int(parts[-1]))
    except ValueError:
        return None
    return dict(zip(PACKET_FIELDS, values))


def _open_serial_rpi4(port, baudrate):
    """Open the serial port on RPi4 with a non-standard baud rate via termios2.
    PL011 (ttyAMA0) supports arbitrary rates through BOTHER/TCSETS2, unlike the
    mini-UART. (Copied verbatim from uart.py — it is the RPi4-specific path.)

    struct termios2 layout (little-endian, 44 bytes):
        offset  0  c_iflag   uint32
        offset  4  c_oflag   uint32
        offset  8  c_cflag   uint32   <- clear CBAUD bits, OR in BOTHER
        offset 12  c_lflag   uint32
        offset 16  c_line    uint8
        offset 17  c_cc[19]  19 bytes
        offset 36  c_ispeed  uint32   <- exact input baud
        offset 40  c_ospeed  uint32   <- exact output baud
    """
    import fcntl, struct
    TCGETS2 = 0x802C542A
    TCSETS2 = 0x402C542B
    BOTHER  = 0o010000
    CBAUD   = 0o010017

    ser = serial.Serial(port, 9600, timeout=None)        # open at any valid rate first
    buf = bytearray(fcntl.ioctl(ser.fd, TCGETS2, bytes(44)))
    cflag = struct.unpack_from('<I', buf, 8)[0]
    cflag = (cflag & ~CBAUD) | BOTHER
    struct.pack_into('<I', buf, 8, cflag)                # c_cflag
    struct.pack_into('<I', buf, 36, baudrate)            # c_ispeed
    struct.pack_into('<I', buf, 40, baudrate)            # c_ospeed
    fcntl.ioctl(ser.fd, TCSETS2, bytes(buf))
    return ser


def _open_uart(port=UART_PORT, baudrate=UART_BAUD):
    """Open the UART, choosing the baud-setting method by board model. RPi4 needs
    the termios2 trick for the non-standard 120000; RPi5 (and unknown) let
    pyserial set it directly."""
    if _RPI_MODEL == 4:
        ser = _open_serial_rpi4(port, baudrate)
    else:
        ser = serial.Serial(port, baudrate, timeout=None)
    ser.reset_input_buffer()
    return ser


# ──────────────────────────────────────────────────────────────────────
#  Live STM32 state — kept IN MEMORY, never written to disk.
#
#  The whole point: the host-vehicle telemetry (drive/ultrasonic/adas) is the
#  fast-changing live feed coming straight off the UART. Writing it to the SD
#  card every 100ms would wear the card and add latency for nothing, so we keep
#  it in this dict and the SSE stream overlays it on top of the file snapshot
#  (meta/v2n/v2p, which dashboard_bridge.py still writes to data.json as before).
#
#  `_stm_lock` guards the dict; `_stm_seq` bumps on every update so the SSE loop
#  can tell "did the live data change?" without diffing the whole dict.
# ──────────────────────────────────────────────────────────────────────
_stm_lock = threading.Lock()
_stm_seq = 0           # bumped on every packet → cheap change-detection for SSE

# Initial live state used until the FIRST UART packet arrives (or while the STM32
# is disconnected). These are the same neutral defaults the values used to have
# in data.json — speed/attitude at 0, all ultrasonics "clear" at 400cm, all ADAS
# modules SAFE (0) — so the dashboard renders fine with no STM32 attached.
_stm_state = {
    "drive":      {"speedKmh": 0, "heading": 0, "pitch": 0, "roll": 0},
    "ultrasonic": {"front": 400, "frontLeft": 400, "frontRight": 400,
                   "rear": 400, "rearLeft": 400, "rearRight": 400},
    "adas":       {"fcw": 0, "eebl": 0, "bsw": 0, "dnpw": 0, "ima": 0},
}

# STM32 ultrasonic names → data.json "ultrasonic" keys. The two CENTER sensors
# map to front/rear; the side sensors map to the four corners.
US_MAP = {
    "FrontCenterUS": "front",
    "FrontLeftUS":   "frontLeft",
    "FrontRightUS":  "frontRight",
    "BackCenterUS":  "rear",
    "BackLeftUS":    "rearLeft",
    "BackRightUS":   "rearRight",
}


def _packet_to_state(p: dict) -> dict:
    """Turn one parsed telemetry packet into the host-vehicle sections exactly as
    the dashboard expects them (drive/ultrasonic/adas). v2n/v2p are NOT here —
    they come from the file."""
    return {
        "drive": {
            "speedKmh": p["speed"] * 0.036,   # cm/s → km/h
            "heading":  p["heading"],
            "pitch":    p["pitch"],
            "roll":     p["roll"],
        },
        "ultrasonic": {dash_key: p[stm_key] for stm_key, dash_key in US_MAP.items()},
        "adas": decode_sys_flags(int(p["sys_flags"])),
    }


def _publish_state(new_state: dict) -> None:
    """Replace the in-memory live state and bump the sequence counter. The SSE
    loop notices the bump and re-sends — no disk I/O at all."""
    global _stm_state, _stm_seq
    with _stm_lock:
        _stm_state = new_state
        _stm_seq += 1


def uart_reader_loop():
    """Block on the UART forever, publishing every valid line to the in-memory
    live state (NOT to disk). readline() sleeps the thread in the kernel until a
    '\\n' arrives — no polling, ≈0% CPU while idle (interrupt-style RX from uart.py).

    Resilient: if the port can't be opened (STM32 not connected yet) or drops,
    it logs and retries every 2s instead of killing the whole server."""
    while True:
        try:
            ser = _open_uart()
            print(f"[uart] reading {UART_PORT} @ {UART_BAUD} baud "
                  f"(RPi model {_RPI_MODEL or '?'}) → live (in-memory)")
        except Exception as exc:
            print(f"[uart] open failed ({exc}); retrying in 2s…")
            time.sleep(2)
            continue

        try:
            while True:
                raw = ser.readline()        # sleeps here (0% CPU) until '\n'
                line = raw.decode("ascii", "ignore")
                packet = parse_line(line)
                if packet is not None:
                    _publish_state(_packet_to_state(packet))
        except (serial.SerialException, OSError) as exc:
            print(f"[uart] link error ({exc}); reopening in 2s…")
            try:
                ser.close()
            except Exception:
                pass
            time.sleep(2)


def start_uart_reader():
    """Run uart_reader_loop() in a background daemon thread so the HTTP server
    keeps running. Daemon → it dies with the process on Ctrl+C."""
    t = threading.Thread(target=uart_reader_loop, daemon=True)
    t.start()
    return t


def main():
    # Start the STM32 → data.json reader first, then serve the dashboard.
    start_uart_reader()

    handler = partial(Handler, directory=HERE)
    server = ThreadingHTTPServer(("0.0.0.0", PORT), handler)

    print("V2X Dashboard running (AUTO MODE — live STM32 telemetry → data.json)")
    print(f"  local  : http://localhost:{PORT}")
    print(f"  network: http://{lan_ip()}:{PORT}")
    print("If the STM32 is disconnected you can still edit data.json by hand.")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye.")


if __name__ == "__main__":
    main()
