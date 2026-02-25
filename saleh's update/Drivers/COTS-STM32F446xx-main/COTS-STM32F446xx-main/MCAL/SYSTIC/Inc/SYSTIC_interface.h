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

#include "../../../LIB/ErrTypes.h"
#include <stdint.h>

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
 * @example SYSTIC_vInit();
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
 * @example SYSTIC_vDelayMs(1000);
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
 * @warning For very short delays (<10µs), accuracy may be affected by function
 * call overhead
 * @example SYSTIC_vDelayUs(500);
 */
void SYSTIC_vDelayUs(uint32_t Copy_u32UsTime);

/**
 * @fn SYSTIC_enumGetElapsedTickSingleShot
 * @brief Generate a precise microsecond delay using polling method
 *
 * @param[in] Copy_pu32Tick Pointer to store the elapsed tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock
 * configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual elapsed tick count may be
 * longer than requested due to function call overhead
 * @example
 * uint32_t elapsed;
 * SYSTIC_enumGetElapsedTickSingleShot(&elapsed);
 */
ErrorState_t SYSTIC_enumGetElapsedTickSingleShot(uint32_t *Copy_pu32Tick);
/**
 * @fn SYSTIC_enumRemainingTickSingleShot
 * @brief Get the remaining tick count for a single-shot SysTick timer
 *
 * @param[in] Copy_pu32Tick Pointer to store the remaining tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks remaining for a single-shot
 * SysTick timer
 *
 * @note The actual remaining tick count might be slightly longer than requested
 * due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual remaining tick count may
 * be longer than requested due to function call overhead
 * @example
 * uint32_t remaining;
 * SYSTIC_enumRemainingTickSingleShot(&remaining);
 */
ErrorState_t SYSTIC_enumRemainingTickSingleShot(uint32_t *Copy_pu32Tick);
/**
 * @fn SYSTIC_enumCallback
 * @warning For very short delays (<10µs), the actual delay may be longer
 *          than requested due to function call overhead
 * @example SYSTIC_enumCallback(MyPeriodicFunction, 1000);
 */
ErrorState_t SYSTIC_enumCallback(void (*Copy_pvCallBack)(void),
                                 uint32_t Copy_u32Tick);
/**
 * @fn SYSTIC_enumCallbackSingleShot
 * @warning For very short delays (<10µs), the actual delay may be longer
 *          than requested due to function call overhead
 * @example SYSTIC_enumCallbackSingleShot(MyOnceFunction, 1000);
 */
ErrorState_t SYSTIC_enumCallbackSingleShot(void (*Copy_pvCallBack)(void),
                                           uint32_t Copy_u32Tick);

#endif /* MCAL_SYSTIC_INTERFACE_H_ */
