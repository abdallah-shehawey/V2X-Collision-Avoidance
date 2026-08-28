
"""fota_packager.py — wrap a raw STM32 application .bin into a .fpkg.

The .fpkg format (see ../../Inc/Application/FOTA/FOTA_Protocol_interface.h,
FOTA_PackageHeader_t — this script's header struct MUST match that one
field-for-field):

    Offset  Field           Type
    0       magic           uint32 LE   "V2XF" -> 0x46583256
    4       header_ver      uint8       1
    5       board_id        uint8       see BOARD_ID_* below
    6       _reserved0      uint16 LE   0
    8       build_no        uint32 LE
    12      payload_len     uint32 LE
    16      payload_crc32   uint32 LE   zlib.crc32 of the payload
    20..    payload         the raw application .bin, unchanged

Usage:
    fota_packager.py <app.bin> <build_no> <board_id> <out.fpkg>

Example (Nucleo bench board, build 42):
    python3 fota_packager.py app_slotA.bin 42 1 firmware_v42.fpkg
"""
import struct
import sys
import zlib

MAGIC = 0x46583256  # "V2XF"
HEADER_VERSION = 1
HEADER_FORMAT = "<IBBHIII"  # magic, header_ver, board_id, _reserved0, build_no, payload_len, payload_crc32
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# Must match FOTA_BOARD_ID_* in FOTA_Protocol_interface.h.
BOARD_ID_NUCLEO = 0x01
BOARD_ID_CARRIER_PCB = 0x02


def pack(bin_path: str, build_no: int, board_id: int, out_path: str) -> None:
    with open(bin_path, "rb") as f:
        payload = f.read()

    crc = zlib.crc32(payload) & 0xFFFFFFFF

    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        HEADER_VERSION,
        board_id,
        0,  # _reserved0
        build_no,
        len(payload),
        crc,
    )
    assert len(header) == HEADER_SIZE, f"header size drifted: {len(header)} != {HEADER_SIZE}"

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(payload)

    print(f"{out_path}: build {build_no}, board 0x{board_id:02X}, "
          f"{len(payload)} bytes, crc32=0x{crc:08X}")


def main() -> None:
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(1)

    bin_path, build_no, board_id, out_path = sys.argv[1:5]
    pack(bin_path, int(build_no), int(board_id, 0), out_path)


if __name__ == "__main__":
    main()
