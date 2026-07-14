/**
 ******************************************************************************
 * @file    SYSTIC_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the SYSTICK driver — the Cortex-M system timer.
 * @ingroup mcal_systic
 ******************************************************************************
 */

#ifndef MCAL_SYSTIC_INTERFACE_H_
#define MCAL_SYSTIC_INTERFACE_H_

#include "../../LIB/ErrTypes.h"
#include <stdint.h>

/**
 * @brief Initialize the SysTick timer with configured settings
 * @details This function initializes the SysTick timer with the following:
 *          - Configures the clock source (AHB or AHB/8)
 *          - Sets up interrupt settings if enabled
 *          - Prepares the timer for delay operations
 *
 * @note Must be called before using any other SYSTIC functions
 * @warning Ensure proper clock configuration before initialization
 * @par Example:
 * SYSTIC_vInit();
 */
void SYSTIC_vInit(void);

/**
 * @brief Generate a precise millisecond delay using polling method
 *
 * @param[in] Copy_u32MsTime Delay duration in milliseconds (1 to 16777215 ms)
 *
 * @details Uses the SysTick timer to generate accurate millisecond delays
 *          by polling the COUNTFLAG bit
 *
 * @note This is a blocking function
 * @warning Maximum delay is limited by the 24-bit counter
 * @par Example:
 * SYSTIC_vDelayMs(1000);
 */
void SYSTIC_vDelayMs(uint32_t Copy_u32MsTime);

/**
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
 * @par Example:
 * SYSTIC_vDelayUs(500);
 */
void SYSTIC_vDelayUs(uint32_t Copy_u32UsTime);

/**
 * @brief Generate a precise microsecond delay using polling method
 *
 * @param[in] Copy_pu32Tick Pointer to store the elapsed tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock
 * configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *
 * @return OK if the tick count was written to @p Copy_pu32Tick, NULL_POINTER if it was NULL.
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual elapsed tick count may be
 * longer than requested due to function call overhead
 * @par Example:
 * 
 * uint32_t elapsed;
 * SYSTIC_enumGetElapsedTickSingleShot(&elapsed);
 */
ErrorState_t SYSTIC_enumGetElapsedTickSingleShot(uint32_t *Copy_pu32Tick);
/**
 * @brief Get the remaining tick count for a single-shot SysTick timer
 *
 * @param[in] Copy_pu32Tick Pointer to store the remaining tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks remaining for a single-shot
 * SysTick timer
 *
 * @return OK if the remaining count was written to @p Copy_pu32Tick, NULL_POINTER if it was NULL.
 *
 * @note The actual remaining tick count might be slightly longer than requested
 * due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual remaining tick count may
 * be longer than requested due to function call overhead
 * @par Example:
 * 
 * uint32_t remaining;
 * SYSTIC_enumRemainingTickSingleShot(&remaining);
 */
ErrorState_t SYSTIC_enumRemainingTickSingleShot(uint32_t *Copy_pu32Tick);
/**
 * @brief Register a **periodic** callback, fired from the SysTick ISR every
 *        @p Copy_u32Tick ticks.
 *
 * @param[in] Copy_pvCallBack Function to call. Runs in interrupt context, so it must
 *                            be short and non-blocking.
 * @param     Copy_u32Tick    Period, in SysTick ticks.
 * @retval OK           The callback was registered and the timer started.
 * @retval NULL_POINTER @p Copy_pvCallBack was NULL.
 *
 * @warning For very short periods (under ~10 us) the call overhead is a
 *          significant fraction of the period, and the real interval will be longer
 *          than asked for.
 */
ErrorState_t SYSTIC_enumCallback(void (*Copy_pvCallBack)(void),
                                 uint32_t Copy_u32Tick);
/**
 * @brief Register a **one-shot** callback, fired once after @p Copy_u32Tick ticks.
 *
 * @param[in] Copy_pvCallBack Function to call. Runs in interrupt context.
 * @param     Copy_u32Tick    Delay before it fires, in SysTick ticks.
 * @retval OK           The callback was registered and the timer started.
 * @retval NULL_POINTER @p Copy_pvCallBack was NULL.
 *
 * @note Disarms itself after firing — unlike @ref SYSTIC_enumCallback, which
 *       repeats until it is replaced.
 */
ErrorState_t SYSTIC_enumCallbackSingleShot(void (*Copy_pvCallBack)(void),
                                           uint32_t Copy_u32Tick);

#endif /* MCAL_SYSTIC_INTERFACE_H_ */
