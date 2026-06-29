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
from collections import Counter
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

# ── Prototype distance scale ──────────────────────────────────────────────
# Real-car prototype operates in a small room / corridor; sensor range is ~20cm
# "clear".  Use 20 as the neutral default (not 400) so the dashboard shows a
# realistic "clear" reading even before the STM32 connects.
US_CLEAR_DEFAULT = 20   # cm — reported when no object detected / STM32 offline

# Initial live state used until the FIRST UART packet arrives (or while the STM32
# is disconnected).  All ultrasonics start at US_CLEAR_DEFAULT; all ADAS SAFE(0).
_stm_state = {
    "drive":      {"speedKmh": 0, "heading": 0, "pitch": 0, "roll": 0},
    "ultrasonic": {"front": US_CLEAR_DEFAULT, "frontLeft": US_CLEAR_DEFAULT,
                   "frontRight": US_CLEAR_DEFAULT, "rear": US_CLEAR_DEFAULT,
                   "rearLeft": US_CLEAR_DEFAULT, "rearRight": US_CLEAR_DEFAULT},
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

# ── Median filter (N=3) per US channel ───────────────────────────────────
# Keep a ring-buffer of the last 3 raw readings per sensor.  Return the
# "nearest-pair median": sort the 3 values, then return the average of the
# two that are closest together — this kills single-sample spikes while
# staying responsive to real distance changes.
_US_BUFSIZE = 3
_us_buf: dict = {key: [] for key in US_MAP}   # raw ring-buffer per STM32 key

def _reset_us_buffers() -> None:
    """Clear every per-channel ring-buffer. Called whenever the link (re)opens
    so stale pre-disconnect readings can't blend with fresh ones."""
    for buf in _us_buf.values():
        buf.clear()

def _median_filter(key: str, raw: float) -> float:
    """Push one raw reading into the ring-buffer for *key* and return the
    nearest-pair median.  Falls back to the raw value until the buffer fills.

    A real, sustained jump (object moved fast) shows up as two consecutive
    readings far from the oldest one: in that case the nearest pair is the two
    NEW samples, so the filter follows the move after one sample instead of
    latching onto the stale value — while a lone spike (one outlier) is still
    rejected because it never forms the closest pair."""
    buf = _us_buf[key]
    buf.append(raw)
    if len(buf) > _US_BUFSIZE:
        buf.pop(0)
    if len(buf) < 2:
        return raw
    s = sorted(buf)
    if len(s) == 2:
        return (s[0] + s[1]) / 2.0
    # 3 values: pick the pair with the smallest gap, return their mean
    d01, d12 = s[1] - s[0], s[2] - s[1]
    return (s[0] + s[1]) / 2.0 if d01 <= d12 else (s[1] + s[2]) / 2.0

# ── Majority-vote (mode) filter per ADAS module ───────────────────────────
# Keep a ring-buffer of the last N decoded severities per module and report the
# most-frequent value (mode). A lone glitch (one bad sample) can never out-vote
# the stable ones, so transient false WARNINGs are debounced. On a tie (no clear
# winner) we KEEP the last stable decision instead of flipping. Same ring-buffer
# + reset-on-reconnect pattern as the ultrasonic median filter above.
_FLAGS_BUFSIZE = 5
_MODULE_NAMES = tuple(name for name, _ in SYS_MODULE_POS)  # fcw,eebl,bsw,dnpw,ima
_flags_buf: dict = {name: [] for name in _MODULE_NAMES}    # decoded samples/module
_flags_stable: dict = {name: 0 for name in _MODULE_NAMES}  # last accepted value

# Only 0/1/2 are legal severities (firmware RiskLevel_t: SAFE/WARNING/CRITICAL).
# Each module is 2 bits wide, so a decoded 3 (0b11) is a flipped UART bit — drop
# it: never buffer it, never let it vote, never show it on the dashboard.
_VALID_SEVERITIES = (0, 1, 2)


def _reset_flags_buffers() -> None:
    """Clear every per-module vote buffer (link re-opened → drop stale votes).
    The last-stable values are left as-is so the dashboard keeps the last known
    state across a brief reconnect."""
    for buf in _flags_buf.values():
        buf.clear()


def _vote_flags(decoded: dict) -> dict:
    """Push each module's freshly-decoded severity into its ring-buffer and
    return the debounced severities. Per module: the mode of the last
    _FLAGS_BUFSIZE samples wins; on a tie we keep that module's last stable
    value. An illegal severity (3) is discarded — not buffered — so a corrupt
    sample can neither vote nor surface. Falls back to the last stable value
    (0 until the first valid sample) while the buffer is empty."""
    result = {}
    for name in _MODULE_NAMES:
        sev = decoded[name]
        buf = _flags_buf[name]
        if sev in _VALID_SEVERITIES:          # ignore corrupt 3 entirely
            buf.append(sev)
            if len(buf) > _FLAGS_BUFSIZE:
                buf.pop(0)
        if buf:
            top = Counter(buf).most_common()  # [(value, n), …] sorted by n desc
            if len(top) == 1 or top[0][1] > top[1][1]:
                _flags_stable[name] = top[0][0]   # clear winner → accept
            # else: tie → keep _flags_stable[name] unchanged
        result[name] = _flags_stable[name]    # last stable (0 until first sample)
    return result


# ── Terminal-only change tracking ────────────────────────────────────────
# Print the voted severities only when they actually change; avoids terminal
# spam while still giving real-time visibility.
_last_printed_flags = None  # tuple of last-printed severities; None = never printed


# Numeric status labels for terminal printing
_STATUS_LABEL = {0: "SAFE", 1: "WARNING", 2: "CRITICAL", 3: "INVALID"}


def _print_flags_changes(voted: dict) -> None:
    """Print the debounced (voted) module severities, only when they change.
    Mirrors exactly what the dashboard shows — not the raw line — so a glitch
    rejected by the vote filter never appears here either."""
    global _last_printed_flags
    key = tuple(voted[name] for name in _MODULE_NAMES)
    if key == _last_printed_flags:
        return
    _last_printed_flags = key
    labels = "  ".join(f"{m.upper()}:{_STATUS_LABEL[v]}" for m, v in voted.items())
    print(f"[FLAGS] {labels}")


def _packet_to_state(p: dict) -> dict:
    """Turn one parsed telemetry packet into the host-vehicle sections exactly as
    the dashboard expects them (drive/ultrasonic/adas). v2n/v2p are NOT here —
    they come from the file.

    Also applies the per-channel median filter (ultrasonic) and the per-module
    majority-vote filter (adas), and prints the voted severities to the terminal
    only when they change, without touching the dashboard at all."""
    # Apply median filter to each US channel
    filtered_us = {
        dash_key: _median_filter(stm_key, p[stm_key])
        for stm_key, dash_key in US_MAP.items()
    }

    # Debounce the ADAS severities with the majority-vote filter (kills the
    # transient false WARNING/CRITICAL glitches), then report + print the result.
    voted = _vote_flags(decode_sys_flags(int(p["sys_flags"])))
    _print_flags_changes(voted)   # terminal mirrors the dashboard, only on change

    return {
        "drive": {
            "speedKmh": p["speed"] * 0.036,   # cm/s → km/h
            "heading":  p["heading"],
            "pitch":    p["pitch"],
            "roll":     p["roll"],
        },
        "ultrasonic": filtered_us,
        "adas": voted,
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
            _reset_us_buffers()      # drop any stale US readings from before a drop
            _reset_flags_buffers()   # and stale ADAS votes
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


def _apply_vehicle_id() -> None:
    """Set meta.vehicleId in data.json based on the detected RPi model.

    RPi 5  → vehicleId = 1
    RPi 4  → vehicleId = 2
    Unknown → vehicleId unchanged (don't touch the file)

    Uses the same atomic read→modify→write pattern as the rest of the codebase
    so it never clobbers unrelated keys written by dashboard_bridge.py.
    """
    # RPi model → vehicle ID mapping (mirrors the UART baud-path selection)
    model_to_id = {5: 1, 4: 2}
    vehicle_id = model_to_id.get(_RPI_MODEL)
    if vehicle_id is None:
        print(f"[init] RPi model unknown ({_RPI_MODEL}); vehicleId not changed")
        return

    try:
        try:
            with open(DATA_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            data = {}

        data.setdefault("meta", {})["vehicleId"] = vehicle_id

        tmp = DATA_FILE + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.write("\n")
        os.replace(tmp, DATA_FILE)

        print(f"[init] RPi {_RPI_MODEL} detected → vehicleId = {vehicle_id}")
    except Exception as exc:
        print(f"[init] could not write vehicleId ({exc})")


def main():
    # Apply vehicle ID to data.json first (RPi4→2, RPi5→1), then start UART reader.
    _apply_vehicle_id()
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
