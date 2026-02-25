/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SCB_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SCB                                             **
 **                                                                           **
 **===========================================================================**
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
