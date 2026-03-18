/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    NVIC_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : NVIC                                            **
 **                                                                           **
 **===========================================================================**
 */
#include "ErrTypes.h"

#include "NVIC_interface.h"
#include "NVIC_private.h"
#include "NVIC_config.h"

#include "STM32F446xx.h"
/*=================================================================================================================*/

/**
 * @file NVIC_program.c
 * @brief NVIC driver source file for Cortex-M4 MCU (NUCLEO-F446RE).
 *
 * This file contains functions to enable, disable, set pending, clear pending,
 * get active flags, and set priority of interrupts in the NVIC.
 *
 * Author: Abdallah Abdelmoemen Shehawey
 */

/**
 * @brief Enable an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to enable.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vEnableIRQ(uint8_t Copy_u8IRQNumber)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_u8IRQNumber <= NVIC_FMPI2C1_ER)
    {
    MNVIC->ISER[(((uint32_t)Copy_u8IRQNumber) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)Copy_u8IRQNumber) & 0x1FUL));
    }
    else
    {
    local_u8ErrorState = NOK;
    }
  return local_u8ErrorState;
}

/**
 * @brief Disable an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to disable.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vDisableIRQ(uint8_t Copy_u8IRQNumber)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_u8IRQNumber <= NVIC_FMPI2C1_ER)
    {
    MNVIC->ICER[(((uint32_t)Copy_u8IRQNumber) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)Copy_u8IRQNumber) & 0x1FUL));
    }
    else
    {
    local_u8ErrorState = NOK;
    }
  return local_u8ErrorState;
}

/**
 * @brief Set pending flag for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to set pending flag.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vSetPendingFlag(uint8_t Copy_u8IRQNumber)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_u8IRQNumber <= NVIC_FMPI2C1_ER)
    {
    MNVIC->ISPR[(((uint32_t)Copy_u8IRQNumber) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)Copy_u8IRQNumber) & 0x1FUL));
    }
    else
    {
    local_u8ErrorState = NOK;
    }
  return local_u8ErrorState;
}

/**
 * @brief Clear pending flag for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to clear pending flag.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vClearPendingFlag(uint8_t Copy_u8IRQNumber)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_u8IRQNumber <= NVIC_FMPI2C1_ER)
    {
    MNVIC->ICPR[(((uint32_t)Copy_u8IRQNumber) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)Copy_u8IRQNumber) & 0x1FUL));
    }
    else
    {
    local_u8ErrorState = NOK;
    }
  return local_u8ErrorState;
}

/**
 * @brief Get active flag status for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to check active flag.
 * @param Copy_pu8Flag Pointer to store active flag status (0 or 1).
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number invalid, or NULL_POINTER if pointer is NULL.
 */
ErrorState_t NVIC_vGetActiveFlag(uint8_t Copy_u8IRQNumber, uint8_t *Copy_pu8Flag)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_pu8Flag == NULL)
    {
    local_u8ErrorState = NULL_POINTER;
    }
    else
    {
    if (Copy_u8IRQNumber <= NVIC_FMPI2C1_ER)
        {
      *Copy_pu8Flag = (MNVIC->IABR[(((uint32_t)Copy_u8IRQNumber) >> 5UL)] >> (((uint32_t)Copy_u8IRQNumber) & 0x1FUL)) & 1;
        }
        else
        {
      local_u8ErrorState = NOK;
        }
    }
  return local_u8ErrorState;
}

/**
 * @brief Set priority for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to set priority.
 * @param Copy_u8Priority Priority value (0 to 15).
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number or priority is invalid.
 */
ErrorState_t NVIC_vSetPriority(uint8_t Copy_u8IRQNumber, uint8_t Copy_u8Priority)
{
  uint8_t local_u8ErrorState = OK;
  if (Copy_u8IRQNumber <= NVIC_FMPI2C1_ER && Copy_u8Priority <= 15)
    {
    MNVIC->IPR[Copy_u8IRQNumber] = Copy_u8Priority << 4;
    }
    else
    {
    local_u8ErrorState = NOK;
    }
  return local_u8ErrorState;
}