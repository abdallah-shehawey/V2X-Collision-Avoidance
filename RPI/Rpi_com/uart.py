#!/usr/bin/env python3
"""
uart.py — Receive host-vehicle telemetry from the STM32 over UART (Raspberry Pi 5).

The STM32 sends an ASCII, '\\n'-delimited CSV line every 100ms (it switched away
from the old binary 0xAA..0x55 packet because that packet's zero-bytes were being
lost on this UART link). One line looks like:

    T,speed,heading,pitch,roll,FL,FC,FR,BL,BC,BR,flags\\n
    T,0.0,215.8,0.1,0.0,17,10,22,29,63,18,0

    field 0  : "T"     line marker
    field 1  : speed   cm/s
    field 2  : heading degrees 0-360
    field 3  : pitch   degrees
    field 4  : roll    degrees
    fields 5-10 : FrontLeft, FrontCenter, FrontRight, BackLeft, BackCenter,
                  BackRight ultrasonic distances [cm]
    field 11 : flags   G_u16SystemFlags (2 bits/module), as an unsigned int

Event-driven, NO polling: listen() does a BLOCKING readline(), so the kernel puts
the thread to sleep until a line arrives (~0% CPU) and wakes it the instant the
'\\n' shows up. Every valid line fires your callback.

Wiring (3.3V only):  Pi RXD(pin10/GPIO15) <- STM32 TX,  GND <-> GND
(Pi TXD->STM32 RX only needed if the Pi ever talks back; this is RX-only.)
"""

import threading

import serial  # pyserial

# On Raspberry Pi 5 the GPIO14/15 UART (pins 8/10) is /dev/ttyAMA0 (RP1 UART0).
# NOTE: /dev/serial0 here points to the Bluetooth UART, NOT the header pins.
PORT = "/dev/ttyAMA0"
# The STM32 firmware is *configured* for 9600 but actually transmits at ~120000
# (baud-calc bug on its side). Measured working window on the wire: 116.5k-124k,
# center ~120k. Set the Pi to the center for max margin until the STM32 is fixed.
BAUDRATE = 120000

LINE_MARKER = "T"

# CSV column order, matching the firmware's snprintf in main.c. The first column
# is the "T" marker and is consumed separately, so this lists columns 1..11.
PACKET_FIELDS = ("speed", "heading", "pitch", "roll",
                 "FrontLeftUS", "FrontCenterUS", "FrontRightUS",
                 "BackLeftUS", "BackCenterUS", "BackRightUS",
                 "sys_flags")

# ── sys_flags decode (G_u16SystemFlags) ──────────────────────────────────
# 2 bits per ADAS module:  0b00 SAFE | 0b01 WARNING | 0b10 CRITICAL
# Bit positions inside the u16 (listed low->high; bits 15:10 reserved):
SYS_MASK = 0b11
SYS_MODULE_POS = (
    ("FCW",  0),   # bits 1:0  Forward Collision Warning
    ("EEBL", 2),   # bits 3:2  Emergency Electronic Brake Light
    ("BSW",  4),   # bits 5:4  Blind Spot Warning (WARNING only)
    ("DNPW", 6),   # bits 7:6  Do Not Pass Warning
    ("IMA",  8),   # bits 9:8  Intersection Movement Assist
)
SYS_STATUS = {0: "SAFE", 1: "WARNING", 2: "CRITICAL", 3: "INVALID"}


def decode_sys_flags(flags):
    """Split the 16-bit status word into {module: 'SAFE'|'WARNING'|'CRITICAL'}."""
    return {name: SYS_STATUS[(flags >> pos) & SYS_MASK]
            for name, pos in SYS_MODULE_POS}


def parse_line(line):
    """Parse one CSV telemetry line into a packet dict, or return None if the line
    is malformed (wrong marker, wrong column count, or a non-numeric field).
    No checksum — the STM32 sends none; structure is the only validity check."""
    parts = line.strip().split(",")
    # marker + 11 values
    if len(parts) != len(PACKET_FIELDS) + 1 or parts[0] != LINE_MARKER:
        return None
    try:
        # all values are floats except sys_flags which is an integer bitfield
        values = [float(p) for p in parts[1:-1]]
        values.append(int(parts[-1]))
    except ValueError:
        return None
    packet = dict(zip(PACKET_FIELDS, values))
    packet["status"] = decode_sys_flags(packet["sys_flags"])
    return packet


def listen(on_packet, port=PORT, baudrate=BAUDRATE):
    """Block on the UART forever, calling on_packet(dict) for every valid line.
    readline() below sleeps the thread in the kernel until a '\\n' arrives — no
    polling, no CPU spent while idle. This is the interrupt-style receive."""
    ser = serial.Serial(port, baudrate, timeout=None)  # timeout=None -> blocking
    ser.reset_input_buffer()
    while True:
        raw = ser.readline()           # <-- sleeps here (0% CPU) until '\n' arrives
        line = raw.decode("ascii", "ignore")
        packet = parse_line(line)
        if packet is not None:
            on_packet(packet)


def start_listening(on_packet, port=PORT, baudrate=BAUDRATE):
    """Run listen() in a background daemon thread so your main code keeps running."""
    t = threading.Thread(target=listen, args=(on_packet, port, baudrate), daemon=True)
    t.start()
    return t


def _print_packet(p):
    """Print one packet with each field on its own line (stacked, not inline)."""
    print("[uart] RX:")
    print(f"  speed   = {p['speed']}")
    print(f"  heading = {p['heading']}")
    print(f"  pitch   = {p['pitch']}")
    print(f"  roll    = {p['roll']}")
    print(f"  FL      = {p['FrontLeftUS']}")
    print(f"  FC      = {p['FrontCenterUS']}")
    print(f"  FR      = {p['FrontRightUS']}")
    print(f"  BL      = {p['BackLeftUS']}")
    print(f"  BC      = {p['BackCenterUS']}")
    print(f"  BR      = {p['BackRightUS']}")
    print(f"  flags   = {p['sys_flags']}")
    for module, state in p["status"].items():
        print(f"    {module:<5}= {state}")
    print("-" * 30)


if __name__ == "__main__":
    print(f"[uart] listening on {PORT} @ {BAUDRATE} baud (Ctrl+C to stop)")
    try:
        listen(_print_packet)
    except KeyboardInterrupt:
        pass
