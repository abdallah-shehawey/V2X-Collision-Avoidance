/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    EXTI_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : EXTI                                            **
 **                                                                           **
 **===========================================================================**
 */

#include <stdint.h>

#include "STM32F446xx.h"
#include "ErrTypes.h"
#include "STD_MACROS.h"

#include "EXTI_config.h"
#include "EXTI_interface.h"
#include "EXTI_private.h"

/**
 * @EXTI_CallBack array:
 * @brief: Array to store callback functions for each EXTI line
 * @details: Static array of function pointers to store callback functions
 *           for EXTI lines 0-15
 * @param: None
 * @return: None
 */
static void (*EXTI_CallBack[16])(void) = {NULL};

/*=================================================================================================================*/
/**
 * @EXTI_vLineInit function:
 * @brief: Initialize EXTI line with configuration
 * @details: Configures EXTI line based on provided configuration structure
 *           - Validates configuration parameters
 *           - Sets trigger source
 *           - Configures interrupt enable/disable state
 *           - Stores callback function
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 *          OK - Operation successful
 *          NULL_POINTER - Invalid configuration pointer or callback
 *          NOK - Invalid EXTI line number
 * @note: Must be called after SYSCFG configuration
 */
ErrorState_t EXTI_vLineInit(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig)
{
  ErrorState_t Local_u8ErrorState = OK;
  if (Copy_pEXTI_LineConfig == NULL && Copy_pEXTI_LineConfig->Copy_pvCallBack == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else if (Copy_pEXTI_LineConfig->Line > EXTI_LINE15)
  {
    Local_u8ErrorState = NOK;
  }
  else
  {
    switch (Copy_pEXTI_LineConfig->TrigSrc)
    {
    case EXTI_RISING_EDGE:
      MEXTI->RTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_FALLING_EDGE:
      MEXTI->FTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->RTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_RISING_FALLING_EDGE:
      MEXTI->RTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_NO_TRIGGER:
      MEXTI->RTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      break;
    }

    switch (Copy_pEXTI_LineConfig->Enable)
    {
    case EXTI_EN:
      MEXTI->IMR |= (1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_DIS:
      MEXTI->IMR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      break;
    }

    EXTI_CallBack[Copy_pEXTI_LineConfig->Line] = Copy_pEXTI_LineConfig->Copy_pvCallBack;
  }
  return Local_u8ErrorState;
}
/*=================================================================================================================*/
/**
 * @EXTI_vEnableInterrupt function:
 * @brief: Enable EXTI interrupt
 * @details: Enables the specified EXTI line interrupt by setting IMR bit
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 *          OK - Operation successful
 *          NULL_POINTER - Invalid configuration pointer
 */
ErrorState_t EXTI_vEnableInterrupt(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig)
{
  ErrorState_t Local_u8ErrorState = OK;
  if (Copy_pEXTI_LineConfig == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    MEXTI->IMR |= (1 << Copy_pEXTI_LineConfig->Line);
  }
  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @EXTI_vSetTrigSrc function:
 * @brief: Set EXTI trigger source
 * @details: Configures how the EXTI line is triggered by setting/clearing RTSR and FTSR bits
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 *         Copy_u8Trigger - New trigger source configuration
 * @return: ErrorState_t - Error state of the operation
 *          OK - Operation successful
 *          NULL_POINTER - Invalid configuration pointer
 *          OUT_OF_RANGE - Invalid trigger source
 */
ErrorState_t EXTI_vSetTrigSrc(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig, EXTI_TriggerSrc_t Copy_u8Trigger)
{
  ErrorState_t Local_u8ErrorState = OK;
  if (Copy_pEXTI_LineConfig == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    switch (Copy_u8Trigger)
    {
    case EXTI_NO_TRIGGER:
      MEXTI->RTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_RISING_EDGE:
      MEXTI->RTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_FALLING_EDGE:
      MEXTI->RTSR &= ~(1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      break;
    case EXTI_RISING_FALLING_EDGE:
      MEXTI->RTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      MEXTI->FTSR |= (1 << Copy_pEXTI_LineConfig->Line);
      break;
    default:
      break;
    }
    return Local_u8ErrorState;
  }
}

/*=================================================================================================================*/
/**
 * @EXTI_vDisableInterrupt function:
 * @brief: Disable EXTI interrupt
 * @details: Disables the specified EXTI line interrupt by clearing IMR bit
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 *          OK - Operation successful
 *          NULL_POINTER - Invalid configuration pointer
 */
ErrorState_t EXTI_vDisableInterrupt(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig)
{
  ErrorState_t Local_u8ErrorState = OK;
  if (Copy_pEXTI_LineConfig == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    MEXTI->IMR &= ~(1 << Copy_pEXTI_LineConfig->Line);
  }
  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @EXTI_vSwIntEvent function:
 * @brief: Generate software interrupt event
 * @details: Generates a software interrupt by writing to SWIER register
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 *          OK - Operation successful
 *          NULL_POINTER - Invalid configuration pointer
 */
ErrorState_t EXTI_vSwIntEvent(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig)
{
  ErrorState_t Local_u8ErrorState = OK;
  if (Copy_pEXTI_LineConfig == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    MEXTI->SWIER |= (1 << Copy_pEXTI_LineConfig->Line);
  }
  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @EXTI_vClearPendFlag function:
 * @brief: Clear EXTI pending flag
 * @details: Clears the pending flag for the specified EXTI line by writing to PR register
 * @param: Copy_u8Line - EXTI line number
 * @return: void
 */
void EXTI_vClearPendFlag(EXTI_Line_t Copy_u8Line)
{
  MEXTI->PR |= (1 << Copy_u8Line);
}

/*=================================================================================================================*/
/**
 * @EXTI_u8ReadPendFlag function:
 * @brief: Read EXTI pending flag status
 * @details: Reads the status of the pending flag for specified EXTI line from PR register
 * @param: Copy_u8Line - EXTI line number
 * @return: uint8_t - Status of the pending flag (0: not pending, 1: pending)
 */
uint8_t EXTI_u8ReadPendFlag(EXTI_Line_t Copy_u8Line)
{
  return ((MEXTI->PR & (1 << Copy_u8Line)) >> Copy_u8Line);
}

/*=================================================================================================================*/

/* ISR --> Implementation */

void EXTI0_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE0] != NULL)
  {
    EXTI_CallBack[EXTI_LINE0]();
    EXTI_vClearPendFlag(EXTI_LINE0);
  }
}

/*___________________________________________________________________________________________________________________*/

void EXTI1_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE1] != NULL)
  {
    EXTI_CallBack[EXTI_LINE1]();
    EXTI_vClearPendFlag(EXTI_LINE1);
  }
}

/*___________________________________________________________________________________________________________________*/

void EXTI2_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE2] != NULL)
  {
    EXTI_CallBack[EXTI_LINE2]();
    EXTI_vClearPendFlag(EXTI_LINE2);
  }
}

/*___________________________________________________________________________________________________________________*/

void EXTI3_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE3] != NULL)
  {
    EXTI_CallBack[EXTI_LINE3]();
    EXTI_vClearPendFlag(EXTI_LINE3);
  }
}

/*___________________________________________________________________________________________________________________*/

void EXTI4_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE4] != NULL)
  {
    EXTI_CallBack[EXTI_LINE4]();
    EXTI_vClearPendFlag(EXTI_LINE4);
  }
}

/*___________________________________________________________________________________________________________________*/

void EXTI9_5_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE5] != NULL || EXTI_CallBack[EXTI_LINE6] != NULL || EXTI_CallBack[EXTI_LINE7] != NULL || EXTI_CallBack[EXTI_LINE8] != NULL || EXTI_CallBack[EXTI_LINE9] != NULL)
  {
    if (EXTI_u8ReadPendFlag(EXTI_LINE5))
    {
      EXTI_CallBack[EXTI_LINE5]();
      EXTI_vClearPendFlag(EXTI_LINE5);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE6))
    {
      EXTI_CallBack[EXTI_LINE6]();
      EXTI_vClearPendFlag(EXTI_LINE6);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE7))
    {
      EXTI_CallBack[EXTI_LINE7]();
      EXTI_vClearPendFlag(EXTI_LINE7);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE8))
    {
      EXTI_CallBack[EXTI_LINE8]();
      EXTI_vClearPendFlag(EXTI_LINE8);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE9))
    {
      EXTI_CallBack[EXTI_LINE9]();
      EXTI_vClearPendFlag(EXTI_LINE9);
    }
  }
}

/*___________________________________________________________________________________________________________________*/

void EXTI15_10_IRQHandler(void)
{
  if (EXTI_CallBack[EXTI_LINE10] != NULL || EXTI_CallBack[EXTI_LINE11] != NULL || EXTI_CallBack[EXTI_LINE12] != NULL || EXTI_CallBack[EXTI_LINE13] != NULL || EXTI_CallBack[EXTI_LINE14] != NULL || EXTI_CallBack[EXTI_LINE15] != NULL)
  {
    if (EXTI_u8ReadPendFlag(EXTI_LINE10))
    {
      EXTI_CallBack[EXTI_LINE10]();
      EXTI_vClearPendFlag(EXTI_LINE10);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE11))
    {
      EXTI_CallBack[EXTI_LINE11]();
      EXTI_vClearPendFlag(EXTI_LINE11);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE12))
    {
      EXTI_CallBack[EXTI_LINE12]();
      EXTI_vClearPendFlag(EXTI_LINE12);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE13))
    {
      EXTI_CallBack[EXTI_LINE13]();
      EXTI_vClearPendFlag(EXTI_LINE13);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE14))
    {
      EXTI_CallBack[EXTI_LINE14]();
      EXTI_vClearPendFlag(EXTI_LINE14);
    }
    else if (EXTI_u8ReadPendFlag(EXTI_LINE15))
    {
      EXTI_CallBack[EXTI_LINE15]();
      EXTI_vClearPendFlag(EXTI_LINE15);
    }
  }
}