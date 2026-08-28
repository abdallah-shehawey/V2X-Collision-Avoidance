/**
 ******************************************************************************
 * @file    FLASH_program.c
 * @brief   Implementation of the FLASH driver — unlock/erase/program the
 *          on-chip flash, register-level, no ST HAL.
 * @ingroup mcal_flash
 ******************************************************************************
 */

#include <stdint.h>

#include "../Inc/Drivers/LIB/ErrTypes.h"
#include "../Inc/Drivers/LIB/STM32F446xx.h"

#include "../Inc/Drivers/MCAL/FLASH/FLASH_interface.h"
#include "../Inc/Drivers/MCAL/FLASH/FLASH_private.h"
#include "../Inc/Drivers/MCAL/FLASH/FLASH_config.h"

/**
 * @brief Clear every error flag currently set in `SR`.
 *
 * `SR`'s error bits are write-1-to-clear, so ORing the mask back in clears
 * exactly the ones that are set and leaves everything else untouched.
 */
static void FLASH_ClearErrorFlags(void)
{
  MFLASH->SR |= FLASH_SR_ERROR_MASK;
}

ErrorState_t FLASH_enumWaitBusy(void)
{
  ErrorState_t local_u8ErrorState = OK;
  uint32_t local_u32TimeoutCounter = 0;

  while (((MFLASH->SR & (1U << FLASH_SR_BSY)) != 0U) && (local_u32TimeoutCounter < FLASH_u32BUSY_TIMEOUT))
  {
    local_u32TimeoutCounter++;
  }

  if (local_u32TimeoutCounter >= FLASH_u32BUSY_TIMEOUT)
  {
    local_u8ErrorState = TIMEOUT_STATE;
  }

  return local_u8ErrorState;
}

ErrorState_t FLASH_enumUnlock(void)
{
  ErrorState_t local_u8ErrorState = OK;

  if ((MFLASH->CR & (1U << FLASH_CR_LOCK)) != 0U)
  {
    MFLASH->KEYR = FLASH_KEY1;
    MFLASH->KEYR = FLASH_KEY2;
  }

  if ((MFLASH->CR & (1U << FLASH_CR_LOCK)) != 0U)
  {
    /* Still locked after the key sequence — wrong keys, or the controller is
     * in a state (e.g. an option-byte operation) this driver does not expect. */
    local_u8ErrorState = NOK;
  }

  return local_u8ErrorState;
}

ErrorState_t FLASH_enumLock(void)
{
  MFLASH->CR |= (1U << FLASH_CR_LOCK);
  return OK;
}

ErrorState_t FLASH_enumEraseSector(FLASH_Sector_t Copy_eSector)
{
  ErrorState_t local_u8ErrorState = OK;

  if (Copy_eSector >= FLASH_SECTOR_COUNT)
  {
    local_u8ErrorState = NOK;
  }
  else
  {
    local_u8ErrorState = FLASH_enumWaitBusy();

    if (local_u8ErrorState == OK)
    {
      local_u8ErrorState = FLASH_enumUnlock();
    }

    if (local_u8ErrorState == OK)
    {
      FLASH_ClearErrorFlags();

      /* Queue sector number and SER first; STRT must be written last — it is
       * what actually kicks the erase off. */
      MFLASH->CR &= ~(0xFU << FLASH_CR_SNB);
      MFLASH->CR |= ((uint32_t)Copy_eSector << FLASH_CR_SNB);
      MFLASH->CR |= (1U << FLASH_CR_SER);
      MFLASH->CR |= (1U << FLASH_CR_STRT);

      local_u8ErrorState = FLASH_enumWaitBusy();

      if (local_u8ErrorState == OK && (MFLASH->SR & FLASH_SR_ERROR_MASK) != 0U)
      {
        local_u8ErrorState = NOK;
      }

      /* Clear SER/SNB regardless of the outcome — leaving SER set would arm
       * the NEXT STRT (from anything) to erase whatever sector happens to be
       * sitting in SNB. */
      MFLASH->CR &= ~(1U << FLASH_CR_SER);
      MFLASH->CR &= ~(0xFU << FLASH_CR_SNB);

      FLASH_enumLock();
    }
  }

  return local_u8ErrorState;
}

ErrorState_t FLASH_enumProgramWord(uint32_t Copy_u32Addr, uint32_t Copy_u32Data)
{
  ErrorState_t local_u8ErrorState = OK;

  if ((Copy_u32Addr & 0x3U) != 0U)
  {
    /* Not 4-byte aligned — x32 programming requires it. */
    local_u8ErrorState = NOK;
  }
  else
  {
    local_u8ErrorState = FLASH_enumWaitBusy();

    if (local_u8ErrorState == OK)
    {
      local_u8ErrorState = FLASH_enumUnlock();
    }

    if (local_u8ErrorState == OK)
    {
      FLASH_ClearErrorFlags();

      MFLASH->CR &= ~(0x3U << FLASH_CR_PSIZE);
      MFLASH->CR |= (FLASH_PSIZE_X32 << FLASH_CR_PSIZE);
      MFLASH->CR |= (1U << FLASH_CR_PG);

      *(volatile uint32_t *)Copy_u32Addr = Copy_u32Data;

      local_u8ErrorState = FLASH_enumWaitBusy();

      if (local_u8ErrorState == OK && (MFLASH->SR & FLASH_SR_ERROR_MASK) != 0U)
      {
        local_u8ErrorState = NOK;
      }

      MFLASH->CR &= ~(1U << FLASH_CR_PG);

      FLASH_enumLock();

      /* Read back what actually landed. A pre-erased destination plus a
       * successful program should read back exactly what was written; any
       * mismatch (partially-erased target, a byte the controller silently
       * dropped) is caught here rather than trusted from SR alone. */
      if (local_u8ErrorState == OK && (*(volatile uint32_t *)Copy_u32Addr != Copy_u32Data))
      {
        local_u8ErrorState = NOK;
      }
    }
  }

  return local_u8ErrorState;
}

ErrorState_t FLASH_enumProgramBuffer(uint32_t Copy_u32Addr, const uint8_t *Copy_pu8Buf, uint32_t Copy_u32Len)
{
  ErrorState_t local_u8ErrorState = OK;
  uint32_t local_u32Index = 0;

  if (Copy_pu8Buf == NULL)
  {
    local_u8ErrorState = NULL_POINTER;
  }
  else if ((Copy_u32Addr & 0x3U) != 0U)
  {
    local_u8ErrorState = NOK;
  }
  else
  {
    /* Full 32-bit words first, little-endian (matches this MCU and every
     * other packed wire struct already in this firmware, e.g. Neighbor). */
    while (((local_u32Index + 4U) <= Copy_u32Len) && (local_u8ErrorState == OK))
    {
      uint32_t local_u32Word = ((uint32_t)Copy_pu8Buf[local_u32Index]) |
                                ((uint32_t)Copy_pu8Buf[local_u32Index + 1] << 8) |
                                ((uint32_t)Copy_pu8Buf[local_u32Index + 2] << 16) |
                                ((uint32_t)Copy_pu8Buf[local_u32Index + 3] << 24);

      local_u8ErrorState = FLASH_enumProgramWord(Copy_u32Addr + local_u32Index, local_u32Word);
      local_u32Index += 4U;
    }

    /* 1-3 trailing bytes: pad the rest of the final word with 0xFF (the
     * pre-erased fill value). Harmless — a reader that only reads back the
     * real Copy_u32Len bytes never sees the padding. */
    if (local_u8ErrorState == OK && local_u32Index < Copy_u32Len)
    {
      uint32_t local_u32Tail = 0xFFFFFFFFUL;
      uint32_t local_u32Remaining = Copy_u32Len - local_u32Index;
      uint8_t *local_pu8TailBytes = (uint8_t *)&local_u32Tail;
      uint32_t i;

      for (i = 0; i < local_u32Remaining; i++)
      {
        local_pu8TailBytes[i] = Copy_pu8Buf[local_u32Index + i];
      }

      local_u8ErrorState = FLASH_enumProgramWord(Copy_u32Addr + local_u32Index, local_u32Tail);
    }
  }

  return local_u8ErrorState;
}
