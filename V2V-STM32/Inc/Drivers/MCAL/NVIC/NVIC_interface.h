/**
 ******************************************************************************
 * @file    NVIC_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the NVIC driver — the nested vectored interrupt controller.
 * @ingroup mcal_nvic
 ******************************************************************************
 */
#ifndef NVIC_INTERFACE_H_
#define NVIC_INTERFACE_H_

#include "stdint.h"
#include "../../LIB/ErrTypes.h"

/**
 * @brief Interrupt (IRQ) numbers of the STM32F446RE, as laid out in the vector table.
 *
 * The value of each enumerator *is* its position in the vector table, which is
 * what the NVIC driver uses to index its register arrays: IRQ @e n lives in bit
 * `n % 32` of word `n / 32`.
 *
 * @note Some vectors are **shared**. @ref NVIC_EXTI9_5 and @ref NVIC_EXTI15_10
 *       each cover a whole range of EXTI lines, so their handlers have to read
 *       the pending register to find out which line actually fired. The six
 *       ultrasonic echo pins fall into these shared vectors.
 * @warning An interrupt whose handler calls a FreeRTOS `...FromISR` function must
 *          be given a priority number of 6 or higher — see the warning on
 *          @ref sys_tasks. @ref NVIC_USART1 is the one this applies to.
 */
typedef enum
{
  NVIC_WWDGEN = 0,         /**< Window watchdog early-wakeup. */
  NVIC_PVDEN,              /**< Programmable voltage detector, through EXTI line 16. */
  NVIC_TAMP_STAMP,         /**< RTC tamper and timestamp, through EXTI line 21. */
  NVIC_RTC_WAKEUP,         /**< RTC wakeup timer, through EXTI line 22. */
  NVIC_FLASH,              /**< Flash memory global interrupt. */
  NVIC_RCC,                /**< RCC global interrupt (clock-ready and clock-security events). */
  NVIC_EXTI0,              /**< EXTI line 0 — pin 0 of the selected port. */
  NVIC_EXTI1,              /**< EXTI line 1. */
  NVIC_EXTI2,              /**< EXTI line 2. */
  NVIC_EXTI3,              /**< EXTI line 3. */
  NVIC_EXTI4,              /**< EXTI line 4. */
  NVIC_DMA1_STREAM0,       /**< DMA1 stream 0. */
  NVIC_DMA1_STREAM1,       /**< DMA1 stream 1. */
  NVIC_DMA1_STREAM2,       /**< DMA1 stream 2. */
  NVIC_DMA1_STREAM3,       /**< DMA1 stream 3. */
  NVIC_DMA1_STREAM4,       /**< DMA1 stream 4. */
  NVIC_DMA1_STREAM5,       /**< DMA1 stream 5. */
  NVIC_DMA1_STREAM6,       /**< DMA1 stream 6. */
  NVIC_ADC,                /**< ADC1, ADC2 and ADC3, sharing one vector. */
  NVIC_CAN1_TX,            /**< CAN1 transmit mailbox empty. */
  NVIC_CAN1_RX0,           /**< CAN1 receive FIFO 0. */
  NVIC_CAN1_RX1,           /**< CAN1 receive FIFO 1. */
  NVIC_CAN1_SCE,           /**< CAN1 status change and error. */
  NVIC_EXTI9_5,            /**< EXTI lines 5 to 9, sharing one vector — the handler must check which line fired. */
  NVIC_TIM1_BRK_TIM9,      /**< TIM1 break, and TIM9 global. */
  NVIC_TIM1_UP_TIM10,      /**< TIM1 update, and TIM10 global. */
  NVIC_TIM1_TRG_COM_TIM11, /**< TIM1 trigger and commutation, and TIM11 global. */
  NVIC_TIM1_CC,            /**< TIM1 capture/compare. */
  NVIC_TIM2,               /**< TIM2 global — times the front-left and front-centre ultrasonic echoes. */
  NVIC_TIM3,               /**< TIM3 global — times the front-right and the three rear ultrasonic echoes. */
  NVIC_TIM4,               /**< TIM4 global. */
  NVIC_I2C1_EV,            /**< I2C1 event. */
  NVIC_I2C1_ER,            /**< I2C1 error. */
  NVIC_I2C2_EV,            /**< I2C2 event. */
  NVIC_I2C2_ER,            /**< I2C2 error. */
  NVIC_SPI1,               /**< SPI1 global — the MPU9250 link. */
  NVIC_SPI2,               /**< SPI2 global. */
  NVIC_USART1,             /**< USART1 global — the ESP32 V2V link. Its priority must be 6 or higher, since its ISR calls FreeRTOS `...FromISR` functions. */
  NVIC_USART2,             /**< USART2 global — the Raspberry Pi telemetry link. */
  NVIC_USART3,             /**< USART3 global. */
  NVIC_EXTI15_10,          /**< EXTI lines 10 to 15, sharing one vector. */
  NVIC_RTC_ALARM,          /**< RTC alarms A and B, through EXTI line 17. */
  NVIC_OTG_FS_WKUP,        /**< USB OTG full-speed wakeup, through EXTI line 18. */
  NVIC_DMA1_STREAM7 = 47,  /**< DMA1 stream 7. */
  NVIC_SDIO = 49,          /**< SDIO global. */
  NVIC_TIM5,               /**< TIM5 global. */
  NVIC_SPI3,               /**< SPI3 global. */
  NVIC_USART4,             /**< UART4 global. */
  NVIC_USART5,             /**< UART5 global. */
  NVIC_TIM6_DAC,           /**< TIM6 global, and the DAC underrun. */
  NVIC_TIM7,               /**< TIM7 global. */
  NVIC_DMA2_STREAM0,       /**< DMA2 stream 0. */
  NVIC_DMA2_STREAM1,       /**< DMA2 stream 1. */
  NVIC_DMA2_STREAM2,       /**< DMA2 stream 2. */
  NVIC_DMA2_STREAM3,       /**< DMA2 stream 3. */
  NVIC_DMA2_STREAM4,       /**< DMA2 stream 4. */
  NVIC_OTG_FS = 67,        /**< USB OTG full-speed global. */
  NVIC_DMA2_STREAM5,       /**< DMA2 stream 5. */
  NVIC_DMA2_STREAM6,       /**< DMA2 stream 6. */
  NVIC_DMA2_STREAM7,       /**< DMA2 stream 7. */
  NVIC_USART6,             /**< USART6 global. */
  NVIC_I2C3_EV,            /**< I2C3 event. */
  NVIC_I2C3_ER,            /**< I2C3 error. */
  NVIC_FPU = 81,           /**< Floating-point unit exception. */
  NVIC_SPI4 = 84,          /**< SPI4 global. */
  NVIC_SPI5,               /**< SPI5 global. */
  NVIC_QUADSPI = 92,       /**< QuadSPI global. */
  NVIC_FMPI2C1_EV = 95,    /**< Fast-mode-plus I2C1 event. */
  NVIC_FMPI2C1_ER          /**< Fast-mode-plus I2C1 error. */
} NVIC_IRQNumber_t;

/**
 * @brief Enable an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to enable.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 * @par Example:
 * NVIC_vEnableIRQ(NVIC_EXTI0);
 */
ErrorState_t NVIC_vEnableIRQ(uint8_t Copy_u8IRQNumber);
/**
 * @brief Disable an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to disable.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 * @par Example:
 * NVIC_vDisableIRQ(NVIC_EXTI0);
 */
ErrorState_t NVIC_vDisableIRQ(uint8_t Copy_u8IRQNumber);
/**
 * @brief Set pending flag for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to set pending flag.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 * @par Example:
 * NVIC_vSetPendingFlag(NVIC_EXTI0);
 */
ErrorState_t NVIC_vSetPendingFlag(uint8_t Copy_u8IRQNumber);
/**
 * @brief Clear pending flag for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to clear pending flag.
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number is invalid.
 * @par Example:
 * NVIC_vClearPendingFlag(NVIC_EXTI0);
 */
ErrorState_t NVIC_vClearPendingFlag(uint8_t Copy_u8IRQNumber);
/**
 * @brief Get active flag status for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to check active flag.
 * @param Copy_pu8Flag Pointer to store active flag status (0 or 1).
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number invalid, or NULL_POINTER if pointer is NULL.
 * @par Example:
 * 
 * uint8_t flag;
 * NVIC_vGetActiveFlag(NVIC_EXTI0, &flag);
 */
ErrorState_t NVIC_vGetActiveFlag(uint8_t Copy_u8IRQNumber, uint8_t *Copy_pu8Flag);
/**
 * @brief Set priority for an IRQ number in the NVIC.
 * @param Copy_u8IRQNumber IRQ number to set priority.
 * @param Copy_u8Priority Priority value (0 to 15).
 * @return ErrorState_t Returns OK if successful, NOK if IRQ number or priority is invalid.
 * @par Example:
 * NVIC_vSetPriority(NVIC_EXTI0, 2);
 */
ErrorState_t NVIC_vSetPriority(uint8_t Copy_u8IRQNumber, uint8_t Copy_u8Priority);

#endif /* NVIC_INTERFACE_H_ */
