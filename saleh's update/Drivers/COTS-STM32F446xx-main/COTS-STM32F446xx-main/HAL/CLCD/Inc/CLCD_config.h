/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCD_config.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCD                                            **
 **                                                                           **
 **===========================================================================**
 */

#ifndef CLCD_CONFIG_H_
#define CLCD_CONFIG_H_

/*___________________________________________________________________________________________________________________*/

/* LCD Mode */
/*
*Choose 8 for 8_BIT Connection, 4 for 4_BIT Connection
*Option
  1- CLCD_FOUR_BITS_MODE
  2- CLCD_EIGHT_BITS_MODE
*/
#define CLCD_MODE CLCD_FOUR_BITS_MODE

/*___________________________________________________________________________________________________________________*/

/*
*Optoins :-
  1- CLCD_DISPLAYON_CURSOROFF
  2- CLCD_DISPLAYOFF_CURSOROFF
  3- CLCD_DISPLAYON_CURSORON
  4- CLCD_DISPLAYON_CURSORON_BLINK
*/

#define CLCD_DISPLAY_CURSOR CLCD_DISPLAYON_CURSORON

/*___________________________________________________________________________________________________________________*/

/*
*Options :-
  1- GPIO_PORTA
  2- GPIO_PORTB
  3- GPIO_PORTC
  4- GPIO_PORTD
  5- GPIO_PORTE
  6- GPIO_PORTF
  7- GPIO_PORTG
  8- GPIO_PORTH
*/

/* D0:D7 */
#define CLCD_DATA_PORT GPIO_PORTA
#define CLCD_DATA_START_PIN GPIO_PIN4
/* RS, RW, EN */
#define CLCD_CONTROL_PORT GPIO_PORTB

/*___________________________________________________________________________________________________________________*/

/*
*Options :-
  1- GPIO_PIN0
  2- GPIO_PIN1
  3- GPIO_PIN2
  4- GPIO_PIN3
  5- GPIO_PIN4
  6- GPIO_PIN5
  7- GPIO_PIN6
  8- GPIO_PIN7
  9- GPIO_PIN8
  10- GPIO_PIN9
  11- GPIO_PIN10
  12- GPIO_PIN11
  13- GPIO_PIN12
  14- GPIO_PIN13
  15- GPIO_PIN14
  16- GPIO_PIN15
*/

#define CLCD_RS GPIO_PIN0
#define CLCD_RW GPIO_PIN1
#define CLCD_EN GPIO_PIN2

/* GPIO Configuration */
#define CLCD_GPIO_MODE GPIO_OUTPUT
#define CLCD_GPIO_OTYPE GPIO_PUSH_PULL
#define CLCD_GPIO_SPEED GPIO_MEDIUM_SPEED
#define CLCD_GPIO_PULL GPIO_NO_PULL

#endif /* CLCD_CONFIG_H_ */
