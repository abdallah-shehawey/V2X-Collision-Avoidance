/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCD_private.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCD                                            **
 **                                                                           **
 **===========================================================================**
 */
#ifndef CLCD_PRIVATE_H_
#define CLCD_PRIVATE_H_

#define F_CPU 16000000UL // CPU frequency in Hz

/* Static helper function prototypes */
static void CLCD_vSendFallingEdge(void);
static ErrorState_t CLCD_InitPin(GPIO_Port_t Port, GPIO_Pin_t Pin);
static ErrorState_t CLCD_InitPort8Bits(GPIO_Port_t Port, GPIO_Pin_t StartPin);
static void CLCD_vSendFallingEdge(void);

/* Private function declarations */
static void _delay_ms(uint32_t ms);
static void _delay_us(uint32_t us);

#endif /* CLCD_PRIVATE_H_ */
