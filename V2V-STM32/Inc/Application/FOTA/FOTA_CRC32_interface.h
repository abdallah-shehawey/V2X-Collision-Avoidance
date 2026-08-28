/**
 ******************************************************************************
 * @file    FOTA_CRC32_interface.h
 * @brief   Software CRC32 — the integrity check used everywhere in FOTA
 *          (per-chunk transport, whole-image verification, the metadata
 *          record itself).
 * @ingroup app_fota
 *
 * @details
 * Table-based, standard reflected polynomial (0xEDB88320) — **bit-for-bit
 * identical to Python's `zlib.crc32`**. That compatibility is the entire
 * reason this exists as software rather than using the STM32's own hardware
 * CRC peripheral: the hardware unit uses a fixed, non-reflected polynomial
 * convention that does not match `zlib.crc32` without extra bit-reversal
 * gymnastics on both sides. A software CRC32 over a ~200 KB image costs low
 * tens of milliseconds at 16 MHz — irrelevant next to the multi-second UART
 * transfer it is checking — and removes an entire class of "why do the two
 * sides disagree" bugs between this firmware and the Raspberry Pi / packaging
 * tooling. See `V2V-STM32/docs/FOTA.md` §6 for the full reasoning.
 *
 * Used by both the bootloader (`V2V-STM32/Bootloader/`) and the application
 * (through @ref app_fota's metadata module) — it has no hardware dependency
 * at all, which is what makes it safe to share between the two.
 ******************************************************************************
 */

#ifndef FOTA_CRC32_INTERFACE_H_
#define FOTA_CRC32_INTERFACE_H_

#include <stdint.h>

/**
 * @addtogroup app_fota
 * @{
 */

/**
 * @brief CRC32 of one buffer, in one call.
 *
 * Equivalent to Python's `zlib.crc32(data)` — this is the function every
 * `.fpkg` payload and metadata record is checked against, on both ends of
 * the link.
 *
 * @param[in] Copy_pu8Data First byte of the buffer.
 * @param[in] Copy_u32Len  Number of bytes.
 * @return The CRC32 of the buffer, seeded and finalized internally (callers
 *         never need @ref FOTA_CRC32_u32Init / @ref FOTA_CRC32_u32Update /
 *         @ref FOTA_CRC32_u32Finalize directly unless streaming — see below).
 */
uint32_t FOTA_CRC32_u32Compute(const uint8_t *Copy_pu8Data, uint32_t Copy_u32Len);

/**
 * @name Streaming API
 * For computing a CRC32 across data that never exists in memory all at once
 * — e.g. the bootloader folding in one 256-byte chunk at a time as it writes
 * each one to flash, rather than buffering the whole image just to CRC it.
 * @{
 */

/** @brief The initial CRC32 state (equivalent to `zlib.crc32(b"")`). Start every stream with this. */
uint32_t FOTA_CRC32_u32Init(void);

/**
 * @brief Fold one more buffer into a running CRC32.
 * @param[in] Copy_u32Crc  The running value — the seed on the first call, or
 *                         whatever the previous call to this function returned.
 * @param[in] Copy_pu8Data Next buffer to fold in.
 * @param[in] Copy_u32Len  Number of bytes in @p Copy_pu8Data.
 * @return The updated running value. Not yet the final CRC32 — see
 *         @ref FOTA_CRC32_u32Finalize.
 */
uint32_t FOTA_CRC32_u32Update(uint32_t Copy_u32Crc, const uint8_t *Copy_pu8Data, uint32_t Copy_u32Len);

/**
 * @brief Turn a running value from @ref FOTA_CRC32_u32Update into the final CRC32.
 * @param[in] Copy_u32Crc The running value after the last @ref FOTA_CRC32_u32Update call.
 * @return The same result @ref FOTA_CRC32_u32Compute would give for the whole
 *         stream in one call.
 */
uint32_t FOTA_CRC32_u32Finalize(uint32_t Copy_u32Crc);
/** @} */

/** @} */ /* end of app_fota */

#endif /* FOTA_CRC32_INTERFACE_H_ */
