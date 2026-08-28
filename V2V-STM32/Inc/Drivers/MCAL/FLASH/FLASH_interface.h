/**
 ******************************************************************************
 * @file    FLASH_interface.h
 * @brief   Public API of the FLASH driver — unlock/erase/program the on-chip
 *          flash, register-level, no ST HAL.
 * @ingroup mcal_flash
 *
 * @details
 * Written for FOTA: the bootloader uses this to erase the inactive A/B
 * application slot and write the incoming image into it; the application
 * side uses it (indirectly, through @ref app_fota) to persist the small
 * update-metadata record. Nothing else in the firmware needs to touch flash
 * at runtime.
 *
 * Follows the same "typed struct over a fixed base address, magic-key-gated
 * operations" style as the IWDG driver (`IWDG_program.c`) — the flash
 * controller unlock sequence (@ref FLASH_enumUnlock) is exactly that same
 * idea applied to a different peripheral.
 *
 * @warning This driver can overwrite the code that is currently executing if
 *          called with the wrong address. It never runs from the sector it is
 *          told to erase/program in this firmware (the bootloader only ever
 *          touches the *application* slots, never its own 32 KB region), but
 *          that safety property lives in how the caller uses this driver, not
 *          in the driver itself — it does not check "am I about to erase
 *          myself".
 ******************************************************************************
 */

#ifndef FLASH_INTERFACE_H_
#define FLASH_INTERFACE_H_

#include <stdint.h>
#include "../../LIB/ErrTypes.h"

/**
 * @addtogroup mcal_flash
 * @{
 */

/**
 * @brief STM32F446RE flash sector numbers, as written into `FLASH->CR.SNB`.
 *
 * Sizes, for reference (512 KB total, single bank):
 * Sector 0-3 = 16 KB each, Sector 4 = 64 KB, Sector 5-7 = 128 KB each.
 * See `V2V-STM32/docs/FOTA.md` for how these are assigned to the bootloader,
 * the metadata record and the two application slots.
 */
typedef enum
{
  FLASH_SECTOR_0 = 0, /**< 0x08000000 - 0x08003FFF (16 KB). */
  FLASH_SECTOR_1,      /**< 0x08004000 - 0x08007FFF (16 KB). */
  FLASH_SECTOR_2,      /**< 0x08008000 - 0x0800BFFF (16 KB). */
  FLASH_SECTOR_3,      /**< 0x0800C000 - 0x0800FFFF (16 KB). */
  FLASH_SECTOR_4,      /**< 0x08010000 - 0x0801FFFF (64 KB). */
  FLASH_SECTOR_5,      /**< 0x08020000 - 0x0803FFFF (128 KB). */
  FLASH_SECTOR_6,      /**< 0x08040000 - 0x0805FFFF (128 KB). */
  FLASH_SECTOR_7,      /**< 0x08060000 - 0x0807FFFF (128 KB). */
  FLASH_SECTOR_COUNT   /**< Number of sectors on this part; not a sector itself. */
} FLASH_Sector_t;

/*============================================================================*/
/*                                PUBLIC API                                  */
/*============================================================================*/

/**
 * @brief Unlock the flash control register (`FLASH->CR`) for writing.
 *
 * Writes the fixed two-value key sequence the reference manual defines
 * (`0x45670123` then `0xCDEF89AB`). Every erase/program call below does this
 * itself and re-locks on the way out, so a caller normally never needs this
 * directly — it is exposed for the rare case of several operations back to
 * back where re-locking/unlocking every single word would be wasteful.
 *
 * @retval OK  The control register accepts writes now.
 * @retval NOK The unlock sequence did not take (`CR.LOCK` is still set) —
 *             normally means it was already unlocked by someone else, or the
 *             flash controller is in a state this driver does not expect.
 */
ErrorState_t FLASH_enumUnlock(void);

/**
 * @brief Re-lock the flash control register.
 * @retval OK Always — locking is a single bit set, it cannot fail.
 */
ErrorState_t FLASH_enumLock(void);

/**
 * @brief Block until the flash controller is no longer busy.
 * @retval OK            `SR.BSY` cleared before the timeout.
 * @retval TIMEOUT_STATE  It never cleared — a stuck-hardware escape only; a
 *                        legitimate erase/program never gets close to it.
 * @note Also used internally by every other call here; exposed publicly
 *       because a caller doing several unlocked operations back to back
 *       needs to wait on each one itself.
 */
ErrorState_t FLASH_enumWaitBusy(void);

/**
 * @brief Erase one sector (sets its contents to 0xFF).
 *
 * Unlocks, erases, waits for completion, checks the error flags, and
 * re-locks — a single self-contained call.
 *
 * @param[in] Copy_eSector Which sector to erase.
 * @retval OK             The sector was erased.
 * @retval NOK            @p Copy_eSector was out of range.
 * @retval TIMEOUT_STATE  The erase never completed (see @ref FLASH_enumWaitBusy).
 * @retval NULL_POINTER   Not used by this call — reserved for API symmetry with
 *                        the other entry points here that do take a pointer.
 *
 * @warning Erasing takes on the order of hundreds of milliseconds to a few
 *          seconds depending on the sector size — this call blocks for the
 *          whole duration. The bootloader deliberately does **not** run the
 *          IWDG while it does this (see the note on that decision in
 *          `V2V-STM32/docs/FOTA.md`, §12) — do not add a watchdog kick around
 *          this call without re-reading why.
 */
ErrorState_t FLASH_enumEraseSector(FLASH_Sector_t Copy_eSector);

/**
 * @brief Program one 32-bit word. The destination must already read 0xFFFFFFFF
 *        (i.e. its sector must already have been erased) — flash can only
 *        clear bits, never set them, so writing over live data silently
 *        ANDs the new value with whatever was already there.
 *
 * @param[in] Copy_u32Addr Destination address. Must be 4-byte aligned and lie
 *                         inside the flash memory range.
 * @param[in] Copy_u32Data The word to write.
 * @retval OK             The word was programmed and read back correctly.
 * @retval NOK            @p Copy_u32Addr was not 4-byte aligned, or the
 *                        readback did not match what was written.
 * @retval TIMEOUT_STATE  The program operation never completed.
 */
ErrorState_t FLASH_enumProgramWord(uint32_t Copy_u32Addr, uint32_t Copy_u32Data);

/**
 * @brief Program an arbitrary-length buffer, word by word.
 *
 * Writes full 32-bit words for as much of @p Copy_pu8Buf as divides evenly by
 * 4; any 1-3 trailing bytes are written as one final word padded with 0xFF
 * (harmless — the destination is pre-erased, so the pad bits stay 1 and are
 * simply never used by anything that reads back only @p Copy_u32Len bytes).
 *
 * @param[in] Copy_u32Addr Destination address. Must be 4-byte aligned.
 * @param[in] Copy_pu8Buf  Source bytes.
 * @param[in] Copy_u32Len  Number of bytes to write.
 * @retval OK             The whole buffer was programmed.
 * @retval NULL_POINTER   @p Copy_pu8Buf was NULL.
 * @retval NOK            @p Copy_u32Addr was not 4-byte aligned.
 * @retval TIMEOUT_STATE  A word-program operation never completed partway through
 *                        (some bytes before the failure may already be written).
 */
ErrorState_t FLASH_enumProgramBuffer(uint32_t Copy_u32Addr, const uint8_t *Copy_pu8Buf, uint32_t Copy_u32Len);

/** @} */ /* end of mcal_flash */

#endif /* FLASH_INTERFACE_H_ */
