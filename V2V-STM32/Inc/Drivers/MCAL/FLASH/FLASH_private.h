/**
 ******************************************************************************
 * @file    FLASH_private.h
 * @brief   Register bit positions and internals of the FLASH driver — not a
 *          public header.
 * @ingroup mcal_flash
 ******************************************************************************
 */

#ifndef FLASH_PRIVATE_H_
#define FLASH_PRIVATE_H_

/**
 * @name Unlock key sequence
 * Fixed values from the reference manual (RM0390). Writing anything else to
 * `KEYR`, or writing these two out of order, leaves `CR.LOCK` set.
 * @{
 */
#define FLASH_KEY1 0x45670123UL
#define FLASH_KEY2 0xCDEF89ABUL
/** @} */

/**
 * @name `CR` (control register) bit positions
 * @{
 */
#define FLASH_CR_PG    0U  /**< Program: next word write to flash triggers a program operation. */
#define FLASH_CR_SER   1U  /**< Sector erase: next `STRT` triggers erasing the sector in `SNB`. */
#define FLASH_CR_MER   2U  /**< Mass erase — never set by this driver; erasing the whole chip is not a thing FOTA does. */
#define FLASH_CR_SNB   3U  /**< Sector number field start bit; 4 bits wide (`SNB[3:0]`, bits 3-6). */
#define FLASH_CR_PSIZE 8U  /**< Program size field start bit; 2 bits wide (`PSIZE[1:0]`, bits 8-9). */
#define FLASH_CR_STRT  16U /**< Start: writing 1 here begins the erase queued by `SER`+`SNB`. */
#define FLASH_CR_LOCK  31U /**< Lock: set = `KEYR`/erase/program writes are ignored. Cleared by the key sequence. */
/** @} */

/**
 * @brief `PSIZE` value for 32-bit (word) programming.
 * @note  Valid at VDD 2.7-3.6 V, which is what this board runs at (no
 *        external VPP, no low-voltage concerns) — see RM0390 §3.4.
 */
#define FLASH_PSIZE_X32 0x2U

/**
 * @name `SR` (status register) bit positions
 * @{
 */
#define FLASH_SR_EOP    0U  /**< End of operation: an erase/program finished (cleared by writing 1 back). */
#define FLASH_SR_OPERR  1U  /**< Operation error: erase/program requested on a protected/invalid target. */
#define FLASH_SR_WRPERR 4U  /**< Write-protection error. */
#define FLASH_SR_PGAERR 5U  /**< Programming alignment error — address was not aligned to `PSIZE`. */
#define FLASH_SR_PGPERR 6U  /**< Programming parallelism error — `PSIZE` did not match a previous op still pending. */
#define FLASH_SR_PGSERR 7U  /**< Programming sequence error — `PG` was not set (or `SER`/`MER` was) before writing. */
#define FLASH_SR_BSY    16U /**< Busy: an erase or program operation is in progress. */

/** @brief Mask of every error bit in `SR`, so a caller can check them in one AND. */
#define FLASH_SR_ERROR_MASK ((1U << FLASH_SR_OPERR) | (1U << FLASH_SR_WRPERR) | \
                             (1U << FLASH_SR_PGAERR) | (1U << FLASH_SR_PGPERR) | \
                             (1U << FLASH_SR_PGSERR))
/** @} */

/**
 * @brief Busy-wait bound for `SR.BSY`, in loop iterations.
 *
 * A **stuck-hardware** escape only, exactly like `USART_u32TIMEOUT`
 * (`USART_private.h`) — sized generously above the worst-case erase time of
 * the largest sector (128 KB) at the slowest end of the flash's specified
 * erase timing, not tuned to the millisecond.
 */
#define FLASH_u32BUSY_TIMEOUT 20000000UL

#endif /* FLASH_PRIVATE_H_ */
