/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    RCC_private.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : RCC                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef MCAL_RCC_PRIVATE_H_
#define MCAL_RCC_PRIVATE_H_

#define RCC_MPLL_DIV_MASK 0X3F
#define RCC_NPLL_MULT_MASK 0X1FF
#define RCC_PPLL_DIV_MASK 0X3
#define RCC_SYS_CLK_MASK 0X3

#define RCC_u32TIMEOUT 10000UL

#endif /* MCAL_RCC_PRIVATE_H_ */
