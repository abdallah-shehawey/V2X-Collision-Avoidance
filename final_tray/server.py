"""
server.py - V2X Dashboard static web server + UART bridge.

TEST MODE  (default): serves dashboard files only. Edit data.json by hand
                      and the browser updates within ~1 second.

AUTO MODE  (UART):    uncomment the uart_reader section at the bottom.
                      Reads STM32 over UART, writes data.json, AND publishes
                      "vehicle_speed" to the local IPC hub so Car_client-1.py
                      can use the speed for the crossing calculation.

Run:
    python3 server.py
Then open the printed URL.
"""

import os
import json
import socket
import time
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

HERE      = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(HERE, "data.json")
PORT      = 8000


class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *args):
        pass


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
    server  = ThreadingHTTPServer(("0.0.0.0", PORT), handler)

    print("V2X Dashboard running (TEST MODE — edit data.json by hand)")
    print(f"  local  : http://localhost:{PORT}")
    print(f"  network: http://{lan_ip()}:{PORT}")
    print("Open data.json, change a value, save → watch the page update.")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye.")


if __name__ == "__main__":
    main()


# ======================================================================
# AUTO MODE: read STM32 over UART, write data.json, publish to IPC hub.
# Uncomment and run in a separate terminal when hardware is ready.
# ======================================================================
# import serial
# from ipc_node import IPCNode
#
# US_KEYS = {"US_F": "front", "US_FL": "frontLeft", "US_FR": "frontRight",
#            "US_R": "rear",  "US_RL": "rearLeft",  "US_RR": "rearRight"}
#
# # IPC node to publish speed to hub (Car_client reads it from here)
# _ipc_server = IPCNode("uart_bridge")
#
# def _connect_ipc():
#     if _ipc_server.connect():
#         _ipc_server.start_listening()
#         print("[server] IPC hub connected — will publish 'vehicle_speed'.")
#     else:
#         print("[server] WARNING: IPC hub not reachable — speed won't reach Car_client.")
#
# def uart_reader(port="/dev/ttyUSB0", baud=115200):
#     _connect_ipc()
#     ser = serial.Serial(port, baud, timeout=1)
#     while True:
#         line = ser.readline().decode(errors="ignore").strip()
#         # e.g. "SPD=96;HDG=120;PIT=3;ROL=-2;TMP=92;EEBL=0;FCW=1;..."
#         if not line:
#             continue
#         try:
#             with open(DATA_FILE, "r", encoding="utf-8") as f:
#                 data = json.load(f)
#         except Exception:
#             data = {}
#
#         speed_kmh = data.get("drive", {}).get("speedKmh", 0.0)
#
#         for pair in line.split(";"):
#             key, _, val = pair.partition("=")
#             key, val = key.strip().upper(), val.strip()
#             if   key == "SPD":
#                 speed_kmh = float(val) * 0.036   # cm/s → km/h
#                 data.setdefault("drive", {})["speedKmh"] = speed_kmh
#             elif key == "HDG":  data.setdefault("drive", {})["heading"]      = float(val)
#             elif key == "PIT":  data.setdefault("drive", {})["pitch"]        = float(val)
#             elif key == "ROL":  data.setdefault("drive", {})["roll"]         = float(val)
#             elif key == "TMP":  data.setdefault("drive", {})["vehicleTempC"] = float(val)
#             elif key in ("EEBL", "FCW", "BSW", "DNPW", "IMA"):
#                 data.setdefault("adas", {})[key.lower()] = int(val)
#             elif key in US_KEYS:
#                 data.setdefault("ultrasonic", {})[US_KEYS[key]] = float(val)
#
#         # Write data.json (dashboard reads this)
#         tmp = DATA_FILE + ".tmp"
#         with open(tmp, "w", encoding="utf-8") as f:
#             json.dump(data, f, ensure_ascii=False, indent=2)
#         os.replace(tmp, DATA_FILE)
#
#         # Publish speed to IPC hub so Car_client can use it
#         try:
#             _ipc_server.publish("vehicle_speed", {"speed_kmh": speed_kmh})
#         except Exception:
#             pass
