
"""fota_bench_test.py — push a .fpkg into the FOTA bootloader over a wired
UART, from a PC, with no Raspberry Pi or Firebase involved.

This is the Stage 3 bench-test tool from ../../docs/FOTA.md's roadmap: it
lets you validate the bootloader's whole receive/flash/verify/commit
pipeline with nothing more than a USB-TTL adapter wired to the STM32's
UART4 (PA0/PA1 — TX/RX crossed between the adapter and the board, plus a
shared GND) and a .fpkg built by fota_packager.py. See
../README.md for full wiring and step-by-step bring-up instructions.

Talks the exact protocol defined in
../../Inc/Application/FOTA/FOTA_Protocol_interface.h — if you change an
opcode, a field size, or the chunk size there, update this script (and
fota_packager.py) to match.

Usage:
    python3 fota_bench_test.py --port /dev/ttyUSB0 --file firmware_v42.fpkg

Requires: pip install pyserial
"""
import argparse
import struct
import sys
import time
import zlib

import serial

START_BYTE = 0xAA
END_BYTE = 0x55

CMD_HELLO = 0x01
CMD_HELLO_ACK = 0x02
CMD_START_UPDATE = 0x10
CMD_START_ACK = 0x11
CMD_START_NAK = 0x12
CMD_DATA_CHUNK = 0x20
CMD_CHUNK_ACK = 0x21
CMD_CHUNK_NAK = 0x22
CMD_END_UPDATE = 0x30
CMD_END_ACK = 0x31
CMD_END_NAK = 0x32
CMD_COMMIT = 0x40
CMD_COMMIT_ACK = 0x41
CMD_COMMIT_NAK = 0x42

CHUNK_SIZE = 256                 # must match FOTA_PROTO_CHUNK_SIZE
CHUNK_TIMEOUT_S = 1.0
MAX_RETRIES_PER_CHUNK = 5

NAK_REASONS = {
    0x01: "BAD_MAGIC",
    0x02: "WRONG_BOARD",
    0x03: "TOO_LARGE",
    0x04: "ERASE_FAILED",
}


def frame_crc(cmd: int, payload: bytes) -> int:
    """Must match Bootloader_u32FrameCrc(): CRC32 over cmd + LE(len) + payload."""
    body = bytes([cmd]) + struct.pack("<H", len(payload)) + payload
    return zlib.crc32(body) & 0xFFFFFFFF


def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    crc = frame_crc(cmd, payload)
    return (bytes([START_BYTE, cmd]) + struct.pack("<H", len(payload)) + payload
            + struct.pack("<I", crc) + bytes([END_BYTE]))


def read_frame(ser: serial.Serial, timeout_s: float):
    """Block until one well-formed, CRC-valid frame arrives or timeout_s
    elapses. Returns (cmd, payload) or None."""
    deadline = time.monotonic() + timeout_s

    def read_byte():
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return None
        ser.timeout = remaining
        b = ser.read(1)
        return b[0] if b else None

    while True:
        b = read_byte()
        if b is None:
            return None
        if b == START_BYTE:
            break

    cmd = read_byte()
    len_lo = read_byte()
    len_hi = read_byte()
    if cmd is None or len_lo is None or len_hi is None:
        return None
    length = len_lo | (len_hi << 8)

    payload = bytearray()
    for _ in range(length):
        b = read_byte()
        if b is None:
            return None
        payload.append(b)

    crc_bytes = bytearray()
    for _ in range(4):
        b = read_byte()
        if b is None:
            return None
        crc_bytes.append(b)

    end = read_byte()
    if end != END_BYTE:
        return None

    received_crc = struct.unpack("<I", bytes(crc_bytes))[0]
    expected_crc = frame_crc(cmd, bytes(payload))
    if received_crc != expected_crc:
        print(f"  [warn] CRC mismatch on received frame cmd=0x{cmd:02X}", file=sys.stderr)
        return None

    return cmd, bytes(payload)


def send_and_wait(ser, cmd, payload, expect_cmds, timeout_s=CHUNK_TIMEOUT_S):
    ser.reset_input_buffer()
    ser.write(build_frame(cmd, payload))
    ser.flush()

    result = read_frame(ser, timeout_s)
    if result is None:
        return None, None

    rcmd, rpayload = result
    if rcmd not in expect_cmds:
        print(f"  [warn] unexpected reply cmd=0x{rcmd:02X}, expected one of "
              f"{[hex(c) for c in expect_cmds]}", file=sys.stderr)
        return None, None

    return rcmd, rpayload


def run(port: str, baud: int, fpkg_path: str) -> int:
    with open(fpkg_path, "rb") as f:
        data = f.read()

    if len(data) < 20:
        print("error: file is smaller than a .fpkg header — did you point this at a raw .bin?", file=sys.stderr)
        return 1

    header = data[:20]
    payload = data[20:]
    magic, header_ver, board_id, _reserved, build_no, payload_len, payload_crc = \
        struct.unpack("<IBBHIII", header)

    if magic != 0x46583256:
        print("error: bad magic — this doesn't look like a .fpkg (run it through fota_packager.py first)",
              file=sys.stderr)
        return 1
    if payload_len != len(payload):
        print(f"error: header says {payload_len} bytes but the file has {len(payload)}", file=sys.stderr)
        return 1

    print(f"Package: build={build_no} board=0x{board_id:02X} header_ver={header_ver} "
          f"len={payload_len} crc32=0x{payload_crc:08X}")

    ser = serial.Serial(port, baud, timeout=1)
    time.sleep(0.2)  # let the port settle after opening

    print("-> HELLO  (reset the board now if it isn't already in its listen window)")
    cmd, resp = send_and_wait(ser, CMD_HELLO, b"", [CMD_HELLO_ACK], timeout_s=2.0)
    if cmd is None:
        print("error: no HELLO_ACK. The bootloader only listens for "
              "BOOTLOADER_RECOVERY_WINDOW_MS (~1s) after every reset — "
              "reset the board and re-run this script within that window.",
              file=sys.stderr)
        return 1
    active_slot, cur_build = struct.unpack("<BI", resp)
    print(f"<- HELLO_ACK: active_slot={'A' if active_slot == 0 else 'B'} build_no={cur_build}")

    print("-> START_UPDATE")
    cmd, resp = send_and_wait(ser, CMD_START_UPDATE, header, [CMD_START_ACK, CMD_START_NAK], timeout_s=5.0)
    if cmd is None:
        print("error: no response to START_UPDATE", file=sys.stderr)
        return 1
    if cmd == CMD_START_NAK:
        reason = resp[0] if resp else 0
        print(f"error: START_NAK ({NAK_REASONS.get(reason, reason)})", file=sys.stderr)
        return 1
    print("<- START_ACK (target slot erased, ready for chunks)")

    seq = 0
    offset = 0
    total_chunks = (len(payload) + CHUNK_SIZE - 1) // CHUNK_SIZE
    t_start = time.monotonic()

    while offset < len(payload):
        chunk = payload[offset:offset + CHUNK_SIZE]
        chunk_payload = struct.pack("<H", seq) + chunk

        for attempt in range(MAX_RETRIES_PER_CHUNK):
            cmd, resp = send_and_wait(ser, CMD_DATA_CHUNK, chunk_payload, [CMD_CHUNK_ACK, CMD_CHUNK_NAK])
            if cmd == CMD_CHUNK_ACK:
                break
            print(f"  [retry {attempt + 1}/{MAX_RETRIES_PER_CHUNK}] chunk {seq}/{total_chunks} "
                  f"({'NAK' if cmd == CMD_CHUNK_NAK else 'timeout'})", file=sys.stderr)
        else:
            print(f"error: chunk {seq} failed after {MAX_RETRIES_PER_CHUNK} attempts", file=sys.stderr)
            return 1

        offset += len(chunk)
        seq += 1
        if seq % 20 == 0 or offset >= len(payload):
            print(f"  chunk {seq}/{total_chunks}  ({offset}/{len(payload)} bytes)")

    elapsed = time.monotonic() - t_start
    print(f"Transfer done in {elapsed:.1f}s ({len(payload) / max(elapsed, 0.001) / 1024:.1f} KB/s)")

    print("-> END_UPDATE")
    cmd, resp = send_and_wait(ser, CMD_END_UPDATE, b"", [CMD_END_ACK, CMD_END_NAK], timeout_s=5.0)
    if cmd != CMD_END_ACK:
        print("error: whole-image CRC check failed on the bootloader side (END_NAK / timeout) "
              "— the target slot was NOT committed and the old image is still active.", file=sys.stderr)
        return 1
    print("<- END_ACK (whole-image CRC32 verified against the header)")

    print("-> COMMIT")
    cmd, resp = send_and_wait(ser, CMD_COMMIT, b"", [CMD_COMMIT_ACK, CMD_COMMIT_NAK], timeout_s=5.0)
    if cmd != CMD_COMMIT_ACK:
        print("error: COMMIT_NAK / timeout — update NOT installed.", file=sys.stderr)
        return 1

    print("<- COMMIT_ACK — the board is switching to the new slot and resetting now.")
    print("Watch for it to boot the new application (telemetry / LEDs / whatever you flashed).")
    print("If it never confirms itself (FOTA_MarkBootOK), the bootloader will automatically")
    print("roll back to the previous slot on its next reset — no action needed from here.")

    return 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/ttyUSB0 or COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--file", required=True, help="Path to a .fpkg built by fota_packager.py")
    args = parser.parse_args()

    sys.exit(run(args.port, args.baud, args.file))


if __name__ == "__main__":
    main()
