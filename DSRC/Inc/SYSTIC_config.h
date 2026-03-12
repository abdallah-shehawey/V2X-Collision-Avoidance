/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SYSTIC_config.h    >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SYSTIC                                          **
 **                                                                           **
 ** Important: All configurations in this file affect the timer's behavior    **
 **           and must match your system's requirements.                      **
 **===========================================================================**
 */

#ifndef MCAL_SYSTIC_CONFIG_H_
#define MCAL_SYSTIC_CONFIG_H_

/**
 * @defgroup SYSTIC_Clock_Config System Clock Configuration
 * @{
 */

/**
 * @brief System clock frequency in MHz
 * @note This MUST match your MCU's configured system clock frequency
 * @warning Incorrect value will result in inaccurate timing
 */
#define SYSTEM_CLOCK_IN_MHZ 16U

/**
 * @brief System clock frequency in KHz
 * @note Automatically calculated from SYSTEM_CLOCK_IN_MHZ
 * @warning Do not modify this value directly
 */
#define SYSTEM_CLOCK_IN_KHZ (SYSTEM_CLOCK_IN_MHZ * 1000U)

#define SYSTEM_CLOCK_IN_HZ  (SYSTEM_CLOCK_IN_KHZ * 1000U)
/** @} */

/**
 * @defgroup SYSTIC_Timer_Config SysTick Timer Configuration
 * @{
 */

/**
 * @brief SysTick Clock Source Configuration
 * @note Select the clock source for the SysTick timer
 *
 * @options:
 * - CLK_SOURCE_AHB: Use processor clock (maximum precision)
 *   Suitable for: High-precision timing requirements
 *
 * - CLK_SOURCE_AHB_DIV8: Use processor clock divided by 8 (power efficient)
 *   Suitable for: General purpose timing with lower power consumption
 */
#define SYSTIC_CLKSOURCE CLK_SOURCE_AHB

/**
 * @brief SysTick Exception Configuration
 * @note Controls whether the SysTick timer generates exceptions
 *
 * @options:
 * - ENABLE: Timer will generate an exception when it reaches zero
 *   Use this for: RTOS tick source or regular interval interrupts
 *
 * - DISABLE: No exceptions generated (polling mode)
 *   Use this for: Simple delay functions or when interrupts are not needed
 */
#define SYSTIC_TICKINT DISABLE
/** @} */

#endif /* MCAL_SYSTIC_CONFIG_H_ */
