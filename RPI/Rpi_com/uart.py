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
RPi4: GPIO UART = /dev/ttyS0 (/dev/serial0). Enable with: sudo raspi-config -> Interface -> Serial.
"""

import threading
import os

import serial  # pyserial


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

# RPi5: ttyAMA0 is the GPIO UART natively.
# RPi4: after dtoverlay=disable-bt in /boot/firmware/config.txt, PL011 is freed
#        from Bluetooth and appears as ttyAMA0 on GPIO14/15 (pins 8/10).
# Both boards end up using /dev/ttyAMA0 — the difference is the baud-setting method.
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


def _open_serial_rpi4(port, baudrate):
    """Open serial port on RPi4 with a non-standard baud rate using termios2.
    PL011 (ttyAMA0) supports arbitrary rates via BOTHER/TCSETS2, unlike mini-UART.

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


def listen(on_packet, port=PORT, baudrate=BAUDRATE):
    """Block on the UART forever, calling on_packet(dict) for every valid line.
    readline() below sleeps the thread in the kernel until a '\\n' arrives — no
    polling, no CPU spent while idle. This is the interrupt-style receive."""
    if _RPI_MODEL == 4:
        ser = _open_serial_rpi4(port, baudrate)
    else:
        # RPi5 (and unknown): pyserial handles the baud rate directly
        ser = serial.Serial(port, baudrate, timeout=None)
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
