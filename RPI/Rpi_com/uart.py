#!/usr/bin/env python3
"""
uart.py — Receive host-vehicle telemetry from the STM32 over UART (Raspberry Pi 5).

Frame the STM32 sends (RPi_Packet_t, __attribute__((packed)), NO checksum):
    [0xAA] [sys_flags u16] [10x float32] [0x55]   = 44 bytes total
    payload between start/end = sys_flags + 10 floats = 42 bytes

Event-driven, NO polling: listen() does a BLOCKING read, so the kernel puts the
thread to sleep until a byte arrives (~0% CPU) and wakes it the instant it does —
the userspace equivalent of a UART RX interrupt. Every valid frame fires your
callback.

Wiring (3.3V only):  Pi TXD(pin14)->STM32 RX,  Pi RXD(pin15)->STM32 TX,  GND<->GND
"""

import struct
import threading

import serial  # pyserial

# On Raspberry Pi 5 the GPIO14/15 UART (pins 8/10) is /dev/ttyAMA0 (RP1 UART0).
# NOTE: /dev/serial0 here points to the Bluetooth UART, NOT the header pins.
PORT = "/dev/ttyAMA0"
# The STM32 firmware is *configured* for 9600 but actually transmits at ~120000
# (baud-calc bug on its side). Measured working window on the wire: 116.5k-124k,
# center ~120k. Set the Pi to the center for max margin until the STM32 is fixed.
BAUDRATE = 120000

START_BYTE = 0xAA
END_BYTE = 0x55

# Payload layout = everything between start and end of RPi_Packet_t.
# The C struct is __attribute__((packed)) on a little-endian Cortex-M, so '<'
# (little-endian, no auto-align) maps 1:1 with no padding bytes.
#   H sys_flags @0 | f FrontLeftUS @2 | f FrontCenterUS @6 | f FrontRightUS @10
#   | f BackLeftUS @14 | f BackCenterUS @18 | f BackRightUS @22 | f speed @26
#   | f heading @30 | f pitch @34 | f roll @38   -> 42 bytes
PACKET_FMT = "<H10f"
PACKET_SIZE = struct.calcsize(PACKET_FMT)  # = 42
PACKET_FIELDS = ("sys_flags",
                 "FrontLeftUS", "FrontCenterUS", "FrontRightUS",
                 "BackLeftUS", "BackCenterUS", "BackRightUS",
                 "speed", "heading", "pitch", "roll")

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


class PacketParser:
    """Feed bytes one at a time; returns a packet dict when a valid frame
    completes (start + 42-byte payload + end), else None. No checksum — the
    STM32 sends none, the 0xAA/0x55 framing is the only validity check."""

    WAIT_START, READ_DATA, READ_END = range(3)

    def __init__(self):
        self.state = self.WAIT_START
        self.buf = bytearray()

    def feed(self, byte):
        if self.state == self.WAIT_START:
            if byte == START_BYTE:
                self.buf = bytearray()
                self.state = self.READ_DATA
        elif self.state == self.READ_DATA:
            self.buf.append(byte)
            if len(self.buf) >= PACKET_SIZE:
                self.state = self.READ_END
        elif self.state == self.READ_END:
            self.state = self.WAIT_START
            if byte == END_BYTE:
                values = struct.unpack(PACKET_FMT, bytes(self.buf))
                packet: dict = dict(zip(PACKET_FIELDS, values))
                packet["status"] = decode_sys_flags(packet["sys_flags"])
                return packet
        return None


def listen(on_packet, port=PORT, baudrate=BAUDRATE):
    """Block on the UART forever, calling on_packet(dict) for every valid frame.
    The read() below sleeps the thread in the kernel until data arrives — no polling,
    no CPU spent while idle. This is the interrupt-style receive."""
    ser = serial.Serial(port, baudrate, timeout=None)  # timeout=None -> blocking
    ser.reset_input_buffer()
    parser = PacketParser()
    while True:
        byte = ser.read(1)            # <-- sleeps here (0% CPU) until a byte arrives
        packet = parser.feed(byte[0])
        if packet is not None:
            on_packet(packet)


def start_listening(on_packet, port=PORT, baudrate=BAUDRATE):
    """Run listen() in a background daemon thread so your main code keeps running."""
    t = threading.Thread(target=listen, args=(on_packet, port, baudrate), daemon=True)
    t.start()
    return t


if __name__ == "__main__":
    print(f"[uart] listening on {PORT} @ {BAUDRATE} baud (Ctrl+C to stop)")
    try:
        listen(lambda p: print("[uart] RX:", p))
    except KeyboardInterrupt:
        pass
