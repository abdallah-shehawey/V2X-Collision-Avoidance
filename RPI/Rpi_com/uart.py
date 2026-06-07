#!/usr/bin/env python3
"""
uart.py — Receive DSRC packets from the STM32 over UART (Raspberry Pi 5).

Frame the STM32 sends (V2X-without-RTOS):
    [0xAA] [Neighbor struct = 28 bytes] [XOR checksum] [0x55]

Event-driven, NO polling: listen() does a BLOCKING read, so the kernel puts the
thread to sleep until a byte arrives (~0% CPU) and wakes it the instant it does —
the userspace equivalent of a UART RX interrupt. Every valid frame fires your
callback.

Wiring (3.3V only):  Pi TXD(pin14)->STM32 RX(PA10),  Pi RXD(pin15)->STM32 TX(PA9),  GND<->GND
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

# Neighbor struct layout on the STM32 (Cortex-M4, little-endian, default padding).
# '<' = little-endian, no auto-align; 'Nx' = N C padding bytes. Total = 28 bytes.
#   B vehicle_id @0 | 3x | f speed @4 | f heading @8 | I last_update @12
#   | B fcw_flag @16 | B dnpw_flag @17 | 2x | f distance_to_intersection @20
#   | B ima_flag @24 | 3x
NEIGHBOR_FMT = "<B3xffIBB2xfB3x"
NEIGHBOR_SIZE = struct.calcsize(NEIGHBOR_FMT)  # = 28
NEIGHBOR_FIELDS = ("vehicle_id", "speed", "heading", "last_update",
                   "fcw_flag", "dnpw_flag", "distance_to_intersection", "ima_flag")


def _xor(data):
    chk = 0
    for b in data:
        chk ^= b
    return chk


class DsrcParser:
    """Feed bytes one at a time; returns a packet dict when a valid frame
    completes (checksum OK), else None. Same state machine as DSRC_RxCallback."""

    WAIT_START, READ_DATA, READ_CHECKSUM, READ_END = range(4)

    def __init__(self):
        self.state = self.WAIT_START
        self.buf = bytearray()
        self.checksum = 0

    def feed(self, byte):
        if self.state == self.WAIT_START:
            if byte == START_BYTE:
                self.buf = bytearray()
                self.state = self.READ_DATA
        elif self.state == self.READ_DATA:
            self.buf.append(byte)
            if len(self.buf) >= NEIGHBOR_SIZE:
                self.state = self.READ_CHECKSUM
        elif self.state == self.READ_CHECKSUM:
            self.checksum = byte
            self.state = self.READ_END
        elif self.state == self.READ_END:
            self.state = self.WAIT_START
            if byte == END_BYTE and _xor(self.buf) == self.checksum:
                values = struct.unpack(NEIGHBOR_FMT, bytes(self.buf))
                return dict(zip(NEIGHBOR_FIELDS, values))
        return None


def listen(on_packet, port=PORT, baudrate=BAUDRATE):
    """Block on the UART forever, calling on_packet(dict) for every valid frame.
    The read() below sleeps the thread in the kernel until data arrives — no polling,
    no CPU spent while idle. This is the interrupt-style receive."""
    ser = serial.Serial(port, baudrate, timeout=None)  # timeout=None -> blocking
    ser.reset_input_buffer()
    parser = DsrcParser()
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


def send_neighbor(ser, vehicle_id, speed, heading, last_update,
                  fcw_flag, dnpw_flag, distance_to_intersection, ima_flag):
    """Build and send one DSRC frame (start + struct + checksum + end)."""
    raw = struct.pack(NEIGHBOR_FMT, vehicle_id, speed, heading, last_update,
                      fcw_flag, dnpw_flag, distance_to_intersection, ima_flag)
    ser.write(bytes([START_BYTE]) + raw + bytes([_xor(raw), END_BYTE]))
    ser.flush()


if __name__ == "__main__":
    print(f"[uart] listening on {PORT} @ {BAUDRATE} baud (Ctrl+C to stop)")
    try:
        listen(lambda p: print("[uart] RX:", p))
    except KeyboardInterrupt:
        pass
