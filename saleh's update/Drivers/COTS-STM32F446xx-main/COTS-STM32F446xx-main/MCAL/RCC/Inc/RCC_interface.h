/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<   RCC_interface.h   >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : RCC                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef MCAL_RCC_INTERFACE_H_
#define MCAL_RCC_INTERFACE_H_

#include "../../../LIB/ErrTypes.h"
#include "../../../LIB/STM32F446xx.h"
#include <stdint.h>


/************************** Bus Definitions **************************/
/********************************
 * @uC_BUS_t enum:
 * @brief: Microcontroller bus selection
 * @param: AHB1, AHB2, AHB3 - Advanced High-performance Bus
 *         APB1, APB2 - Advanced Peripheral Bus
 * @return: Selected bus type
 */
typedef enum {
  RCC_AHB1, /* Advanced High-performance Bus 1 */
  RCC_AHB2, /* Advanced High-performance Bus 2 */
  RCC_AHB3, /* Advanced High-performance Bus 3 */
  RCC_APB1, /* Advanced Peripheral Bus 1 */
  RCC_APB2  /* Advanced Peripheral Bus 2 */
} RCC_uC_BUS_t;

/********************************
 * @AHB1_BUS_t enum:
 * @brief: Peripherals connected to AHB1 bus
 * @details: Contains GPIO ports A-H, CRC, Backup SRAM,
 *          DMA1/2, USB OTG HS and ULPI
 */
typedef enum {
  RCC_GPIOAEN,          /* GPIO Port A Enable */
  RCC_GPIOBEN,          /* GPIO Port B Enable */
  RCC_GPIOCEN,          /* GPIO Port C Enable */
  RCC_GPIODEN,          /* GPIO Port D Enable */
  RCC_GPIOEEN,          /* GPIO Port E Enable */
  RCC_GPIOFEN,          /* GPIO Port F Enable */
  RCC_GPIOGEN,          /* GPIO Port G Enable */
  RCC_GPIOHEN,          /* GPIO Port H Enable */
  RCC_CRCEN = 12,       /* CRC Enable */
  RCC_BKPSRAMEN = 18,   /* Backup SRAM Enable */
  RCC_DMA1EN = 21,      /* DMA1 Enable */
  RCC_DMA2EN = 22,      /* DMA2 Enable */
  RCC_OTGHSEN = 29,     /* USB OTG HS Enable */
  RCC_OTGHSULPIEN = 30, /* USB OTG ULPI Enable */
} RCC_AHB1_BUS_t;

/********************************
 * @AHB2_BUS_t enum:
 * @brief: Peripherals connected to AHB2 bus
 * @details: Contains DCMI and USB OTG FS
 */
typedef enum {
  RCC_DCMIEN = 0,  /* Digital Camera Interface Enable */
  RCC_OTGFSEN = 7, /* USB OTG FS Enable */
} RCC_AHB2_BUS_t;

/********************************
 * @AHB3_BUS_t enum:
 * @brief: Peripherals connected to AHB3 bus
 * @details: Contains FMC and QSPI
 */
typedef enum {
  RCC_FMCEN = 0,  /* Flexible Memory Controller Enable */
  RCC_QSPIEN = 1, /* Quad SPI Enable */
} RCC_AHB3_BUS_t;

/********************************
 * @APB2_BUS_t enum:
 * @brief: Peripherals connected to APB2 bus
 * @details: Contains timers, USART1/6, ADCs, SDIO,
 *          SPI1/4, SYSCFG, and SAI1/2
 */
typedef enum {
  RCC_TIM1EN = 0,  /* Timer 1 Enable */
  RCC_TIM8EN,      /* Timer 8 Enable */
  RCC_USART1 = 4,  /* USART1 Enable */
  RCC_USART6,      /* USART6 Enable */
  RCC_ADC1EN = 8,  /* ADC1 Enable */
  RCC_ADC2EN,      /* ADC2 Enable */
  RCC_ADC3EN,      /* ADC3 Enable */
  RCC_SDIOEN,      /* SDIO Enable */
  RCC_SPI1EN,      /* SPI1 Enable */
  RCC_SPI4EN,      /* SPI4 Enable */
  RCC_SYSCFGEN,    /* System Configuration Enable */
  RCC_TIM9EN = 16, /* Timer 9 Enable */
  RCC_TIM10EN,     /* Timer 10 Enable */
  RCC_TIM11EN,     /* Timer 11 Enable */
  RCC_SAI1EN = 22, /* SAI1 Enable */
  RCC_SAI2EN,      /* SAI2 Enable */
} RCC_APB2_BUS_t;

/********************************
 * @APB1_BUS_t enum:
 * @brief: Peripherals connected to APB1 bus
 * @details: Contains timers, watchdog, SPI2/3, USART2-5,
 *          I2C, CAN, CEC, PWR and DAC
 */
typedef enum {
  RCC_TIM2EN,      /* Timer 2 Enable */
  RCC_TIM3EN,      /* Timer 3 Enable */
  RCC_TIM4EN,      /* Timer 4 Enable */
  RCC_TIM5EN,      /* Timer 5 Enable */
  RCC_TIM6EN,      /* Timer 6 Enable */
  RCC_TIM7EN,      /* Timer 7 Enable */
  RCC_TIM12EN,     /* Timer 12 Enable */
  RCC_TIM13EN,     /* Timer 13 Enable */
  RCC_TIM14EN,     /* Timer 14 Enable */
  RCC_WWDGEN = 11, /* Window Watchdog Enable */
  RCC_SPI2EN = 14, /* SPI2 Enable */
  RCC_SPI3EN,      /* SPI3 Enable */
  RCC_SPDIFRX,     /* SPDIF-RX Enable */
  RCC_USART2EN,    /* USART2 Enable */
  RCC_USART3EN,    /* USART3 Enable */
  RCC_USART4EN,    /* USART4 Enable */
  RCC_USART5EN,    /* USART5 Enable */
  RCC_I2C1EN,      /* I2C1 Enable */
  RCC_I2C2EN,      /* I2C2 Enable */
  RCC_I2C3EN,      /* I2C3 Enable */
  RCC_FMPI2C1EN,   /* FMPI2C1 Enable */
  RCC_CAN1EN,      /* CAN1 Enable */
  RCC_CAN2EN,      /* CAN2 Enable */
  RCC_CECEN,       /* HDMI-CEC Enable */
  RCC_PWREN,       /* Power Interface Enable */
  RCC_DACEN,       /* DAC Enable */
} RCC_APB1_BUS_t;

/********************************
 * @AHB_BUS_DIV_t enum:
 * @brief: AHB bus clock division factors
 * @details: Defines division ratios from 1 to 512
 */
typedef enum {
  RCC_AHB_NOT_DIV = 7, /* No Division */
  RCC_AHB_DIV_2,       /* Divide by 2 */
  RCC_AHB_DIV_4,       /* Divide by 4 */
  RCC_AHB_DIV_8,       /* Divide by 8 */
  RCC_AHB_DIV_16,      /* Divide by 16 */
  RCC_AHB_DIV_64,      /* Divide by 64 */
  RCC_AHB_DIV_128,     /* Divide by 128 */
  RCC_AHB_DIV_256,     /* Divide by 256 */
  RCC_AHB_DIV_512,     /* Divide by 512 */
} RCC_AHB_BUS_DIV_t;

/********************************
 * @APB_BUS_DIV_t enum:
 * @brief: APB bus clock division factors
 * @details: Defines division ratios from 1 to 16
 */
typedef enum {
  RCC_APB_NOT_DIV = 3, /* No Division */
  RCC_APB_DIV_2,       /* Divide by 2 */
  RCC_APB_DIV_4,       /* Divide by 4 */
  RCC_APB_DIV_8,       /* Divide by 8 */
  RCC_APB_DIV_16,      /* Divide by 16 */
} RCC_APB_BUS_DIV_t;

/********************************
 * @brief: Main PLL division factors
 * @details: Defines minimum and maximum division values
 */
#define RCC_MPLL_DIV_2 2   /* Minimum PLL division */
#define RCC_MPLL_DIV_63 63 /* Maximum PLL division */

/********************************
 * @brief: NPLL multiplication factors
 * @details: Defines minimum and maximum multiplication values
 */
#define RCC_NPLL_CLK_MULT_50 50   /* Minimum PLL multiplication */
#define RCC_NPLL_CLK_MULT_432 432 /* Maximum PLL multiplication */

/********************************
 * @PPLL_CLK_DIV enum:
 * @brief: PLLP clock division factors
 * @details: Defines even division ratios from 2 to 8
 */
typedef enum {
  RCC_PPLL_DIV_2, /* Divide by 2 */
  RCC_PPLL_DIV_4, /* Divide by 4 */
  RCC_PPLL_DIV_6, /* Divide by 6 */
  RCC_PPLL_DIV_8, /* Divide by 8 */
} RCC_PPLL_CLK_DIV_t;

/********************************
 * @PLL_CLK_SRC enum:
 * @brief: PLL clock source selection
 * @details: Selects between HSI and HSE as PLL input
 */
typedef enum {
  RCC_PLL_HSI, /* High Speed Internal Clock Source */
  RCC_PLL_HSE, /* High Speed External Clock Source */
} RCC_PLL_CLK_SRC_t;

/************************** Clock Source Definitions **************************/
/********************************
 * @CLK_SRC_t enum:
 * @brief: Clock source selection for system clock
 * @param: HSI_CLK - High Speed Internal Clock
 *         HSE_CLK - High Speed External Clock
 *         PLLP_CLK - PLL P output Clock
 *         PLLR_CLK - PLL R output Clock
 *         PLL_CLK - Phase Locked Loop Clock
 * @return: Selected clock source
 */
typedef enum {
  RCC_HSI_CLK, /* High Speed Internal Clock (16 MHz RC oscillator) */
  RCC_HSE_CLK, /* High Speed External Clock (4-26 MHz crystal/ceramic resonator)
                */
  RCC_PLLP_CLK, /* Main PLL P output Clock */
  RCC_PLLR_CLK, /* Main PLL R output Clock */
  RCC_PLL_CLK   /* Phase Locked Loop Clock */
} RCC_CLK_SRC_t;

/************************** Clock Enable/Disable Definitions
 * **************************/
/********************************
 * @CLK_EN_t enum:
 * @brief: Clock enable/disable selection
 * @param: CLK_ON - Enable selected clock
 *         CLK_OFF - Disable selected clock
 * @return: Clock state selection
 */
typedef enum {
  RCC_CLK_ON, /* Enable the selected clock */
  RCC_CLK_OFF /* Disable the selected clock */
} RCC_CLK_EN_t;

/************************** Peripheral Enable/Disable Definitions
 * **************************/
/********************************
 * @PER_EN_t enum:
 * @brief: Peripheral enable/disable selection
 * @param: PER_ON - Enable selected peripheral
 *         PER_OFF - Disable selected peripheral
 * @return: Peripheral state selection
 */
typedef enum {
  RCC_PER_ON, /* Enable the selected peripheral */
  RCC_PER_OFF /* Disable the selected peripheral */
} RCC_PER_EN_t;

/************************** PLL Configuration Structure
 * **************************/
/********************************
 * @RCC_PLLConfig_t struct:
 * @brief: PLL configuration parameters structure
 * @param: PLLSource - PLL clock source (HSI or HSE)
 *         PLLM_Div - PLL input division factor (2-63)
 *         PLLN_Mult - PLL multiplication factor (50-432)
 *         PLLP_Div - PLL output division factor (2,4,6,8)
 * @return: None
 */
typedef struct {
  RCC_PLL_CLK_SRC_t PLLSource; /* PLL clock source (PLL_HSI, PLL_HSE) */
  uint8_t PLLM_Div;            /* PLL M divider (MPLL_DIV_2 to MPLL_DIV_63) */
  uint8_t
      PLLN_Mult; /* PLL N multiplier (NPLL_CLK_MULT_50 to NPLL_CLK_MULT_432) */
  RCC_PPLL_CLK_DIV_t PLLP_Div; /* PLL P divider (PPLL_DIV_2 to PPLL_DIV_8) */
} RCC_PLLConfig_t;

/************************** Function Prototypes **************************/
/**
 * @fn     RCC_enumSetClkSts
 * @brief  Enable or disable a specific clock source
 * @param  Copy_u8CLK: Clock source (HSI_CLK, HSE_CLK, PLL_CLK)
 * @param  Copy_u8Status: Clock status (CLK_ON, CLK_OFF)
 * @return ErrorState_t: Error status (RCC_OK, TIMEOUT_STATE, BUSY_STATE)
 * @example RCC_enumSetClkSts(RCC_HSE_CLK, RCC_CLK_ON);
 */
ErrorState_t RCC_enumSetClkSts(uint8_t Copy_u8CLK, uint8_t Copy_u8Status);

/**
 * @fn     RCC_enumSetSysClk
 * @brief  Set the system clock source
 * @param  Copy_u8CLK: Clock source (HSI_CLK, HSE_CLK, PLLP_CLK, PLLR_CLK)
 * @return ErrorState_t: Error status (RCC_OK, BUSY_STATE)
 * @example RCC_enumSetSysClk(RCC_HSE_CLK);
 */
ErrorState_t RCC_enumSetSysClk(uint8_t Copy_u8CLK);

/**
 * @fn     RCC_enumPLLConfig
 * @brief  Configure PLL parameters
 * @param  Copy_PLLConfig: Pointer to PLL configuration structure
 * @return ErrorState_t: Error status (RCC_OK, NOK, BUSY_STATE)
 * @example 
 * RCC_PLLConfig_t MyPLL = {RCC_PLL_HSE, 8, 336, RCC_PPLL_DIV_2};
 * RCC_enumPLLConfig(&MyPLL);
 */
ErrorState_t RCC_enumPLLConfig(const RCC_PLLConfig_t *Copy_PLLConfig);

/**
 * @fn     RCC_enumAHBConfig
 * @brief  Configure AHB bus clock prescaler
 * @param  Copy_u8AHPDiv: AHB prescaler value (AHB_NOT_DIV to AHB_DIV_512)
 * @return ErrorState_t: Error status (RCC_OK, NOK, BUSY_STATE)
 * @example RCC_enumAHBConfig(RCC_AHB_NOT_DIV);
 */
ErrorState_t RCC_enumAHBConfig(uint8_t Copy_u8AHPDiv);

/**
 * @fn     RCC_enumAPB1Config
 * @brief  Configure APB1 bus clock prescaler
 * @param  Copy_u8APB1Div: APB1 prescaler value (APB_NOT_DIV to APB_DIV_16)
 * @return ErrorState_t: Error status (RCC_OK, NOK, BUSY_STATE)
 * @example RCC_enumAPB1Config(RCC_APB_DIV_2);
 */
ErrorState_t RCC_enumAPB1Config(uint8_t Copy_u8APB1Div);

/**
 * @fn     RCC_enumAPB2Config
 * @brief  Configure APB2 bus clock prescaler
 * @param  Copy_u8APB2Div: APB2 prescaler value (APB_NOT_DIV to APB_DIV_16)
 * @return ErrorState_t: Error status (RCC_OK, NOK, BUSY_STATE)
 * @example RCC_enumAPB2Config(RCC_APB_NOT_DIV);
 */
ErrorState_t RCC_enumAPB2Config(uint8_t Copy_u8APB2Div);

/**
 * @fn     RCC_enumAHPPerSts
 * @brief  Enable or disable peripheral clock on AHB buses
 * @param  Copy_u8Bus: AHB bus number (RCC_AHB1, RCC_AHB2, RCC_AHB3)
 * @param  Copy_u8AHPPer: Peripheral number on the selected bus (e.g., RCC_GPIOAEN)
 * @param  Copy_u8Status: Peripheral clock status (RCC_PER_ON, RCC_PER_OFF)
 * @return ErrorState_t: Error status (RCC_OK, BUSY_STATE)
 * @example RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOAEN, RCC_PER_ON);
 */
ErrorState_t RCC_enumAHPPerSts(uint8_t Copy_u8Bus, uint8_t Copy_u8AHPPer,
                               uint8_t Copy_u8Status);

/**
 * @fn     RCC_enumABPPerSts
 * @brief  Enable or disable peripheral clock on APB buses
 * @param  Copy_u8Bus: APB bus number (RCC_APB1, RCC_APB2)
 * @param  Copy_u8AHPPer: Peripheral number on the selected bus (e.g., RCC_USART2EN)
 * @param  Copy_u8Status: Peripheral clock status (RCC_PER_ON, RCC_PER_OFF)
 * @return ErrorState_t: Error status (RCC_OK, BUSY_STATE)
 * @example RCC_enumABPPerSts(RCC_APB1, RCC_USART2EN, RCC_PER_ON);
 */
ErrorState_t RCC_enumABPPerSts(uint8_t Copy_u8Bus, uint8_t Copy_u8AHPPer,
                               uint8_t Copy_u8Status);

#endif /* MCAL_RCC_INTERFACE_H_ */
