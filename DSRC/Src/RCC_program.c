/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    RCC_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : RCC                                             **
 **                                                                           **
 **===========================================================================**
 */

/*******************************************************************************
 *                                Includes                                     *
 *******************************************************************************/

#include <stdint.h>

#include "STM32F446xx.h"
#include "ErrTypes.h"
#include "STD_MACROS.h"

#include "RCC_config.h"
#include "../Inc/RCC_interface.h"
#include "../Inc/RCC_private.h"

/*******************************************************************************
 *                                Global Variables                               *
 *******************************************************************************/
/* Global variable to track RCC driver state (IDLE/BUSY) */
static uint8_t RCC_u8State = IDLE;

/*******************************************************************************
 *                              Functions Definitions                            *
 *******************************************************************************/

/**
 * @fn     RCC_enumSetClkSts
 * @brief  This function controls the state (ON/OFF) of the main clock sources
 *         It handles enabling/disabling of HSI, HSE, and PLL clocks with timeout protection
 * @param  Copy_u8CLK: Clock source to control (HSI_CLK, HSE_CLK, PLL_CLK)
 * @param  Copy_u8Status: Desired clock state (CLK_ON, CLK_OFF)
 * @return ErrorState_t:
 *         - OK: Operation completed successfully
 *         - TIMEOUT_STATE: Clock failed to stabilize within timeout period
 *         - BUSY_STATE: RCC driver is busy with another operation
 * @details
 *         - For HSI: Controls bit 0 in CR register and checks HSIRDY (bit 1)
 *         - For HSE: Controls bit 16 in CR register and checks HSERDY (bit 17)
 *         - For PLL: Controls bit 24 in CR register and checks PLLRDY (bit 25)
 */
ErrorState_t RCC_enumSetClkSts(uint8_t Copy_u8CLK, uint8_t Copy_u8Status)
{
  ErrorState_t Local_u8ErrorState = OK;
  uint32_t Local_u32TimeoutCounter = 0;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    if (Copy_u8CLK == RCC_HSI_CLK)
    {
      /* HSI Clock Control */
      if (Copy_u8Status == RCC_CLK_ON)
      {
        /* Enable HSI and wait for it to be ready */
        SET_BIT(MRCC->CR, 0);
        while (READ_BIT(MRCC->CR, 1) == 0 && Local_u32TimeoutCounter != RCC_u32TIMEOUT)
        {
          Local_u32TimeoutCounter++;
        }

        if (Local_u32TimeoutCounter == RCC_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
      }
      else if (Copy_u8Status == RCC_CLK_OFF)
      {

        CLR_BIT(MRCC->CR, 0);
        while (READ_BIT(MRCC->CR, 1) == 1 && Local_u32TimeoutCounter != RCC_u32TIMEOUT)
        {
          Local_u32TimeoutCounter++;
        }

        if (Local_u32TimeoutCounter == RCC_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
      }
      else
      {
      }
    }
    else if (Copy_u8CLK == RCC_HSE_CLK)
    {
      /* HSE Clock Control */
      if (Copy_u8Status == RCC_CLK_ON)
      {
        /* Enable HSE bypass and HSE clock, then wait for it to be ready */
        SET_BIT(MRCC->CR, 18); /* Enable HSE bypass */
        SET_BIT(MRCC->CR, 16); /* Enable HSE clock */
        while (READ_BIT(MRCC->CR, 17) == 0 && Local_u32TimeoutCounter != RCC_u32TIMEOUT)
        {
          Local_u32TimeoutCounter++;
        }

        if (Local_u32TimeoutCounter == RCC_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
      }

      else if (Copy_u8Status == RCC_CLK_OFF)
      {
        /* Disable HSE clock and wait for it to stop */
        CLR_BIT(MRCC->CR, 16);
        while (READ_BIT(MRCC->CR, 17) == 1 && Local_u32TimeoutCounter != RCC_u32TIMEOUT)
        {
          Local_u32TimeoutCounter++;
        }

        if (Local_u32TimeoutCounter == RCC_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
      }
    }
    else if (Copy_u8CLK == RCC_PLL_CLK)
    {
      /* PLL Clock Control */
      if (Copy_u8Status == RCC_CLK_ON)
      {
        /* Enable PLL and wait for it to be ready */
        SET_BIT(MRCC->CR, 24);
        while (READ_BIT(MRCC->CR, 25) == 0 && Local_u32TimeoutCounter != RCC_u32TIMEOUT)
        {
          Local_u32TimeoutCounter++;
        }

        if (Local_u32TimeoutCounter == RCC_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
      }

      else if (Copy_u8Status == RCC_CLK_OFF)
      {
        /* Disable PLL and wait for it to stop */
        CLR_BIT(MRCC->CR, 24);
        while (READ_BIT(MRCC->CR, 25) == 1 && Local_u32TimeoutCounter != RCC_u32TIMEOUT)
        {
          Local_u32TimeoutCounter++;
        }

        if (Local_u32TimeoutCounter == RCC_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
      }
    }
    else
    {
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}
/*=================================================================================================================*/
/**
 * @fn     RCC_enumSetSysClk
 * @brief  This function configures the system clock source
 *         It allows switching between different clock sources as the main system clock
 * @param  Copy_u8CLK: Clock source to be used as system clock
 *         (HSI_CLK, HSE_CLK, PLLP_CLK, PLLR_CLK)
 * @return RCC_ErrorState:
 *         - RCC_OK: System clock switched successfully
 *         - BUSY_STATE: RCC driver is busy with another operation
 * @details
 *         - Modifies SW bits in CFGR register to select system clock source
 *         - Ensures proper clock switching by clearing and setting appropriate bits
 */
ErrorState_t RCC_enumSetSysClk(uint8_t Copy_u8CLK)
{
  ErrorState_t Local_u8ErrorState = OK;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    switch (Copy_u8CLK)
    {
    case RCC_HSI_CLK:
      /* Select HSI as system clock */
      MRCC->CFGR &= ~(RCC_SYS_CLK_MASK);
      MRCC->CFGR |= Copy_u8CLK;
      break;

    case RCC_HSE_CLK:
      /* Select HSE as system clock */
      MRCC->CFGR &= ~(RCC_SYS_CLK_MASK);
      MRCC->CFGR |= Copy_u8CLK;
      break;

    case RCC_PLLP_CLK:
      /* Select PLL P output as system clock */
      MRCC->CFGR &= ~(RCC_SYS_CLK_MASK);
      MRCC->CFGR |= Copy_u8CLK;
      break;

    case RCC_PLLR_CLK:
      /* Select PLL R output as system clock */
      MRCC->CFGR &= ~(RCC_SYS_CLK_MASK);
      MRCC->CFGR |= Copy_u8CLK;
      break;
    default:
      break;
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @fn     RCC_enumPLLConfig
 * @brief  This function configures the PLL clock parameters
 *         It sets up PLL multiplication and division factors for desired frequency
 * @param  Copy_PLLConfig: Pointer to PLL configuration structure containing:
 *         - PLLSource: Clock source for PLL (HSI or HSE)
 *         - PLLM_Div: Division factor for PLL input (2-63)
 *         - PLLN_Mult: Multiplication factor (50-432)
 *         - PLLP_Div: Division factor for main PLL output (2,4,6,8)
 * @return RCC_ErrorState:
 *         - RCC_OK: PLL configured successfully
 *         - NOK: Invalid configuration parameters
 *         - BUSY_STATE: RCC driver is busy
 * @details
 *         - Configures PLLCFGR register for PLL clock setup
 *         - Validates all configuration parameters before applying
 *         - Ensures proper frequency ranges for stable operation
 */
ErrorState_t RCC_enumPLLConfig(const RCC_PLLConfig_t *Copy_PLLConfig)
{
  ErrorState_t Local_u8ErrorState = OK;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    /* Validate configuration parameters */
    if ((Copy_PLLConfig != NULL) &&
        (Copy_PLLConfig->PLLM_Div >= RCC_MPLL_DIV_2 && Copy_PLLConfig->PLLM_Div <= RCC_MPLL_DIV_63) &&
        (Copy_PLLConfig->PLLN_Mult >= RCC_NPLL_CLK_MULT_50 && Copy_PLLConfig->PLLN_Mult <= RCC_NPLL_CLK_MULT_432) &&
        (Copy_PLLConfig->PLLSource == RCC_PLL_HSI || Copy_PLLConfig->PLLSource == RCC_PLL_HSE) &&
        (Copy_PLLConfig->PLLP_Div >= RCC_PPLL_DIV_2 && Copy_PLLConfig->PLLP_Div <= RCC_PPLL_DIV_8))
    {
      /* Configure PLL source */
      if (Copy_PLLConfig->PLLSource == RCC_PLL_HSI)
      {
        CLR_BIT(MRCC->PLLCFGR, 22);
      }
      else if (Copy_PLLConfig->PLLSource == RCC_PLL_HSE)
      {
        SET_BIT(MRCC->PLLCFGR, 22);
      }

      /* Configure PLL M division factor */
      MRCC->PLLCFGR &= (~(RCC_MPLL_DIV_MASK));
      MRCC->PLLCFGR |= (Copy_PLLConfig->PLLM_Div);

      /* Configure PLL N multiplication factor */
      MRCC->PLLCFGR &= (~(RCC_NPLL_MULT_MASK << 6));
      MRCC->PLLCFGR |= (Copy_PLLConfig->PLLN_Mult << 6);

      /* Configure PLL P division factor */
      MRCC->PLLCFGR &= (~(RCC_PPLL_DIV_MASK << 16));
      MRCC->PLLCFGR |= ((Copy_PLLConfig->PLLP_Div * 2 + 2) << 16);
    }
    else
    {
      Local_u8ErrorState = NOK;
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @fn     RCC_enumAHBConfig
 * @brief  This function configures the AHB bus clock prescaler
 *         It sets the division factor for the AHB clock relative to system clock
 * @param  Copy_u8AHPDiv: AHB prescaler value (AHB_NOT_DIV to AHB_DIV_512)
 * @return RCC_ErrorState:
 *         - RCC_OK: AHB prescaler configured successfully
 *         - NOK: Invalid prescaler value
 *         - BUSY_STATE: RCC driver is busy
 * @details
 *         - Modifies HPRE bits in CFGR register
 *         - Supports division factors: 1,2,4,8,16,64,128,256,512
 */
ErrorState_t RCC_enumAHBConfig(uint8_t Copy_u8AHPDiv)
{
  ErrorState_t Local_u8ErrorState = OK;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    if (Copy_u8AHPDiv >= RCC_AHB_NOT_DIV || Copy_u8AHPDiv <= RCC_AHB_DIV_512)
    {
      /* Configure AHB prescaler in CFGR register */
      MRCC->CFGR &= (~(0xF << 4));
      MRCC->CFGR |= (Copy_u8AHPDiv << 4);
    }
    else
    {
      Local_u8ErrorState = NOK;
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @fn     RCC_enumAPB1Config
 * @brief  This function configures the APB1 bus clock prescaler
 *         It sets the division factor for the APB1 clock relative to AHB clock
 * @param  Copy_u8APB1Div: APB1 prescaler value (APB_NOT_DIV to APB_DIV_16)
 * @return RCC_ErrorState:
 *         - RCC_OK: APB1 prescaler configured successfully
 *         - NOK: Invalid prescaler value
 *         - BUSY_STATE: RCC driver is busy
 * @details
 *         - Modifies PPRE1 bits in CFGR register
 *         - Supports division factors: 1,2,4,8,16
 */
ErrorState_t RCC_enumAPB1Config(uint8_t Copy_u8APB1Div)
{
  ErrorState_t Local_u8ErrorState = OK;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    if (Copy_u8APB1Div >= RCC_APB_NOT_DIV || Copy_u8APB1Div <= RCC_APB_DIV_16)
    {
      /* Configure APB1 prescaler in CFGR register */
      MRCC->CFGR &= (~(0x7 << 10));
      MRCC->CFGR |= (Copy_u8APB1Div << 10);
    }
    else
    {
      Local_u8ErrorState = NOK;
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @fn     RCC_enumAPB2Config
 * @brief  This function configures the APB2 bus clock prescaler
 *         It sets the division factor for the APB2 clock relative to AHB clock
 * @param  Copy_u8APB2Div: APB2 prescaler value (APB_NOT_DIV to APB_DIV_16)
 * @return RCC_ErrorState:
 *         - RCC_OK: APB2 prescaler configured successfully
 *         - NOK: Invalid prescaler value
 *         - BUSY_STATE: RCC driver is busy
 * @details
 *         - Modifies PPRE2 bits in CFGR register
 *         - Supports division factors: 1,2,4,8,16
 */
ErrorState_t RCC_enumAPB2Config(uint8_t Copy_u8APB2Div)
{
  ErrorState_t Local_u8ErrorState = OK;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    if (Copy_u8APB2Div >= RCC_APB_NOT_DIV || Copy_u8APB2Div <= RCC_APB_DIV_16)
    {
      /* Configure APB2 prescaler in CFGR register */
      MRCC->CFGR &= (~(0x7 << 13));
      MRCC->CFGR |= (Copy_u8APB2Div << 13);
    }
    else
    {
      Local_u8ErrorState = NOK;
    }
    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @fn     RCC_enumAHPPerSts
 * @brief  This function controls the clock enable/disable for peripherals on AHB buses
 *         It manages peripheral clock gating for power optimization
 * @param  Copy_u8Bus: AHB bus number (AHB1, AHB2, AHB3)
 * @param  Copy_u8AHPPer: Peripheral number on the selected bus
 * @param  Copy_u8Status: Peripheral clock status (PER_ON, PER_OFF)
 * @return RCC_ErrorState:
 *         - RCC_OK: Peripheral clock state changed successfully
 *         - BUSY_STATE: RCC driver is busy
 * @details
 *         - Controls bits in AHB1ENR, AHB2ENR, AHB3ENR registers
 *         - Enables/disables clock for specific peripherals on AHB buses
 */
ErrorState_t RCC_enumAHPPerSts(uint8_t Copy_u8Bus, uint8_t Copy_u8AHPPer, uint8_t Copy_u8Status)
{
  ErrorState_t Local_u8ErrorState = OK;
  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;

    switch (Copy_u8Bus)
    {
    case RCC_AHB1:
      /* Configure AHB1 peripheral clock */
      if (Copy_u8Status == RCC_PER_ON)
      {
        SET_BIT(MRCC->AHP1ENR, Copy_u8AHPPer);
      }
      else if (Copy_u8Status == RCC_PER_OFF)
      {
        CLR_BIT(MRCC->AHP1ENR, Copy_u8AHPPer);
      }
      break;
    case RCC_AHB2:
      /* Configure AHB2 peripheral clock */
      if (Copy_u8Status == RCC_PER_ON)
      {
        SET_BIT(MRCC->AHP2ENR, Copy_u8AHPPer);
      }
      else if (Copy_u8Status == RCC_PER_OFF)
      {
        CLR_BIT(MRCC->AHP2ENR, Copy_u8AHPPer);
      }
      break;
    case RCC_AHB3:
      /* Configure AHB3 peripheral clock */
      if (Copy_u8Status == RCC_PER_ON)
      {
        SET_BIT(MRCC->AHP3ENR, Copy_u8AHPPer);
      }
      else if (Copy_u8Status == RCC_PER_OFF)
      {
        CLR_BIT(MRCC->AHP3ENR, Copy_u8AHPPer);
      }
      break;
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}

/*=================================================================================================================*/
/**
 * @fn     RCC_enumABPPerSts
 * @brief  This function controls the clock enable/disable for peripherals on APB buses
 *         It manages peripheral clock gating for power optimization
 * @param  Copy_u8Bus: APB bus number (APB1, APB2)
 * @param  Copy_u8AHPPer: Peripheral number on the selected bus
 * @param  Copy_u8Status: Peripheral clock status (PER_ON, PER_OFF)
 * @return RCC_ErrorState:
 *         - RCC_OK: Peripheral clock state changed successfully
 *         - BUSY_STATE: RCC driver is busy
 * @details
 *         - Controls bits in APB1ENR and APB2ENR registers
 *         - Enables/disables clock for specific peripherals on APB buses
 */
ErrorState_t RCC_enumABPPerSts(uint8_t Copy_u8Bus, uint8_t Copy_u8ABPPer, uint8_t Copy_u8Status)
{
  ErrorState_t Local_u8ErrorState = OK;

  if (RCC_u8State == IDLE)
  {
    RCC_u8State = BUSY;
    switch (Copy_u8Bus)
    {
    case RCC_APB1:
      if (Copy_u8Status == RCC_PER_ON)
      {
        SET_BIT(MRCC->APB1ENR, Copy_u8ABPPer);
      }
      else if (Copy_u8Status == RCC_PER_OFF)
      {
        CLR_BIT(MRCC->APB1ENR, Copy_u8ABPPer);
      }
      else
      {
      }
      break;
    case RCC_APB2:
      if (Copy_u8Status == RCC_PER_ON)
      {
        SET_BIT(MRCC->APB2ENR, Copy_u8ABPPer);
      }
      else if (Copy_u8Status == RCC_PER_OFF)
      {
        CLR_BIT(MRCC->APB2ENR, Copy_u8ABPPer);
      }
      else
      {
      }
      break;

    default:
      break;
    }

    RCC_u8State = IDLE;
  }
  else
  {
    Local_u8ErrorState = BUSY_STATE;
  }

  return Local_u8ErrorState;
}
