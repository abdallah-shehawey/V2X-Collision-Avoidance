/**
 ******************************************************************************
 * @file    FOTA_Metadata_interface.h
 * @brief   The flash layout (bootloader / metadata / Slot A / Slot B) and the
 *          update-metadata record: which slot is active, whether the last
 *          boot confirmed itself, and what each slot currently holds.
 * @ingroup app_fota
 *
 * @details
 * This is the one place the A/B partitioning scheme is spelled out in code —
 * see `V2V-STM32/docs/FOTA.md` §3 for the full design rationale. Both the
 * bootloader and the application link this same module: the bootloader owns
 * almost all of it (deciding which slot to boot, writing a new slot during an
 * update, rolling back), while the application only ever calls
 * @ref FOTA_MarkBootOK (confirming a just-installed update stuck) and,
 * later, @ref FOTA_RequestUpdate (asking to reboot into update mode — not
 * yet wired to any command in this firmware, see the note on that function).
 *
 * @section fota_meta_layout Flash layout
 *
 * @verbatim
 *   0x08000000 +-------------------------------+
 *              | Bootloader          (32 KB)   |  Sectors 0-1
 *   0x08008000 +-------------------------------+
 *              | Metadata            (16 KB)   |  Sector 2 — this module
 *   0x0800C000 +-------------------------------+
 *              | Application Slot A  (208 KB)  |  Sectors 3-5
 *   0x08040000 +-------------------------------+
 *              | Application Slot B  (256 KB)  |  Sectors 6-7
 *   0x08080000 +-------------------------------+  (end of 512 KB flash)
 * @endverbatim
 *
 * @section fota_meta_log The metadata sector as an append log
 *
 * A fixed-size (@ref FOTA_META_RECORD_SIZE) record is *appended* to Sector 2
 * on every state change, never overwritten in place — so a power loss
 * mid-write just leaves the previous, still-CRC-valid record as the answer.
 * @ref FOTA_Metadata_voidInit scans from the start of the sector and keeps
 * the last valid record it finds (records are written in increasing @ref
 * FOTA_Metadata_t::seq order, so "last valid" is always "newest"). When the
 * sector fills, the next @ref FOTA_Metadata_enumSave erases it and starts the
 * log over at slot 0 — only the newest record has any value, so nothing is
 * lost by dropping the history.
 ******************************************************************************
 */

#ifndef FOTA_METADATA_INTERFACE_H_
#define FOTA_METADATA_INTERFACE_H_

#include <stdint.h>
#include "../../Drivers/LIB/ErrTypes.h"
#include "../../Drivers/MCAL/FLASH/FLASH_interface.h"

/**
 * @addtogroup app_fota
 * @{
 */

/**
 * @name Flash layout
 * @{
 */
#define FOTA_BOOTLOADER_BASE 0x08000000UL          /**< Sectors 0-1. */
#define FOTA_BOOTLOADER_SIZE (32UL * 1024UL)

#define FOTA_METADATA_BASE   0x08008000UL          /**< Sector 2. */
#define FOTA_METADATA_SIZE   (16UL * 1024UL)
#define FOTA_METADATA_SECTOR FLASH_SECTOR_2

#define FOTA_SLOTA_BASE      0x0800C000UL          /**< Sectors 3-5. */
#define FOTA_SLOTA_SIZE      (208UL * 1024UL)

#define FOTA_SLOTB_BASE      0x08040000UL          /**< Sectors 6-7. */
#define FOTA_SLOTB_SIZE      (256UL * 1024UL)
/** @} */

/** @brief Slot identifiers — also the value of @ref FOTA_Metadata_t::active_slot and the index into its `slot[]` array. */
#define FOTA_SLOT_A 0U
#define FOTA_SLOT_B 1U

/** @brief Magic value identifying a valid metadata record — ASCII "V2XM", read as a little-endian uint32. */
#define FOTA_META_MAGIC 0x4D583256UL

/** @brief Size in bytes of one slot in the append log. Comfortably larger than `sizeof(FOTA_Metadata_t)`, 64-byte aligned. */
#define FOTA_META_RECORD_SIZE 64U

/** @brief How many record slots fit in the metadata sector. */
#define FOTA_META_RECORD_COUNT (FOTA_METADATA_SIZE / FOTA_META_RECORD_SIZE)

/** @brief What is known about one application slot's contents. */
typedef struct __attribute__((packed))
{
  uint32_t build_no;      /**< Monotonic build counter of the image in this slot. */
  uint32_t payload_len;   /**< Its size in bytes. */
  uint32_t payload_crc32; /**< Its CRC32, as verified by the bootloader at the end of the transfer that wrote it. */
  uint8_t  board_id;      /**< Which hardware it was built for — see @ref FOTA_BOARD_ID_NUCLEO. */
  uint8_t  valid;         /**< 1 once the bootloader has CRC-verified this slot's contents; 0 = do not boot this slot. */
  uint8_t  _reserved[2];  /**< Padding, always 0. */
} FOTA_SlotInfo_t;

/**
 * @brief One record of the metadata append log — see @ref fota_meta_log.
 *
 * Packed, 52 bytes of real content inside a 64-byte (@ref
 * FOTA_META_RECORD_SIZE) slot — the trailing bytes of each slot are simply
 * unused padding in flash, left as 0xFF.
 */
typedef struct __attribute__((packed))
{
  uint32_t magic;             /**< @ref FOTA_META_MAGIC. */
  uint32_t seq;                /**< Increases by 1 every @ref FOTA_Metadata_enumSave — newest-wins tiebreak. */
  uint8_t  active_slot;         /**< @ref FOTA_SLOT_A or @ref FOTA_SLOT_B — which one the bootloader jumps to. */
  uint8_t  boot_pending;        /**< 1 = the bootloader jumped to `active_slot` and it has not yet confirmed itself alive. */
  uint8_t  boot_attempts;       /**< Consecutive unconfirmed boots of `active_slot`; @ref FOTA_MAX_BOOT_ATTEMPTS triggers rollback. */
  uint8_t  update_requested;    /**< 1 = the application asked the bootloader to stay resident and listen for a transfer on the next reset. */
  FOTA_SlotInfo_t slot[2];      /**< `slot[FOTA_SLOT_A]`, `slot[FOTA_SLOT_B]`. */
  uint32_t record_crc32;        /**< CRC32 over every byte above (not including this field itself). */
} FOTA_Metadata_t;

/** @brief How many consecutive unconfirmed boots of a slot before the bootloader rolls back to the other one. */
#define FOTA_MAX_BOOT_ATTEMPTS 2U

/*============================================================================*/
/*                                PUBLIC API                                  */
/*============================================================================*/

/**
 * @brief Load the current metadata state from flash into RAM. Call once, early at boot.
 *
 * Scans the metadata sector for the newest valid record. If none is found
 * (a brand-new, never-updated board — the metadata sector is still fully
 * erased) synthesizes safe defaults **in RAM only, without writing flash**:
 * Slot A active, not pending, and trusted as valid (it is whatever was
 * flashed over SWD, which predates this whole scheme). Slot B starts
 * unvalidated. Nothing is written to flash until the first real
 * @ref FOTA_Metadata_enumSave, so a board that never receives an update never
 * wears the metadata sector at all.
 */
void FOTA_Metadata_voidInit(void);

/**
 * @brief Borrow the current in-RAM metadata state.
 * @return Pointer to the live cache — read-only; copy it out, modify the
 *         copy, and pass that to @ref FOTA_Metadata_enumSave to change
 *         anything. The pointer is not valid to keep across a Save (the
 *         cache is overwritten in place).
 */
const FOTA_Metadata_t *FOTA_Metadata_pstGet(void);

/**
 * @brief Persist a new metadata state: append one record to the log.
 *
 * Sets `magic`, bumps `seq` past whatever is currently cached, and
 * recomputes `record_crc32` itself — the caller does not need to (and any
 * value it set in those three fields is overwritten). Compacts the sector
 * (erase + restart the log at slot 0) automatically if it is full.
 *
 * @param[in,out] Copy_pstMeta The new state to persist. `magic`/`seq`/
 *                             `record_crc32` are filled in by this call.
 * @retval OK             Persisted; @ref FOTA_Metadata_pstGet now returns this state.
 * @retval NULL_POINTER   @p Copy_pstMeta was NULL.
 * @retval NOK / TIMEOUT_STATE  The flash erase or program failed — see
 *                             @ref FLASH_enumEraseSector / @ref FLASH_enumProgramBuffer.
 *                             The in-RAM cache is left unchanged on failure.
 */
ErrorState_t FOTA_Metadata_enumSave(FOTA_Metadata_t *Copy_pstMeta);

/**
 * @brief Application-side call: confirm "this boot is good."
 *
 * Call once, a few seconds after the scheduler starts, gated on every
 * monitored task's heartbeat having advanced at least once (see
 * `vTask_Watchdog` in `main.c` — this reuses that same liveness signal as a
 * proxy for "the whole task set is alive and looping", without adding a
 * second responsibility to that task's own code). If the currently active
 * slot has an unconfirmed boot pending, clears it — so the bootloader will
 * not roll back on the next reset.
 *
 * Idempotent and cheap to call repeatedly: the first call that finds nothing
 * pending (the normal case on every boot except one right after a fresh
 * install) does no flash write at all, and every call after the first is a
 * no-op guarded in RAM. Safe to call from a task context that runs
 * periodically rather than exactly once.
 *
 * @note  Deliberately separate from `vTask_Watchdog`'s own IWDG-kicking
 *        logic — that task's job is safety-critical liveness monitoring and
 *        should not grow a second, unrelated responsibility.
 */
void FOTA_MarkBootOK(void);

/**
 * @brief Application-side call: ask to reboot into the bootloader's update mode.
 *
 * Writes `update_requested = 1` to the metadata (so the bootloader stays
 * resident and listens for a transfer on the next boot instead of
 * auto-jumping) and immediately performs a software reset
 * (@ref SCB_vSystemReset) — never returns.
 *
 * @note **Not yet called from anywhere in this firmware.** Wiring it to an
 *       actual "update available" command from the Raspberry Pi is Stage 5
 *       of the FOTA roadmap (`V2V-STM32/docs/FOTA.md` §11) — today, the
 *       bootloader's always-on recovery window (a short wait for `HELLO` on
 *       every reset, regardless of this flag) is what `Bootloader/tools/
 *       fota_bench_test.py` uses to start a session for bench testing.
 */
void FOTA_RequestUpdate(void);

/** @} */ /* end of app_fota */

#endif /* FOTA_METADATA_INTERFACE_H_ */
