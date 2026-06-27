#!/usr/bin/env python3
"""
recv_raw.py — Dump ANY bytes that arrive on the UART, even garbage.

No parser, no checksum, no framing — just print whatever shows up. Use this to
check that the wiring actually carries data. If you see bytes (even nonsense),
the link is alive and only the baud/format needs fixing.

    python recv_raw.py            # default: /dev/ttyAMA0 @ 9600
    python recv_raw.py 115200     # try another baud
"""

import sys
import serial

PORT = "/dev/ttyAMA0"   # GPIO14/15 UART on Raspberry Pi 5 (pins 8/10)
baud = int(sys.argv[1]) if len(sys.argv) > 1 else 120000  # STM32 real TX rate

ser = serial.Serial(PORT, baud, timeout=1)
ser.reset_input_buffer()
print(f"listening on {PORT} @ {baud} baud — Ctrl+C to stop")

try:
    while True:
        data = ser.read(64)        # returns whatever arrived within 1s (or b'')
        if data:
            print("hex:", data.hex(" "), "| text:", data.decode("ascii", "replace"))
except KeyboardInterrupt:
    pass
finally:
    ser.close()
