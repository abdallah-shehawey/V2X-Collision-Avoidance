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

#include <stdint.h>
#include "STM32F446xx.h"
#include "ErrTypes.h"

#include "SYSTIC_config.h"
#include "SYSTIC_private.h"
#include "SYSTIC_interface.h"


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
  /* Configure clock source */
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
  /* Calculate tick time based on clock source */
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
  /* Calculate tick time based on clock source */
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
