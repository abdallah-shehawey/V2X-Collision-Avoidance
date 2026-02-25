/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SYSTIC_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SYSTIC                                          **
 **                                                                           **
 **===========================================================================**
 */
/**
 * @file SYSTIC_program.c
 * @brief SysTick Timer Driver Implementation
 * @details This file implements the SysTick timer functionality for STM32F446RE
 *          microcontroller. SysTick is a 24-bit down counter that can be used
 *          for generating precise delays and periodic interrupts.
 *          The driver supports both polling and interrupt-based operation modes.
 */

#include <stdint.h>
#include "STM32F446xx.h"
#include "ErrTypes.h"

#include "SYSTIC_config.h"
#include "SYSTIC_interface.h"
#include "SYSTIC_private.h"

static void (*SYSTIC_CallBack)(void) = NULL;
static uint8_t SingleShot_Flag = 1;

/*=================================================================================================================*/
/**
 * @fn SYSTIC_vInit
 * @brief Initialize and configure the SysTick timer
 * @details This function performs the following configurations:
 *          1. Sets up the clock source based on SYSTIC_CLKSOURCE configuration
 *             - AHB clock: Maximum precision but higher power consumption
 *             - AHB/8: Lower precision but more power efficient
 *          2. Configures the interrupt state based on SYSTIC_TICKINT
 *             - Enabled: Timer will generate exceptions
 *             - Disabled: Polling mode operation
 *
 * @note This function must be called before using any other SYSTIC functions
 * @warning Ensure system clock is properly configured before calling this function
 */
void SYSTIC_vInit(void)
{
  /* Configure clock source - SysTick can use either:
   * - AHB clock (full speed) when SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
   * - AHB/8 clock (divided by 8) when SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
   */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
  /* Use processor clock (AHB) for maximum precision */
  MSYSTIC->CTRL |= (1u << SYSTIC_CTRL_CLKSOURCE);

#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
  /* Use processor clock divided by 8 for power efficiency */
  MSYSTIC->CTRL &= (~(1u << SYSTIC_CTRL_CLKSOURCE));

#else
#error "Invalid SYSTIC Clock Source Configuration"
#endif

  /* Configure interrupt generation */
#if SYSTIC_TICKINT == ENABLE
  /* Enable SysTick exception generation */
  MSYSTIC->CTRL |= (1u << SYSTIC_CTRL_TICKINT);
#elif SYSTIC_TICKINT == DISABLE
  /* Disable SysTick exception generation (polling mode) */
  MSYSTIC->CTRL &= (~(1u << SYSTIC_CTRL_TICKINT));
#else
#error "Invalid SYSTIC Interrupt Configuration"
#endif
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_vDisable
 * @brief Disable the SysTick timer
 * @details Stops the timer by clearing the ENABLE bit in the CTRL register
 *          This function is used internally by delay functions
 */
static void SYSTIC_vDisable(void)
{
  MSYSTIC->CTRL &= (~(1u << SYSTIC_CTRL_ENABLE));
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_vEnable
 * @brief Enable the SysTick timer
 *
 * @details Starts the timer by setting the ENABLE bit in the CTRL register
 *          This function is used internally by delay functions
 */
static void SYSTIC_vEnable(void)
{
  MSYSTIC->CTRL |= (1u << SYSTIC_CTRL_ENABLE);
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_vWait
 * @brief Wait for the SysTick timer to complete counting
 *
 * @details Polls the COUNTFLAG bit in the CTRL register until it is set,
 *          indicating that the counter has reached zero
 */
static void SYSTIC_vWait(void)
{
  while ((MSYSTIC->CTRL & (1u << SYSTIC_CTRL_COUNTFLAG)) == 0);
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_vDelayMs
 * @brief Generate a precise millisecond delay
 *
 * @param[in] Copy_u32MsTime Delay duration in milliseconds
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very long delays, consider using a timer or RTC instead
 */
void SYSTIC_vDelayMs(uint32_t Copy_u32MsTime)
{
  /* Calculate tick time based on clock source
   * - For AHB/8: 1 tick = 1/(SYSTEM_CLOCK_IN_KHZ/8) ms
   * - For AHB: 1 tick = 1/SYSTEM_CLOCK_IN_KHZ ms
   */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
  float Local_f32TickTimeInMs = 1.0 / (SYSTEM_CLOCK_IN_KHZ / 8.0);
#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
  double Local_f32TickTimeInMs = 1.0 / SYSTEM_CLOCK_IN_KHZ;
#endif

  /* Calculate required number of ticks */
  uint32_t Local_u32NoOfTicks = Copy_u32MsTime / Local_f32TickTimeInMs;

  /* Check if delay fits in single counter cycle */
  if (Local_u32NoOfTicks <= SYSTIC_MAX_NO_OF_TICKS)
  {
    /* Single cycle delay */
    MSYSTIC->VAL = 0;                   /* Clear current value */
    MSYSTIC->LOAD = Local_u32NoOfTicks; /* Load delay value */
    SYSTIC_vEnable();                   /* Start timer */
    SYSTIC_vWait();                     /* Wait for completion */
    SYSTIC_vDisable();                  /* Stop timer */
  }
  else
  {
    /* Handle long delays by breaking into multiple cycles */
    while (Local_u32NoOfTicks > 0)
    {
      if (Local_u32NoOfTicks > SYSTIC_MAX_NO_OF_TICKS)
      {
        /* Load maximum possible value */
        Local_u32NoOfTicks -= SYSTIC_MAX_NO_OF_TICKS;
        MSYSTIC->VAL = 0;
        MSYSTIC->LOAD = SYSTIC_MAX_NO_OF_TICKS;
        SYSTIC_vEnable();
        SYSTIC_vWait();
        SYSTIC_vDisable();
      }
      else
      {
        /* Load remaining ticks */
        MSYSTIC->VAL = 0;
        MSYSTIC->LOAD = Local_u32NoOfTicks;
        SYSTIC_vEnable();
        SYSTIC_vWait();
        SYSTIC_vDisable();
        break;
      }
    }
  }
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_vDelayUs
 * @brief Generate a precise microsecond delay
 *
 * @param[in] Copy_u32UsTime Delay duration in microseconds
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual delay may be longer
 *          than requested due to function call overhead
 */
void SYSTIC_vDelayUs(uint32_t Copy_u32UsTime)
{
  /* Calculate tick time based on clock source
   * - For AHB/8: 1 tick = 1/(SYSTEM_CLOCK_IN_KHZ/8) us
   * - For AHB: 1 tick = 1/SYSTEM_CLOCK_IN_KHZ us
   */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
  double Local_f32TickTimeInUs = 1.0 / (SYSTEM_CLOCK_IN_MHZ / 8.0);
#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
  double Local_f32TickTimeInUs = 1.0 / SYSTEM_CLOCK_IN_MHZ;
#endif

  /* Calculate required number of ticks */
  uint32_t Local_u32NoOfTicks = Copy_u32UsTime / Local_f32TickTimeInUs;

  /* Check if delay fits in single counter cycle */
  if (Local_u32NoOfTicks <= SYSTIC_MAX_NO_OF_TICKS)
  {
    /* Single cycle delay */
    MSYSTIC->VAL = 0;                   /* Clear current value */
    MSYSTIC->LOAD = Local_u32NoOfTicks; /* Load delay value */
    SYSTIC_vEnable();                   /* Start timer */
    SYSTIC_vWait();                     /* Wait for completion */
    SYSTIC_vDisable();                  /* Stop timer */
  }
  else
  {
    /* Handle long delays by breaking into multiple cycles */
    while (Local_u32NoOfTicks > 0)
    {
      if (Local_u32NoOfTicks > SYSTIC_MAX_NO_OF_TICKS)
      {
        /* Load maximum possible value */
        Local_u32NoOfTicks -= SYSTIC_MAX_NO_OF_TICKS;
        MSYSTIC->VAL = 0;
        MSYSTIC->LOAD = SYSTIC_MAX_NO_OF_TICKS;
        SYSTIC_vEnable();
        SYSTIC_vWait();
        SYSTIC_vDisable();
      }
      else
      {
        /* Load remaining ticks */
        MSYSTIC->VAL = 0;
        MSYSTIC->LOAD = Local_u32NoOfTicks;
        SYSTIC_vEnable();
        SYSTIC_vWait();
        SYSTIC_vDisable();
        break;
      }
    }
  }
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_enumGetElapsedTickSingleShot
 * @brief Generate a precise microsecond delay using polling method
 *
 * @param[in] Copy_pu32Tick Pointer to store the elapsed tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual delay may be longer
 *          than requested due to function call overhead
 */
ErrorState_t SYSTIC_enumGetElapsedTickSingleShot(uint32_t * Copy_pu32Tick)
{
      /* Calculate tick time based on clock source */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
double Local_f32TickTimeInUs = 1.0 / (SYSTEM_CLOCK_IN_MHZ / 8.0);
#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
double Local_f32TickTimeInUs = 1.0 / SYSTEM_CLOCK_IN_MHZ;
#endif
  /* Local variables
   * - Local_u32Tick: Stores the current tick count
   * - Local_u32LoadValue: Stores the initial load value
   */
  ErrorState_t Local_enumErrorState = OK;

  if (Copy_pu32Tick == NULL)
  {
    Local_enumErrorState = NULL_POINTER;
  }
  else
  {
    *Copy_pu32Tick = (((MSYSTIC->LOAD) - (MSYSTIC->VAL)) * Local_f32TickTimeInUs);
  }

  return Local_enumErrorState;
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_enumRemainingTickSingleShot
 * @brief Generate a precise microsecond delay using polling method
 *
 * @param[in] Copy_pvCallBack Pointer to the callback function
 * @param[in] Copy_pu32Tick Pointer to store the elapsed tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 */
ErrorState_t SYSTIC_enumRemainingTickSingleShot(uint32_t * Copy_pu32Tick)
{
    /* Calculate tick time based on clock source */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
double Local_f32TickTimeInUs = 1.0 / (SYSTEM_CLOCK_IN_MHZ / 8.0);
#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
double Local_f32TickTimeInUs = 1.0 / SYSTEM_CLOCK_IN_MHZ;
#endif

  ErrorState_t Local_enumErrorState = OK;

  if (Copy_pu32Tick == NULL)
  {
    Local_enumErrorState = NULL_POINTER;
  }
  else
  {
    *Copy_pu32Tick = (MSYSTIC->VAL) * Local_f32TickTimeInUs;
  }

  return Local_enumErrorState;
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_enumCallback
 * @brief Generate a precise microsecond delay using polling method with callback function
 *
 * @param[in] Copy_pvCallBack Pointer to the callback function
 * @param[in] Copy_u32Tick Pointer to store the elapsed tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual delay may be longer
 *          than requested due to function call overhead
 */
ErrorState_t SYSTIC_enumCallback(void(*Copy_pvCallBack)(void), uint32_t Copy_u32Tick)
{
  /* Configure clock source and set callback function
   * - This function sets up the SysTick timer for continuous operation
   * - The callback function will be called periodically
   */
  ErrorState_t Local_enumErrorState = OK;

  /* Calculate tick time based on clock source */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
  double Local_f32TickTimeInUs = 1.0 / (SYSTEM_CLOCK_IN_MHZ / 8.0);
#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
  double Local_f32TickTimeInUs = 1.0 / SYSTEM_CLOCK_IN_MHZ;
#endif

  /* Calculate required number of ticks */
  uint32_t Local_u32NoOfTicks = Copy_u32Tick / Local_f32TickTimeInUs;

  if (Copy_pvCallBack == NULL)
  {
    Local_enumErrorState = NULL_POINTER;
  }
  else
  {
    SingleShot_Flag = 1;
    SYSTIC_CallBack = Copy_pvCallBack;
    MSYSTIC->VAL = 0;
    MSYSTIC->LOAD = Local_u32NoOfTicks;
    SYSTIC_vEnable();
  }

  return Local_enumErrorState;
}

/*=================================================================================================================*/
/**
 * @fn SYSTIC_enumCallbackSingleShot
 * @brief Generate a precise microsecond delay using polling method with callback function
 *
 * @param[in] Copy_pvCallBack Pointer to the callback function
 * @param[in] Copy_u32Tick Pointer to store the elapsed tick count
 *
 * @details This function:
 *          1. Calculates the number of ticks needed based on clock configuration
 *          2. Handles delays longer than maximum counter value (24-bit) by
 *             breaking them into multiple shorter delays
 *          3. Uses polling method to wait for completion
 *
 * @note The actual delay might be slightly longer than requested due to:
 *       - Function call overhead
 *       - Context switching (if interrupts are enabled)
 *       - Clock frequency rounding
 *
 * @warning For very short delays (<10µs), the actual delay may be longer
 *          than requested due to function call overhead
 */
ErrorState_t SYSTIC_enumCallbackSingleShot(void(*Copy_pvCallBack)(void), uint32_t Copy_u32Tick)
{
  ErrorState_t Local_enumErrorState = OK;

  /* Calculate tick time based on clock source */
#if SYSTIC_CLKSOURCE == CLK_SOURCE_AHB_DIV8
  double Local_f32TickTimeInUs = 1.0 / (SYSTEM_CLOCK_IN_MHZ / 8.0);
#elif SYSTIC_CLKSOURCE == CLK_SOURCE_AHB
  double Local_f32TickTimeInUs = 1.0 / SYSTEM_CLOCK_IN_MHZ;
#endif

  /* Calculate required number of ticks */
  uint32_t Local_u32NoOfTicks = Copy_u32Tick / Local_f32TickTimeInUs;

  if (Copy_pvCallBack == NULL)
  {
    Local_enumErrorState = NULL_POINTER;
  }
  else
  {
    SingleShot_Flag = 0;
    SYSTIC_CallBack = Copy_pvCallBack;
    MSYSTIC->VAL = 0;
    MSYSTIC->LOAD = Local_u32NoOfTicks;
    SYSTIC_vEnable();
  }

  return Local_enumErrorState;
}

/*=================================================================================================================*/
/**
 * @fn SysTick_Handler
 * @brief SysTick interrupt handler
 *
 * @details Automatically called when the counter reaches zero
 *          Clears the COUNTFLAG bit (bit 16 in CTRL register)
 *          Calls the registered callback function if set
 */
void SysTick_Handler(void)
{
  /* SysTick interrupt handler
   * - Automatically called when the counter reaches zero
   * - Clears the COUNTFLAG bit (bit 16 in CTRL register)
   * - Calls the registered callback function if set
   */
  /* Clear COUNTFLAG */
  if (SYSTIC_CallBack != NULL)
  {
    SYSTIC_CallBack();
  }

  if (SingleShot_Flag == 0)
  {
    SYSTIC_vDisable();
  }
}
