/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    LED_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446RE                                          **
 **                  SWC    : LED                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef LED_INTERFACE_H_
#define LED_INTERFACE_H_

#include "GPIO_interface.h"
#include "ErrTypes.h"

typedef enum
{
  ACTIVE_LOW = 0,
  ACTIVE_HIGH
} LED_ActiveState_t;

/* LED Configuration Structure */
typedef struct
{
  GPIO_Port_t PortName; /* GPIO Port (PORTA to PORTH) */
  GPIO_Pin_t PinNumber; /* GPIO Pin Number (PIN0 to PIN15) */
  LED_ActiveState_t ActiveState; /* LED Active State (ACTIVE_HIGH or ACTIVE_LOW) */
} LED_Config_t;

/*
 * @brief  : Initialize LED pin as output
 * @param  : LED_Configuration - Structure containing LED configuration
 * @return : ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_Init(LED_Config_t *LED_Configuration);

/*
 * @brief  : Turn LED on
 * @param  : LED_Configuration - Structure containing LED configuration
 * @return : ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_TurnOn(LED_Config_t *LED_Configuration);

/*
 * @brief  : Turn LED off
 * @param  : LED_Configuration - Structure containing LED configuration
 * @return : ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_TurnOff(LED_Config_t *LED_Configuration);

/*
 * @brief  : Toggle LED state
 * @param  : LED_Configuration - Structure containing LED configuration
 * @return : ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_Toggle(LED_Config_t *LED_Configuration);

#endif /* LED_INTERFACE_H_ */

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
