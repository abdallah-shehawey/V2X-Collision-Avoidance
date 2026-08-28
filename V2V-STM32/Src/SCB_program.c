/**
 ******************************************************************************
 * @file    SCB_program.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Implementation of the SCB driver — the system control block.
 * @ingroup mcal_scb
 ******************************************************************************
 */
#include <stdint.h>

#include "../Inc/Drivers/LIB/ErrTypes.h"

#include "../Inc/Drivers/MCAL/SCB/SCB_interface.h"
#include "../Inc/Drivers/MCAL/SCB/SCB_private.h"
#include "../Inc/Drivers/MCAL/SCB/SCB_config.h"

#include "../Inc/Drivers/LIB/STM32F446xx.h"

ErrorState_t SCB_vSetPriorityGrouping(uint8_t Copy_u8PriorityGrouping)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_u8PriorityGrouping > SCB_MAX_PRIORITY_GROUPING)
  {
    local_u8ErrorState = NOK;
  }
  else
  {
    MSCB->AIRCR = AIRCR_VECTKEY | (Copy_u8PriorityGrouping << 8);
  }
  return local_u8ErrorState;
}

void SystemInit(void)
{
    /* Enable FPU (Full access to CP10 and CP11) */
    /* Access Control Register (CPACR) address: 0xE000ED88 */
    volatile uint32_t *Local_p32CPACR = (uint32_t *)0xE000ED88;
    *Local_p32CPACR |= ((3UL << 10*2) | (3UL << 11*2));

    /* Optional: Force NVIC State reset to avoid unpredictable behavior after reset */
    /* Handled by startup usually, but extra safety */
}

ErrorState_t SCB_vSetVectorTable(uint32_t Copy_u32VectorTableAddr)
{
  uint8_t local_u8ErrorState = OK;

  /* VTOR's low 9 bits are reserved (read as 0) on this part — the vector
   * table base must be at least 512-byte aligned. A flash sector boundary
   * (16 KB minimum on this MCU) satisfies this with plenty to spare; this
   * check only exists to catch a genuinely wrong address, e.g. one that
   * points mid-sector. */
  if ((Copy_u32VectorTableAddr & 0x1FFUL) != 0UL)
  {
    local_u8ErrorState = NOK;
  }
  else
  {
    MSCB->VTOR = Copy_u32VectorTableAddr;
  }

  return local_u8ErrorState;
}

void SCB_vSystemReset(void)
{
  /* Preserve the current PRIGROUP field — a reset request must not silently
   * change the interrupt priority-grouping split. */
  uint32_t Local_u32Prigroup = MSCB->AIRCR & SCB_AIRCR_PRIGROUP_MASK;

  MSCB->AIRCR = SCB_AIRCR_RESET_VECTKEY | Local_u32Prigroup | SCB_AIRCR_SYSRESETREQ;

  __asm volatile ("dsb" ::: "memory"); /* ensure the write is visible before we wait */

  for (;;)
  {
    /* The reset takes effect within a few clock cycles; spin until it does. */
  }
}
