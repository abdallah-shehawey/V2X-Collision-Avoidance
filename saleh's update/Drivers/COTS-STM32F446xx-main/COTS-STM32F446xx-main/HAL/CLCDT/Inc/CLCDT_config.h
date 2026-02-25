/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCDT_config.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCDT (LCD with TIM5 Delay)                    **
 **                                                                           **
 **===========================================================================**
 */

#ifndef CLCDT_CONFIG_H_
#define CLCDT_CONFIG_H_

/*___________________________________________________________________________________________________________________*/

/* LCD Mode */
/*
 * Choose 8 for 8_BIT Connection, 4 for 4_BIT Connection
 * Options:
 *   1- CLCDT_FOUR_BITS_MODE
 *   2- CLCDT_EIGHT_BITS_MODE
 */
#define CLCDT_MODE CLCDT_FOUR_BITS_MODE

/*___________________________________________________________________________________________________________________*/

/*
 * Options:
 *   1- CLCDT_DISPLAYON_CURSOROFF
 *   2- CLCDT_DISPLAYOFF_CURSOROFF
 *   3- CLCDT_DISPLAYON_CURSORON
 *   4- CLCDT_DISPLAYON_CURSORON_BLINK
 */
#define CLCDT_DISPLAY_CURSOR CLCDT_DISPLAYON_CURSORON

/*___________________________________________________________________________________________________________________*/

/* Data Port & Start Pin (4-bit: D4..D7) */
#define CLCDT_DATA_PORT       GPIO_PORTA
#define CLCDT_DATA_START_PIN  GPIO_PIN4

/* Control Port Pins */
#define CLCDT_CONTROL_PORT    GPIO_PORTB
#define CLCDT_RS              GPIO_PIN0
#define CLCDT_RW              GPIO_PIN1
#define CLCDT_EN              GPIO_PIN2

/*___________________________________________________________________________________________________________________*/

/* GPIO Configuration */
#define CLCDT_GPIO_MODE   GPIO_OUTPUT
#define CLCDT_GPIO_OTYPE  GPIO_PUSH_PULL
#define CLCDT_GPIO_SPEED  GPIO_MEDIUM_SPEED
#define CLCDT_GPIO_PULL   GPIO_NO_PULL

/*___________________________________________________________________________________________________________________*/

/*
 * TIM5 Delay Configuration
 * TIM5 is on APB1 Bus.
 * With HSI = 16MHz and APB1 prescaler = 1:
 *   TIM5 Clock = 16MHz
 *   Prescaler  = 16-1 = 15  --> Timer Tick = 1us (1MHz)
 */
#define CLCDT_TIM5_PRESCALER  (16U - 1U)   /* 1us tick at 16MHz HSI */

#endif /* CLCDT_CONFIG_H_ */
