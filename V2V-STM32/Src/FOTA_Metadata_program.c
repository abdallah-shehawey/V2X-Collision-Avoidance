/**
 ******************************************************************************
 * @file    FOTA_Metadata_program.c
 * @brief   Implementation of the FOTA metadata append log and boot-state helpers.
 * @ingroup app_fota
 ******************************************************************************
 */

#include <stdint.h>
#include <string.h>

#include "../Inc/Drivers/LIB/ErrTypes.h"
#include "../Inc/Drivers/MCAL/FLASH/FLASH_interface.h"
#include "../Inc/Drivers/MCAL/SCB/SCB_interface.h"
#include "../Inc/Application/FOTA/FOTA_CRC32_interface.h"
#include "../Inc/Application/FOTA/FOTA_Metadata_interface.h"

/** @brief The current metadata state, loaded once by @ref FOTA_Metadata_voidInit and updated by every @ref FOTA_Metadata_enumSave. */
static FOTA_Metadata_t FOTA_Metadata_stCache;

/** @brief Byte offset within the metadata sector where the next record will be written. */
static uint32_t FOTA_Metadata_u32NextFreeOffset;

/** @brief Per-boot guard so @ref FOTA_MarkBootOK only ever writes flash at most once. */
static uint8_t FOTA_Metadata_u8BootConfirmed = 0U;

/**
 * @brief CRC32 that @ref FOTA_Metadata_t::record_crc32 should hold for @p Copy_pstMeta.
 *
 * Covers every byte of the struct EXCEPT `record_crc32` itself, which is the
 * last member — this is why the layout puts it last.
 */
static uint32_t FOTA_Metadata_u32ComputeRecordCrc(const FOTA_Metadata_t *Copy_pstMeta)
{
  return FOTA_CRC32_u32Compute((const uint8_t *)Copy_pstMeta,
                                (uint32_t)(sizeof(FOTA_Metadata_t) - sizeof(uint32_t)));
}

/** @brief A record is valid if its magic and its own CRC both check out. Anything else (including a fully-erased 0xFF slot) is not. */
static uint8_t FOTA_Metadata_u8IsRecordValid(const FOTA_Metadata_t *Copy_pstMeta)
{
  uint8_t local_u8Valid = 0U;

  if (Copy_pstMeta->magic == FOTA_META_MAGIC)
  {
    if (Copy_pstMeta->record_crc32 == FOTA_Metadata_u32ComputeRecordCrc(Copy_pstMeta))
    {
      local_u8Valid = 1U;
    }
  }

  return local_u8Valid;
}

/**
 * @brief Fill @p Copy_pstMeta with the safe "nothing has ever been recorded" state.
 * @note Trusts Slot A as valid: on a board that has never gone through this
 *       scheme, whatever is physically in Slot A got there over SWD, which
 *       is outside FOTA entirely — there is no prior record to say
 *       otherwise, and refusing to boot it would brick every board on its
 *       very first power-on. Slot B starts unvalidated.
 */
static void FOTA_Metadata_voidSetDefaults(FOTA_Metadata_t *Copy_pstMeta)
{
  memset(Copy_pstMeta, 0, sizeof(FOTA_Metadata_t));
  Copy_pstMeta->active_slot = FOTA_SLOT_A;
  Copy_pstMeta->slot[FOTA_SLOT_A].valid = 1U;
}

void FOTA_Metadata_voidInit(void)
{
  uint32_t local_u32Offset;
  uint8_t local_u8FoundAny = 0U;
  FOTA_Metadata_t local_stCandidate;

  FOTA_Metadata_u32NextFreeOffset = 0U;
  FOTA_Metadata_u8BootConfirmed = 0U;

  for (local_u32Offset = 0U; local_u32Offset < FOTA_METADATA_SIZE; local_u32Offset += FOTA_META_RECORD_SIZE)
  {
    memcpy(&local_stCandidate, (const void *)(FOTA_METADATA_BASE + local_u32Offset), sizeof(FOTA_Metadata_t));

    if (!FOTA_Metadata_u8IsRecordValid(&local_stCandidate))
    {
      /* Records are written strictly in order, back to back, from offset 0 —
       * the first slot that is NOT a valid record marks both the end of the
       * log (nothing after this point matters) and where the next Save goes. */
      break;
    }

    /* Valid records are written in increasing seq order, so the last one we
     * see scanning forward is always the newest — no need to compare seq
     * numbers against each other. */
    FOTA_Metadata_stCache = local_stCandidate;
    local_u8FoundAny = 1U;
    FOTA_Metadata_u32NextFreeOffset = local_u32Offset + FOTA_META_RECORD_SIZE;
  }

  if (!local_u8FoundAny)
  {
    FOTA_Metadata_voidSetDefaults(&FOTA_Metadata_stCache);
    FOTA_Metadata_u32NextFreeOffset = 0U;
  }
}

const FOTA_Metadata_t *FOTA_Metadata_pstGet(void)
{
  return &FOTA_Metadata_stCache;
}

ErrorState_t FOTA_Metadata_enumSave(FOTA_Metadata_t *Copy_pstMeta)
{
  ErrorState_t local_u8ErrorState = OK;

  if (Copy_pstMeta == NULL)
  {
    local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    Copy_pstMeta->magic = FOTA_META_MAGIC;
    Copy_pstMeta->seq   = FOTA_Metadata_stCache.seq + 1U;
    Copy_pstMeta->record_crc32 = FOTA_Metadata_u32ComputeRecordCrc(Copy_pstMeta);

    if (FOTA_Metadata_u32NextFreeOffset >= FOTA_METADATA_SIZE)
    {
      /* Sector full. Only the newest record has any value, so compact by
       * erasing and starting the log over at slot 0 — the record being
       * saved right now becomes that fresh slot 0. */
      local_u8ErrorState = FLASH_enumEraseSector(FOTA_METADATA_SECTOR);
      FOTA_Metadata_u32NextFreeOffset = 0U;
    }

    if (local_u8ErrorState == OK)
    {
      local_u8ErrorState = FLASH_enumProgramBuffer(FOTA_METADATA_BASE + FOTA_Metadata_u32NextFreeOffset,
                                                    (const uint8_t *)Copy_pstMeta,
                                                    (uint32_t)sizeof(FOTA_Metadata_t));
    }

    if (local_u8ErrorState == OK)
    {
      FOTA_Metadata_stCache = *Copy_pstMeta;
      FOTA_Metadata_u32NextFreeOffset += FOTA_META_RECORD_SIZE;
    }
  }

  return local_u8ErrorState;
}

void FOTA_MarkBootOK(void)
{
  if (!FOTA_Metadata_u8BootConfirmed)
  {
    /* Set the guard FIRST: if the save below fails, do not retry-storm a
     * flash write from whatever periodic context is calling this. It will
     * simply try again from scratch on the next power-on. */
    FOTA_Metadata_u8BootConfirmed = 1U;

    if (FOTA_Metadata_stCache.boot_pending)
    {
      FOTA_Metadata_t local_stMeta = FOTA_Metadata_stCache;
      local_stMeta.boot_pending  = 0U;
      local_stMeta.boot_attempts = 0U;
      (void)FOTA_Metadata_enumSave(&local_stMeta);
    }
  }
}

void FOTA_RequestUpdate(void)
{
  FOTA_Metadata_t local_stMeta = FOTA_Metadata_stCache;
  local_stMeta.update_requested = 1U;
  (void)FOTA_Metadata_enumSave(&local_stMeta);

  SCB_vSystemReset(); /* never returns */
}
