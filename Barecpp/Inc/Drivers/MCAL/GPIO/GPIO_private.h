/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    GPIO_private.h     >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : GPIO                                            **
 **                                                                           **
 **===========================================================================**
 */

#ifndef GPIO_PRIVATE_H_
#define GPIO_PRIVATE_H_

#define UNUSED(X) (void)X

/************************** GPIO PORT COUNT **************************/
#define GPIO_PORT_COUNT 8u

/************************** GPIO MODER MASK **************************/
#define MODER_MASK      0b11
#define MODER_PIN_ACCESS 2u

/************************** GPIO OTYPER MASK **************************/
#define OTYPER_MASK     0b1

/************************** GPIO OSPEEDR MASK **************************/
#define OSPEEDR_MASK    0b11
#define OSPEEDR_PIN_ACCESS 2u

/************************** GPIO PUPDR MASK **************************/
#define PUPDR_MASK      0b11
#define PUPDR_PIN_ACCESS   2u

/* 4 bits for Alternate Function Configuration */
#define AFR_MASK          0xF
#define AFR_PIN_ACCESS    4u
#define AFR_PIN_SHIFT     8u

/* Number of pins per port */
#define GPIO_PIN_COUNT 16u

#endif /* GPIO_PRIVATE_H_ */
