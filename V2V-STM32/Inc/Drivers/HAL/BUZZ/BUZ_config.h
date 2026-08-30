/**
 ******************************************************************************
 * @file    BUZ_config.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Compile-time configuration for the BUZZ driver — the piezo buzzer.
 * @ingroup hal_buzz
 ******************************************************************************
 */

#ifndef BUZ_CONFIG_H_
#define BUZ_CONFIG_H_

/* Include needed files */
#include "../../MCAL/GPIO/GPIO_interface.h"

/********************** Buzzer GPIO Configurations **********************/
/* Default GPIO configurations for buzzer */
#define BUZ_DEFAULT_PORT  GPIO_PORTA      /**< Port the buzzer is wired to. */
#define BUZ_DEFAULT_PIN   GPIO_PIN0       /**< Pin the buzzer is wired to. */
#define BUZ_DEFAULT_STATE BUZ_ACTIVE_HIGH /**< Whether the buzzer sounds on a high or a low level. */
/* GPIO Mode Configuration */
#define BUZ_GPIO_MODE  GPIO_OUTPUT       /**< Pin mode — the buzzer is driven as a plain digital output. */
#define BUZ_GPIO_OTYPE GPIO_PUSH_PULL    /**< Output type: push-pull, so the pin actively drives both levels. */
#define BUZ_GPIO_SPEED GPIO_MEDIUM_SPEED /**< Slew rate. A buzzer needs nothing fast; a slower edge means less EMI. */
#define BUZ_GPIO_PULL  GPIO_NO_PULL      /**< No internal pull resistor — the pin is always actively driven. */
/* Example Buzzer Configuration */
/*
BUZ_Config_t Buzzer1 = {
    .Port = BUZ_DEFAULT_PORT,
    .Pin = BUZ_DEFAULT_PIN,
    .ActiveState = BUZ_DEFAULT_STATE
};
*/

#endif /* BUZ_CONFIG_H_ */
