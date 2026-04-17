/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SYSTIC_private.h    >>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SYSTIC                                          **
 **                                                                           **
 **===========================================================================**
 */

#ifndef MCAL_SYSTIC_PRIVATE_H_
#define MCAL_SYSTIC_PRIVATE_H_

/**
 * @defgroup SYSTIC_Registers SysTick Register Bit Definitions
 * @brief Bit positions in the SysTick Control and Status Register (STK_CTRL)
 * @{
 */
#define SYSTIC_CTRL_ENABLE     (0U)    /**< Counter Enable bit - Enables the counter */
#define SYSTIC_CTRL_TICKINT    (1U)    /**< Exception Enable bit - Enables SysTick exception request */
#define SYSTIC_CTRL_CLKSOURCE  (2U)    /**< Clock Source bit - Selects the clock source (AHB or AHB/8) */
#define SYSTIC_CTRL_COUNTFLAG  (16U)   /**< Count Flag bit - Indicates counter has counted to 0 since last read */
/** @} */

/**
 * @defgroup SYSTIC_Constants SysTick Constants
 * @brief Important constants for SysTick configuration
 * @{
 */
#define SYSTIC_MAX_NO_OF_TICKS (0x00FFFFFF) /**< Maximum counter value (24-bit counter) */

/**
 * @brief Clock source selection values for SysTick timer
 * @note These values are written to CLKSOURCE bit in STK_CTRL
 */
#define CLK_SOURCE_AHB      (1U)      /**< Use processor clock (AHB) directly */
#define CLK_SOURCE_AHB_DIV8 (0U) /**< Use processor clock divided by 8 */

/**
 * @brief Generic enable/disable definitions
 */
#define ENABLE  (1U)  /**< Enable a feature */
#define DISABLE (0U) /**< Disable a feature */
/** @} */

/**
 * @defgroup SYSTIC_Private_Functions SysTick Private Functions
 * @brief Internal helper functions for SysTick operations
 * @{
 */

/**
 * @brief Enable the SysTick counter
 * @note Sets the ENABLE bit in STK_CTRL register
 */
static void SYSTIC_vEnable(void);

/**
 * @brief Disable the SysTick counter
 * @note Clears the ENABLE bit in STK_CTRL register
 */
static void SYSTIC_vDisable(void);

/**
 * @brief Wait for the SysTick counter to reach zero
 * @note Polls the COUNTFLAG bit in STK_CTRL register
 */
static void SYSTIC_vWait(void);
/** @} */

#endif /* MCAL_SYSTIC_PRIVATE_H_ */