/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    BUZ_config.h   >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                            **
 **                  SWC    : BUZZER                                         **
 **                                                                           **
 **===========================================================================**
 */

#ifndef BUZ_CONFIG_H_
#define BUZ_CONFIG_H_

/* Include needed files */
#include "GPIO_interface.h"

/********************** Buzzer GPIO Configurations **********************/
/* Default GPIO configurations for buzzer */
#define BUZ_DEFAULT_PORT GPIO_PORTA
#define BUZ_DEFAULT_PIN GPIO_PIN0
#define BUZ_DEFAULT_STATE ACTIVE_HIGH

/* GPIO Mode Configuration */
#define BUZ_GPIO_MODE GPIO_OUTPUT
#define BUZ_GPIO_OTYPE GPIO_PUSH_PULL
#define BUZ_GPIO_SPEED GPIO_MEDIUM_SPEED
#define BUZ_GPIO_PULL GPIO_NO_PULL

/* Example Buzzer Configuration */
/*
BUZ_Config_t Buzzer1 = {
    .Port = BUZ_DEFAULT_PORT,
    .Pin = BUZ_DEFAULT_PIN,
    .ActiveState = BUZ_DEFAULT_STATE
};
*/

#endif /* BUZ_CONFIG_H_ */
