/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<    SYSTIC_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SYSTIC                                          **
 **                                                                           **
 **===========================================================================**
 */

#ifndef MCAL_SYSTIC_INTERFACE_H_
#define MCAL_SYSTIC_INTERFACE_H_

#include <stdint.h>
#include "ErrTypes.h"

/**
 * @fn SYSTIC_vInit
 * @brief Initialize the SysTick timer with configured settings
 * @details This function initializes the SysTick timer with the following:
 *          - Configures the clock source (AHB or AHB/8)
 *          - Sets up interrupt settings if enabled
 *          - Prepares the timer for delay operations
 *
 * @note Must be called before using any other SYSTIC functions
 * @warning Ensure proper clock configuration before initialization
 */
void SYSTIC_vInit(void);

/**
 * @fn SYSTIC_vDelayMs
 * @brief Generate a precise millisecond delay using polling method
 *
 * @param[in] Copy_u32MsTime Delay duration in milliseconds (1 to 16777215 ms)
 *
 * @details Uses the SysTick timer to generate accurate millisecond delays
 *          by polling the COUNTFLAG bit
 *
 * @note This is a blocking function
 * @warning Maximum delay is limited by the 24-bit counter
 */
void SYSTIC_vDelayMs(uint32_t Copy_u32MsTime);

/**
 * @fn SYSTIC_vDelayUs
 * @brief Generate a precise microsecond delay using polling method
 *
 * @param[in] Copy_u32UsTime Delay duration in microseconds (1 to 16777215 µs)
 *
 * @details Uses the SysTick timer to generate accurate microsecond delays
 *          by polling the COUNTFLAG bit
 *
 * @note This is a blocking function
 * @warning For very short delays (<10µs), accuracy may be affected by function call overhead
 */
void SYSTIC_vDelayUs(uint32_t Copy_u32UsTime);

#endif /* MCAL_SYSTIC_INTERFACE_H_ */