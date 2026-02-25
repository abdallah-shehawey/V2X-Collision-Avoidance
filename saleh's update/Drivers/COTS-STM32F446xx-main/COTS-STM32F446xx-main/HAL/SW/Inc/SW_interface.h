/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SW_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : SW                                              **
 **                                                                           **
 **===========================================================================**
 */

#ifndef SW_INTERFACE_H_
#define SW_INTERFACE_H_

#include "GPIO_interface.h"

/**
 * @brief Switch state definitions
 */
#define SW_PRESSED 1     /**< Switch is in pressed state */
#define SW_NOT_PRESSED 0 /**< Switch is in released state */

/**
 * @brief Switch pull configuration definitions
 * @details Defines four possible pull configurations:
 *          1. Internal pull-up: MCU's internal pull-up resistor
 *          2. Internal pull-down: MCU's internal pull-down resistor
 *          3. External pull-up: External pull-up resistor
 *          4. External pull-down: External pull-down resistor
 */
#define SW_INT_PULL_UP 0   /**< Internal pull-up configuration */
#define SW_INT_PULL_DOWN 1 /**< Internal pull-down configuration */
#define SW_EXT_PULL_UP 2   /**< External pull-up configuration */
#define SW_EXT_PULL_DOWN 3 /**< External pull-down configuration */

/**
 * @brief Switch Configuration Structure
 * @details Contains all configuration parameters for a switch:
 *          - GPIO port and pin
 *          - Pull-up/down configuration
 */
typedef struct
{
  GPIO_Port_t Port;           /**< GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t Pin;             /**< GPIO Pin Number (PIN0 to PIN15) */
  GPIO_PullUpDown_t PullType; /**< Pull Configuration (INT/EXT PULL UP/DOWN) */
} SW_Config_t;

/**
 * @fn    SW_Init
 * @brief Initialize a switch/button
 * @details This function:
 *          1. Configures GPIO pin as input
 *          2. Sets appropriate pull-up/down configuration
 *          3. Configures pin speed and output type
 * @param Config: Pointer to switch configuration structure
 */
void SW_Init(const SW_Config_t *Config);

/**
 * @fn    SW_Read
 * @brief Read the current state of a switch
 * @details This function:
 *          1. Reads the GPIO pin state
 *          2. Interprets the state based on pull configuration
 *          3. Returns the logical switch state
 * @param Config: Pointer to switch configuration structure
 * @return uint8_t:
 *         - SW_PRESSED: Switch is pressed
 *         - SW_NOT_PRESSED: Switch is not pressed
 */
uint8_t SW_Read(const SW_Config_t *Config);

#endif /* SW_INTERFACE_H_ */
