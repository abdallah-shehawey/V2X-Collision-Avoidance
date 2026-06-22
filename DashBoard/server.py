"""
V2X Dashboard - static web server.

This server ONLY serves the dashboard files. It never changes any value.
The browser subscribes to `/events` (Server-Sent Events). The server watches
`data.json` and PUSHES the new content the instant the file changes, so the
dashboard updates with near-zero latency (no 1s polling delay), and:

  * TEST MODE  (now):   YOU edit data.json by hand and save it. The dashboard
                        updates the instant you save. This is how you confirm
                        the whole chain works before the hardware is connected.

  * AUTO MODE  (later): a separate process reads the STM over UART and writes
                        the SAME data.json. The dashboard does not change at
                        all. A skeleton for that writer is at the bottom of
                        this file (uart_reader, not used yet).

Run:
    python3 server.py
Then open the printed URL (e.g. http://localhost:8000).
"""

import json
import os
import socket
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(HERE, "data.json")
PORT = 8000


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
    def read_compact():
        # read data.json, validate it, return it as ONE line (SSE-safe).
        # returns None if the file is missing or mid-write (invalid JSON).
        try:
            with open(DATA_FILE, "rb") as f:
                data = json.load(f)
            return json.dumps(data, ensure_ascii=False).encode("utf-8")
        except (OSError, ValueError):
            return None

    def stream_events(self):
        # Server-Sent Events: watch data.json's mtime and push the new
        # content the instant it changes. The os.stat poll is local & cheap;
        # the network only carries data when the file actually changes.
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        last_mtime = None
        last_payload = None
        ticks = 0
        try:
            while True:
                try:
                    mtime = os.path.getmtime(DATA_FILE)
                except OSError:
                    mtime = None
                payload = None
                if mtime is not None and mtime != last_mtime:
                    payload = self.read_compact()
                    last_mtime = mtime
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


def main():
    handler = partial(Handler, directory=HERE)
    server = ThreadingHTTPServer(("0.0.0.0", PORT), handler)

    print("V2X Dashboard running (TEST MODE - edit data.json by hand)")
    print(f"  local  : http://localhost:{PORT}")
    print(f"  network: http://{lan_ip()}:{PORT}")
    print("Open data.json, change a value, save -> watch the page update.")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye.")


if __name__ == "__main__":
    main()


# ======================================================================
#  AUTO MODE (later): read STM over UART and write data.json.
#  Run this in a separate terminal next to the server. Nothing in the
#  front-end changes. Uncomment and fill in when the hardware is ready.
# ======================================================================
# import json, time, serial
#
# # Ultrasonic distances are in centimetres. US_F = front, US_FL = front-left,
# # US_FR = front-right, US_R = rear, US_RL = rear-left, US_RR = rear-right.
# US_KEYS = {"US_F": "front", "US_FL": "frontLeft", "US_FR": "frontRight",
#            "US_R": "rear",  "US_RL": "rearLeft",  "US_RR": "rearRight"}
#
# def uart_reader(port="/dev/ttyUSB0", baud=115200):
#     ser = serial.Serial(port, baud, timeout=1)
#     while True:
#         line = ser.readline().decode(errors="ignore").strip()
#         # e.g. "SPD=96;HDG=120;PIT=3;ROL=-2;TMP=92;EEBL=0;FCW=1;BSW=0;DNPW=0;IMA=2;
#         #       US_F=140;US_FL=210;US_FR=55;US_R=300;US_RL=180;US_RR=30"
#         if not line:
#             continue
#         with open(DATA_FILE, "r", encoding="utf-8") as f:
#             data = json.load(f)
#         for pair in line.split(";"):
#             key, _, val = pair.partition("=")
#             key, val = key.strip().upper(), val.strip()
#             if   key == "SPD":  data["drive"]["speedKmh"]     = float(val) * 0.036  # cm/s -> km/h
#             elif key == "HDG":  data["drive"]["heading"]      = float(val)
#             elif key == "PIT":  data["drive"]["pitch"]        = float(val)
#             elif key == "ROL":  data["drive"]["roll"]         = float(val)
#             elif key == "TMP":  data["drive"]["vehicleTempC"] = float(val)
#             elif key in ("EEBL", "FCW", "BSW", "DNPW", "IMA"):
#                 data["adas"][key.lower()] = int(val)
#             elif key in US_KEYS:
#                 data.setdefault("ultrasonic", {})[US_KEYS[key]] = float(val)
#         tmp = DATA_FILE + ".tmp"
#         with open(tmp, "w", encoding="utf-8") as f:
#             json.dump(data, f, ensure_ascii=False, indent=2)
#         os.replace(tmp, DATA_FILE)
