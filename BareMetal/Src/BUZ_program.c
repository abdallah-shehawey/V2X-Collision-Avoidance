/**
**===========================================================================**
**<<<<<<<<<<<<<<<<<<<<<<<<<<    BUZ_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>**
**                                                                           **
**                  Author : Abdallah Abdelmoemen Shehawey                   **
**                  Layer  : HAL                                            **
**                  SWC    : BUZZER                                         **
**                                                                           **
**===========================================================================**
*/

#include "BUZ_interface.h"
#include "BUZ_private.h"
#include "BUZ_config.h"

/**
 * @brief Initialize the buzzer pin
 */
ErrorState_t BUZ_Init(const BUZ_Config_t *BUZ_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (BUZ_Configuration != NULL)
  {
    /* Create GPIO pin configuration */
    GPIO_PinConfig_t PinConfig =
        {
            .Port = BUZ_Configuration->Port,
            .PinNum = BUZ_Configuration->Pin,
            .Mode = GPIO_OUTPUT,
            .Otype = GPIO_PUSH_PULL,
            .Speed = GPIO_MEDIUM_SPEED,
            .PullType = GPIO_NO_PULL};

    /* Initialize GPIO pin */
    Local_ErrorState = GPIO_enumPinInit(&PinConfig);

    /* Set initial state to OFF */
    if (Local_ErrorState == OK)
    {
      Local_ErrorState = BUZ_Off(BUZ_Configuration);
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }

  return Local_ErrorState;
}

/**
 * @brief Turn on the buzzer
 */
ErrorState_t BUZ_On(const BUZ_Config_t *BUZ_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (BUZ_Configuration != NULL)
  {
    /* Write the appropriate value based on active state */
    GPIO_PinValue_t PinValue = (BUZ_Configuration->ActiveState == ACTIVE_HIGH) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
    Local_ErrorState = GPIO_enumWritePinVal(BUZ_Configuration->Port, BUZ_Configuration->Pin, PinValue);
  }
  else
  {
    Local_ErrorState = NOK;
  }

  return Local_ErrorState;
}

/**
 * @brief Turn off the buzzer
 */
ErrorState_t BUZ_Off(const BUZ_Config_t *BUZ_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (BUZ_Configuration != NULL)
  {
    /* Write the appropriate value based on active state */
    GPIO_PinValue_t PinValue = (BUZ_Configuration->ActiveState == ACTIVE_HIGH) ? GPIO_PIN_LOW : GPIO_PIN_HIGH;
    Local_ErrorState = GPIO_enumWritePinVal(BUZ_Configuration->Port, BUZ_Configuration->Pin, PinValue);
  }
  else
  {
    Local_ErrorState = NOK;
  }

  return Local_ErrorState;
}

/**
 * @brief Toggle the buzzer state
 */
ErrorState_t BUZ_Toggle(const BUZ_Config_t *BUZ_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (BUZ_Configuration != NULL)
  {
    Local_ErrorState = GPIO_enumTogPinVal(BUZ_Configuration->Port, BUZ_Configuration->Pin);
  }
  else
  {
    Local_ErrorState = NOK;
  }

  return Local_ErrorState;
}