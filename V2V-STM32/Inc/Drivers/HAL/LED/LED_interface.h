/**
 ******************************************************************************
 * @file    LED_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the LED driver — the indicator LEDs.
 * @ingroup hal_led
 ******************************************************************************
 */

#ifndef LED_INTERFACE_H_
#define LED_INTERFACE_H_

#include "../../MCAL/GPIO/GPIO_interface.h"
#include "../../LIB/ErrTypes.h"


/**
 * @brief Which pin level lights the LED — i.e. which way round it is wired.
 */
typedef enum
{
  ACTIVE_LOW = 0, /**< Lights when the pin is driven **low** (LED cathode on the pin, anode to VDD). */
  ACTIVE_HIGH     /**< Lights when the pin is driven **high** (LED anode on the pin, cathode to ground). */
} LED_ActiveState_t;

/**
 * @brief Describes one LED: where it is wired, and which way round it is.
 *
 * `ActiveState` is what lets the rest of the firmware say "on" and "off" without
 * knowing how the LED is wired. An @ref ACTIVE_HIGH LED lights when its pin is
 * driven high; an @ref ACTIVE_LOW one lights when the pin is driven *low*
 * (cathode to the pin, anode to VDD). The driver inverts the level for you.
 */
typedef struct
{
  GPIO_Port_t PortName;          /**< Port the LED is on. */
  GPIO_Pin_t PinNumber;          /**< Pin the LED is on. */
  LED_ActiveState_t ActiveState; /**< Whether the LED lights on a high or a low level. */
} LED_Config_t;

/**
 * @brief Initialize LED pin as output
 * @param LED_Configuration - Structure containing LED configuration
 * @return ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_Init(LED_Config_t *LED_Configuration);

/**
 * @brief Turn LED on
 * @param LED_Configuration - Structure containing LED configuration
 * @return ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_TurnOn(LED_Config_t *LED_Configuration);

/**
 * @brief Turn LED off
 * @param LED_Configuration - Structure containing LED configuration
 * @return ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_TurnOff(LED_Config_t *LED_Configuration);

/**
 * @brief Toggle LED state
 * @param LED_Configuration - Structure containing LED configuration
 * @return ErrorState_t - Error state (ERROR_OK if successful, ERROR_NOK if error)
 */
ErrorState_t LED_Toggle(LED_Config_t *LED_Configuration);

#endif /* LED_INTERFACE_H_ */

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
