/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    NVIC_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : NVIC                                            **
 **                                                                           **
 **===========================================================================**
 */
#ifndef NVIC_INTERFACE_H_
#define NVIC_INTERFACE_H_

#include "stdint.h"


typedef enum
{
  NVIC_WWDGEN,
  NVIC_PVDEN,
  NVIC_TAMP_STAMP,
  NVIC_RTC_WAKEUP,
  NVIC_FLASH,
  NVIC_RCC,
  NVIC_EXTI0,
  NVIC_EXTI1,
  NVIC_EXTI2,
  NVIC_EXTI3,
  NVIC_EXTI4,
  NVIC_DMA1_STREAM0,
  NVIC_DMA1_STREAM1,
  NVIC_DMA1_STREAM2,
  NVIC_DMA1_STREAM3,
  NVIC_DMA1_STREAM4,
  NVIC_DMA1_STREAM5,
  NVIC_DMA1_STREAM6,
  NVIC_ADC,
  NVIC_CAN1_TX,
  NVIC_CAN1_RX0,
  NVIC_CAN1_RX1,
  NVIC_CAN1_SCE,
  NVIC_EXTI9_5,
  NVIC_TIM1_BRK_TIM9,
  NVIC_TIM1_UP_TIM10,
  NVIC_TIM1_TRG_COM_TIM11,
  NVIC_TIM1_CC,
  NVIC_TIM2,
  NVIC_TIM3,
  NVIC_TIM4,
  NVIC_I2C1_EV,
  NVIC_I2C1_ER,
  NVIC_I2C2_EV,
  NVIC_I2C2_ER,
  NVIC_SPI1,
  NVIC_SPI2,
  NVIC_USART1,
  NVIC_USART2,
  NVIC_USART3,
  NVIC_EXTI15_10,
  NVIC_RTC_ALARM,
  NVIC_OTG_FS_WKUP,
  NVIC_DMA1_STREAM7 = 47,
  NVIC_SDIO = 49,
  NVIC_TIM5,
  NVIC_SPI3,
  NVIC_DMA2_STREAM0 = 56,
  NVIC_DMA2_STREAM1,
  NVIC_DMA2_STREAM2,
  NVIC_DMA2_STREAM3,
  NVIC_DMA2_STREAM4,
  NVIC_OTG_FS = 67,
  NVIC_DMA2_STREAM5,
  NVIC_DMA2_STREAM6,
  NVIC_DMA2_STREAM7,
  NVIC_USART6,
  NVIC_I2C3_EV,
  NVIC_I2C3_ER,
  NVIC_FPU = 81,
  NVIC_SPI4 = 84,
  NVIC_SPI5,
  NVIC_QUADSPI = 92,
  NVIC_FMPI2C1_EV = 95,
  NVIC_FMPI2C1_ER
} NVIC_IRQNumber_t;

/**
 * @brief Enable an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to enable.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vEnableIRQ(uint8_t Copy_u8IRQNumber);
/**
 * @brief Disable an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to disable.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vDisableIRQ(uint8_t Copy_u8IRQNumber);
/**
 * @brief Set pending flag for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to set pending flag.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vSetPendingFlag(uint8_t Copy_u8IRQNumber);
/**
 * @brief Clear pending flag for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to clear pending flag.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 */
ErrorState_t NVIC_vClearPendingFlag(uint8_t Copy_u8IRQNumber);
/**
 * @brief Get active flag status for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to check active flag.
 * @param Copy_pu8Flag Pointer to store active flag status (0 or 1).
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number invalid, or NULL_POINTER if pointer is NULL.
 */
ErrorState_t NVIC_vGetActiveFlag(uint8_t Copy_u8IRQNumber, uint8_t *Copy_pu8Flag);
/**
 * @brief Set priority for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to set priority.
 * @param Copy_u8Priority Priority value (0 to 15).
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number or priority is invalid.
 */
ErrorState_t NVIC_vSetPriority(uint8_t Copy_u8IRQNumber, uint8_t Copy_u8Priority);

#endif /* NVIC_INTERFACE_H_ */
