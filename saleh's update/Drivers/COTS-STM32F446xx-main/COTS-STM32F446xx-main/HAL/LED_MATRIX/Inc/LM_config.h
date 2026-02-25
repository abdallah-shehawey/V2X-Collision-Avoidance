/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    LM_config.h     >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : LED_MATRIX                                      **
 **                                                                           **
 **===========================================================================**
 */
#ifndef LM_CONFIG_H_
#define LM_CONFIG_H_

#define LED_MATRIX_ROW_PORT       GPIO_PORTA
#define LED_MATRIX_COL_PORT       GPIO_PORTB
#define LED_MATRIX_ROW_START_PIN  GPIO_PIN0
#define LED_MATRIX_COL_START_PIN  GPIO_PIN0

GPIO_HalfPortConfig_t LED_MATRIC_ROW =
{
    .Port = LED_MATRIX_ROW_PORT,
    .Mode = GPIO_OUTPUT,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_LOW_SPEED,
    .PullType = GPIO_NO_PULL,
};

GPIO_8PinsConfig_t LED_MATRIX_COL =
{
    .Port = LED_MATRIX_COL_PORT,
    .Mode = GPIO_OUTPUT,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_LOW_SPEED,
    .PullType = GPIO_NO_PULL,
    .StartPin = LED_MATRIX_COL_START_PIN
};

#endif /* LM_CONFIG_H_ */
