/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SW_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : SW                                              **
 **                                                                           **
 **===========================================================================**
 */
#include <stdint.h>
#include "STD_MACROS.h"
#include "ErrTypes.h"

#include "GPIO_interface.h"

#include "SW_interface.h"
#include "SW_config.h"
#include "SW_private.h"

/*******************************************************************************
 *                           Hardware Connection Diagram                         *
 *******************************************************************************/
/*
 * Switch Connection Options:
 *
 * 1. Internal Pull-up Configuration:
 *    - Switch connects GPIO pin to GND
 *    - MCU's internal pull-up enabled
 *    - Pressed = LOW, Released = HIGH
 *
 * 2. Internal Pull-down Configuration:
 *    - Switch connects GPIO pin to VDD
 *    - MCU's internal pull-down enabled
 *    - Pressed = HIGH, Released = LOW
 *
 * 3. External Pull-up Configuration:
 *    - Switch connects GPIO pin to GND
 *    - External pull-up resistor to VDD
 *    - MCU's internal pull disabled
 *    - Pressed = LOW, Released = HIGH
 *
 * 4. External Pull-down Configuration:
 *    - Switch connects GPIO pin to VDD
 *    - External pull-down resistor to GND
 *    - MCU's internal pull disabled
 *    - Pressed = HIGH, Released = LOW
 */

/*******************************************************************************
 *                           Function Implementations                            *
 *******************************************************************************/

/**
 * @fn    SW_Init
 * @brief Initialize a switch/button
 * @details This function:
 *          1. Configures GPIO pin as input
 *          2. Sets appropriate pull-up/down configuration
 *          3. Configures pin speed and output type
 * @param SW_Config: Pointer to switch configuration structure
 * @note  The function handles both internal and external pull configurations:
 *        - For internal pull-up/down: Enables MCU's internal resistors
 *        - For external pull-up/down: Disables MCU's internal resistors
 */
void SW_Init(const SW_Config_t *SW_Config)
{
  GPIO_PinConfig_t GPIO_PinConfig = {0};

  /* Configure basic pin settings */
  GPIO_PinConfig.Port = SW_Config->Port;
  GPIO_PinConfig.PinNum = SW_Config->Pin;
  GPIO_PinConfig.Mode = GPIO_INPUT;
  GPIO_PinConfig.Speed = GPIO_LOW_SPEED;
  GPIO_PinConfig.Otype = GPIO_PUSH_PULL;

  /* Configure pull-up/down based on switch configuration */
  if (SW_Config->PullType == SW_INT_PULL_UP)
  {
    GPIO_PinConfig.PullType = GPIO_PULL_UP;
  }
  else if (SW_Config->PullType == SW_INT_PULL_DOWN)
  {
    GPIO_PinConfig.PullType = GPIO_PULL_DOWN;
  }
  else if (SW_Config->PullType == SW_EXT_PULL_UP)
  {
    GPIO_PinConfig.PullType = GPIO_NO_PULL; /* External pull-up */
  }
  else if (SW_Config->PullType == SW_EXT_PULL_DOWN)
  {
    GPIO_PinConfig.PullType = GPIO_NO_PULL; /* External pull-down */
  }

  /* Initialize GPIO pin */
  GPIO_enumPinInit(&GPIO_PinConfig);
}

/**
 * @fn    SW_Read
 * @brief Read the current state of a switch
 * @details This function:
 *          1. Reads the GPIO pin state
 *          2. Interprets the state based on pull configuration:
 *             - Pull-up (int/ext): LOW = Pressed, HIGH = Released
 *             - Pull-down (int/ext): HIGH = Pressed, LOW = Released
 *          3. Returns the logical switch state
 * @param Config: Pointer to switch configuration structure
 * @return uint8_t:
 *         - SW_PRESSED: Switch is pressed
 *         - SW_NOT_PRESSED: Switch is not pressed
 * @note  The function automatically handles the logic inversion
 *        needed for different pull configurations
 */
uint8_t SW_Read(const SW_Config_t *Config)
{
  GPIO_PinValue_t LOCAL_u8PinVal = SW_NOT_PRESSED;
  uint8_t LOC_u8Result = SW_NOT_PRESSED;

  /* Read the current pin value */
  GPIO_enumReadPinVal(Config->Port, Config->Pin, &LOCAL_u8PinVal);

  /* Determine switch state based on pull configuration */
  if ((Config->PullType == SW_INT_PULL_UP) || (Config->PullType == SW_EXT_PULL_UP))
  {
    /* For pull-up configurations, LOW means pressed */
    LOC_u8Result = (LOCAL_u8PinVal == GPIO_PIN_LOW) ? SW_PRESSED : SW_NOT_PRESSED;
  }
  else if ((Config->PullType == SW_INT_PULL_DOWN) || (Config->PullType == SW_EXT_PULL_DOWN))
  {
    /* For pull-down configuration, HIGH means pressed */
    LOC_u8Result = (LOCAL_u8PinVal == GPIO_PIN_HIGH) ? SW_PRESSED : SW_NOT_PRESSED;
  }

  return LOC_u8Result;
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>