/**
 ******************************************************************************
 * @file    FOTA_Protocol_interface.h
 * @brief   The FOTA wire format: frame layout, opcodes and the `.fpkg`
 *          package header — the contract shared between the STM32 bootloader
 *          and whatever is on the other end of the UART (today: the PC bench
 *          script `Bootloader/tools/fota_bench_test.py`; later: the
 *          Raspberry Pi `RPI/FOTA/fota_agent.py`).
 * @ingroup app_fota
 *
 * @details
 * Header-only, deliberately: this file declares no functions, only the
 * layout both ends must agree on. It exists so there is exactly one place a
 * field width, an opcode value or the chunk size is spelled out in C — the
 * Python side mirrors these same numbers by hand (there is no C-to-Python
 * codegen here), so if you change a value in this file, update
 * `Bootloader/tools/fota_packager.py` and `fota_bench_test.py` to match, and
 * update the tables in `V2V-STM32/docs/FOTA.md` §4/§5 too.
 *
 * @section fota_frame Frame layout
 *
 * @verbatim
 *   +--------+-----+------------+------------------+-------------+--------+
 *   | START  | CMD |  LEN (LE)  |     PAYLOAD       | CRC32 (LE)  |  END   |
 *   |  0xAA  | 1 B |    2 B     |    LEN bytes      |    4 B      |  0x55  |
 *   +--------+-----+------------+------------------+-------------+--------+
 * @endverbatim
 *
 * Same structural idea as the `Neighbor` frame in `Inc/Application/DSRC/DSRC.h`
 * (start byte / payload / check / end byte, parsed by an explicit state
 * machine that falls back to "waiting for start" on anything unexpected) —
 * but CRC32 over the whole frame instead of a single XOR byte, since a
 * corrupted flash write is a much bigger deal than one dropped telemetry
 * line, and request/response instead of broadcast.
 *
 * `LEN` and the multi-byte fields inside payloads are little-endian, matching
 * this MCU and every other packed struct already in this firmware.
 ******************************************************************************
 */

#ifndef FOTA_PROTOCOL_INTERFACE_H_
#define FOTA_PROTOCOL_INTERFACE_H_

#include <stdint.h>

/**
 * @addtogroup app_fota
 * @{
 */

#define FOTA_PROTO_START_BYTE 0xAAU /**< First byte of every frame. */
#define FOTA_PROTO_END_BYTE   0x55U /**< Last byte of every frame. */

/** @brief Largest payload the frame parser will accept — bounds its receive buffer.
 *  Must comfortably fit the biggest payload actually used: a DATA_CHUNK
 *  (2-byte sequence number + @ref FOTA_PROTO_CHUNK_SIZE bytes). */
#define FOTA_PROTO_MAX_PAYLOAD (2U + 256U)

/** @brief Bytes of application image carried per DATA_CHUNK. */
#define FOTA_PROTO_CHUNK_SIZE 256U

/**
 * @brief Every command this protocol defines.
 *
 * ACK and NAK are always separate opcodes (never "one opcode, ACK-or-NAK
 * inferred from the payload") — keeps the parser's dispatch a plain switch,
 * with no case that has to inspect its own payload just to know what kind of
 * reply it received.
 */
typedef enum
{
  FOTA_CMD_HELLO        = 0x01U, /**< Pi/PC -> STM32: "Are you the bootloader and listening?" No payload. */
  FOTA_CMD_HELLO_ACK    = 0x02U, /**< STM32 -> Pi/PC: yes — payload is @ref FOTA_HelloAck_t. */

  FOTA_CMD_START_UPDATE = 0x10U, /**< Pi/PC -> STM32: announces the incoming image — payload is @ref FOTA_PackageHeader_t. */
  FOTA_CMD_START_ACK    = 0x11U, /**< STM32 -> Pi/PC: header accepted, target slot erased, ready for chunks. No payload. */
  FOTA_CMD_START_NAK    = 0x12U, /**< STM32 -> Pi/PC: header rejected — payload is one @ref FOTA_NakReason_t byte. */

  FOTA_CMD_DATA_CHUNK   = 0x20U, /**< Pi/PC -> STM32: one chunk — payload is @ref FOTA_ChunkHeader_t followed by up to @ref FOTA_PROTO_CHUNK_SIZE data bytes. */
  FOTA_CMD_CHUNK_ACK    = 0x21U, /**< STM32 -> Pi/PC: chunk written — payload is the 2-byte sequence number (LE) that was accepted. */
  FOTA_CMD_CHUNK_NAK    = 0x22U, /**< STM32 -> Pi/PC: chunk rejected (bad CRC, out-of-order) — payload is the 2-byte sequence number the sender should resend. */

  FOTA_CMD_END_UPDATE   = 0x30U, /**< Pi/PC -> STM32: "that was the last chunk" — no payload. */
  FOTA_CMD_END_ACK      = 0x31U, /**< STM32 -> Pi/PC: whole-image CRC32 verified OK. No payload. */
  FOTA_CMD_END_NAK      = 0x32U, /**< STM32 -> Pi/PC: whole-image CRC32 (or length) mismatch — the session is aborted, COMMIT will be refused. No payload. */

  FOTA_CMD_COMMIT       = 0x40U, /**< Pi/PC -> STM32: only valid after END_ACK — "make it permanent." No payload. */
  FOTA_CMD_COMMIT_ACK   = 0x41U, /**< STM32 -> Pi/PC: metadata updated; the STM32 is about to boot the new slot. No payload. */
  FOTA_CMD_COMMIT_NAK   = 0x42U, /**< STM32 -> Pi/PC: COMMIT sent without a preceding END_ACK, or the metadata write itself failed. No payload. */
} FOTA_Cmd_t;

/** @brief Reason byte carried by a @ref FOTA_CMD_START_NAK. */
typedef enum
{
  FOTA_NAK_BAD_MAGIC     = 0x01U, /**< Header magic did not match — this is not a `.fpkg`. */
  FOTA_NAK_WRONG_BOARD   = 0x02U, /**< `board_id` does not match this hardware. */
  FOTA_NAK_TOO_LARGE     = 0x03U, /**< `payload_len` exceeds the inactive slot's capacity. */
  FOTA_NAK_ERASE_FAILED  = 0x04U, /**< The target slot failed to erase (flash hardware fault). */
} FOTA_NakReason_t;

/**
 * @brief The `.fpkg` package header — travels as the @ref FOTA_CMD_START_UPDATE
 *        payload, and is also what a `.fpkg` file's first 20 bytes are on disk.
 *
 * Packed to exactly 20 bytes, no compiler padding, so both ends read the same
 * bytes at the same offsets. Never linked into the application image itself —
 * it rides alongside the raw `.bin` payload only, because the application's
 * own vector table must sit at byte 0 of its flash slot for the bootloader's
 * VTOR relocation to work (see `V2V-STM32/docs/FOTA.md` §4/§7).
 */
typedef struct __attribute__((packed))
{
  uint32_t magic;          /**< @ref FOTA_PKG_MAGIC — rejects a garbled/wrong file fast. */
  uint8_t  header_ver;      /**< Format version of this header (1 today). */
  uint8_t  board_id;        /**< Target hardware — see @ref FOTA_BOARD_ID_NUCLEO / @ref FOTA_BOARD_ID_CARRIER_PCB. */
  uint16_t _reserved0;      /**< Padding, always 0. Keeps the struct 4-byte aligned without relying on compiler defaults. */
  uint32_t build_no;        /**< Monotonic build counter — not semver, so "is this newer?" is one integer comparison. */
  uint32_t payload_len;     /**< Exact byte length of the application image that follows. */
  uint32_t payload_crc32;   /**< CRC32 (@ref FOTA_CRC32_u32Compute) over the payload bytes. */
} FOTA_PackageHeader_t;

/** @brief Magic value identifying a `.fpkg` header — the ASCII bytes "V2XF", read as a little-endian uint32. */
#define FOTA_PKG_MAGIC 0x46583256UL

/** @brief `.fpkg` header format version this firmware understands. */
#define FOTA_PKG_HEADER_VERSION 1U

/**
 * @name Board IDs
 * Rejects an image built for the wrong hardware revision before it ever
 * touches flash — see the PA0/PA1-vs-PA2/PA3 pin-plan note in
 * `V2V-STM32/docs/FOTA.md` §5 for why "which board is this" is not always
 * obvious from the firmware alone.
 * @{
 */
#define FOTA_BOARD_ID_NUCLEO      0x01U /**< NUCLEO-F446RE bench board. */
#define FOTA_BOARD_ID_CARRIER_PCB 0x02U /**< Custom carrier PCB (see `docs/PCB_BUILD_STAGES.md`). */
/** @} */

/** @brief Payload of a @ref FOTA_CMD_HELLO_ACK frame. */
typedef struct __attribute__((packed))
{
  uint8_t  active_slot; /**< 0 = Slot A currently active, 1 = Slot B. The INACTIVE slot is always the update target. */
  uint32_t build_no;    /**< Build number of the currently active slot (0 if that slot has never been validated). */
} FOTA_HelloAck_t;

/** @brief Fixed header at the start of every @ref FOTA_CMD_DATA_CHUNK payload, followed by up to @ref FOTA_PROTO_CHUNK_SIZE data bytes. */
typedef struct __attribute__((packed))
{
  uint16_t seq; /**< 0-based chunk sequence number — chunk N covers payload bytes [N*256, N*256+len). */
} FOTA_ChunkHeader_t;

/** @} */ /* end of app_fota */

#endif /* FOTA_PROTOCOL_INTERFACE_H_ */
