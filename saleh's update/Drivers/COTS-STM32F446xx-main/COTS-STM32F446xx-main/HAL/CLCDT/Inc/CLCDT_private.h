/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCDT_private.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCDT (LCD with TIM5 Delay)                    **
 **                                                                           **
 **===========================================================================**
 */

#ifndef CLCDT_PRIVATE_H_
#define CLCDT_PRIVATE_H_

/* Static helper function prototypes */
static void CLCDT_vSendFallingEdge(void);
static ErrorState_t CLCDT_InitPin(GPIO_Port_t Port, GPIO_Pin_t Pin);
static ErrorState_t CLCDT_InitPort8Bits(GPIO_Port_t Port, GPIO_Pin_t StartPin);

/* Private delay functions using TIM5 */
static void _delay_us_tim5(uint32_t us);
static void _delay_ms_tim5(uint32_t ms);

#endif /* CLCDT_PRIVATE_H_ */
