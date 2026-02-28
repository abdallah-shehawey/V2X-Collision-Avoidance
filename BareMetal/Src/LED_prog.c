/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    LED_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446RE                                          **
 **                  SWC    : LED                                             **
 **                                                                           **
 **===========================================================================**
 */

#include "ErrTypes.h"

#include "GPIO_interface.h"

#include "LED_interface.h"
#include "LED_config.h"
#include "LED_private.h"
/*___________________________________________________________________________________________________________________*/

/*
* Brief : This Function initialize the pin which connected to led as output pin
* Parameters :
            => struct has the led port , pin, status
* return : ErrorState_t
*/
ErrorState_t LED_Init(LED_Config_t *LED_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (LED_Configuration != NULL)
  {
    GPIO_PinConfig_t Pin_Config =
        {
            .Port = LED_Configuration->PortName,
            .PinNum = LED_Configuration->PinNumber,
            .Mode = GPIO_OUTPUT,
            .Otype = GPIO_PUSH_PULL,
            .Speed = GPIO_MEDIUM_SPEED,
            .PullType = GPIO_NO_PULL
        };

    Local_ErrorState = GPIO_enumPinInit(&Pin_Config);
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }
  return Local_ErrorState;
}

/*___________________________________________________________________________________________________________________*/

/*
* Brief : This Function set high on led pin ( led on )
* Parameters :
            => struct has the led port , pin, status
* return : ErrorState_t
*/
ErrorState_t LED_TurnOn(LED_Config_t *LED_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (LED_Configuration != NULL)
  {
    if (LED_Configuration->ActiveState == ACTIVE_HIGH)
    {
      Local_ErrorState = GPIO_enumWritePinVal(LED_Configuration->PortName, LED_Configuration->PinNumber, GPIO_PIN_HIGH);
    }
    else if (LED_Configuration->ActiveState == ACTIVE_LOW)
    {
      Local_ErrorState = GPIO_enumWritePinVal(LED_Configuration->PortName, LED_Configuration->PinNumber, GPIO_PIN_LOW);
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }
  return Local_ErrorState;
}

/*___________________________________________________________________________________________________________________*/

/*
* Brief : This Function set low on led pin ( led off )
* Parameters :
            => struct has the led port , pin , status
* return : ErrorState_t
*/
ErrorState_t LED_TurnOff(LED_Config_t *LED_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (LED_Configuration != NULL)
  {
    if (LED_Configuration->ActiveState == ACTIVE_HIGH)
    {
      Local_ErrorState = GPIO_enumWritePinVal(LED_Configuration->PortName, LED_Configuration->PinNumber, GPIO_PIN_LOW);
    }
    else if (LED_Configuration->ActiveState == ACTIVE_LOW)
    {
      Local_ErrorState = GPIO_enumWritePinVal(LED_Configuration->PortName, LED_Configuration->PinNumber, GPIO_PIN_HIGH);
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }
  return Local_ErrorState;
}

/*___________________________________________________________________________________________________________________*/

/*
* Brief : This Function toggle led pin
* Parameters :
            => struct has the led port , pin , status
* return : ErrorState_t
*/
ErrorState_t LED_Toggle(LED_Config_t *LED_Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (LED_Configuration != NULL)
  {
    Local_ErrorState = GPIO_enumTogPinVal(LED_Configuration->PortName, LED_Configuration->PinNumber);
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }
  return Local_ErrorState;
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>