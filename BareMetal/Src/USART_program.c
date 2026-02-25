/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    USART_program.c >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : USART                                           **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Drivers/LIB/STM32F446xx.h"
#include <stdint.h>

#include "../Inc/Drivers/LIB/ErrTypes.h"


#include "../Inc/Drivers/MCAL/USART/USART_config.h"
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Drivers/MCAL/USART/USART_private.h"

/* Array of USART port register definitions for easy access */
static USART_RegDef_t *USART_Channel[USART_CHANNEL_COUNT] = {
    MUSART1, MUSART2, MUSART3, MUSART4, MUSART5, MUSART6};
/*Global flag for the USART Busy State*/
static uint8_t USART_u8State[USART_CHANNEL_COUNT] = {IDLE};
static void (*USART_CallBack[USART_CHANNEL_COUNT])(void) = {NULL};
/*==================================================================================================*/
/**
 * @fn USART_Init
 * @brief Initialize the USART peripheral with the provided configuration.
 *
 * This function configures all USART parameters including clock phase,
 * polarity, master/slave mode, baud rate, data frame format, and other
 * settings.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_Init(USART_Config_t *ChannelConfig) {
  ErrorState_t Local_u8ErrorState = OK;

  if (ChannelConfig == NULL) {
    Local_u8ErrorState = NULL_POINTER;
  } else {
    if (ChannelConfig->Channel > USART_CHANNEL_COUNT) {
      Local_u8ErrorState = NOK;
    } else {
      /* Configure Over Sampling */
      USART_Channel[ChannelConfig->Channel]->CR1 |=
          (ChannelConfig->OverSampling & 0X1) << CR1_OVER;

      /* Configure Baud Rate */
      /* Integer calculation to avoid floating point issues */
      uint32_t PCLK = MAXIMUM_CLOCK; // Assuming 16MHz
      uint32_t USARTDIV = 0;

      if (ChannelConfig->OverSampling == USART_OVERSAMPLING_16) {
        /* OSA=0 (16) -> DIV = PCLK / (16 * Baud) */
        /* Multiply by 100 to handle 2 decimal places of precision for rounding
         */
        /* Mantissa = PCLK / (16 * Baud) */
        /* Fraction = ((PCLK % (16 * Baud)) * 16) / (16 * Baud) ... simplified
         */

        /* Standard Formula for BRR:
           DIV_Mantissa = PCLK / (16 * Baud)
           DIV_Fraction = (PCLK % (16 * Baud)) * 16 / (16 * Baud) + 0.5 (round)
        */

        uint32_t div_mul_100 =
            (uint32_t)(((uint64_t)PCLK * 100) / (16 * ChannelConfig->BaudRate));
        uint32_t mantissa = div_mul_100 / 100;
        uint32_t fraction =
            ((div_mul_100 % 100) * 16 + 50) / 100; // +50 for rounding

        /* Handle carry over from fraction rounding */
        if (fraction > 15) {
          mantissa += fraction >> 4;
          fraction &= 0xF;
        }

        USART_Channel[ChannelConfig->Channel]->BRR =
            (mantissa << 4) | (fraction & 0xF);
      } else if (ChannelConfig->OverSampling == USART_OVERSAMPLING_8) {
        /* OSA=1 (8) -> DIV = PCLK / (8 * Baud) */
        uint32_t div_mul_100 =
            (uint32_t)(((uint64_t)PCLK * 100) / (8 * ChannelConfig->BaudRate));
        uint32_t mantissa = div_mul_100 / 100;
        uint32_t fraction =
            ((div_mul_100 % 100) * 8 + 50) / 100; // +50 for rounding

        /* Handle carry over */
        if (fraction > 7) {
          mantissa += fraction >> 3;
          fraction &= 0x7;
        }

        USART_Channel[ChannelConfig->Channel]->BRR =
            (mantissa << 4) | (fraction & 0x7);
      }

      /* Configure Word Length */
      USART_Channel[ChannelConfig->Channel]->CR1 |=
          (ChannelConfig->WordLength & 0X1) << CR1_WL;
      /* Configure Stop Bits */
      USART_Channel[ChannelConfig->Channel]->CR2 |=
          (ChannelConfig->StopBits & 0X3) << CR2_SB;
      /* Configure Parity */
      if (ChannelConfig->Parity != USART_PARITY_NONE) {
        /* Enable Parity */
        USART_Channel[ChannelConfig->Channel]->CR1 |= (1 << CR1_PCE);
        /* Configure Parity */
        USART_Channel[ChannelConfig->Channel]->CR1 |=
            (ChannelConfig->Parity & 0X1) << CR1_PS;
      }

      /* Configure Mode */
      switch (ChannelConfig->Mode) {
      case USART_MODE_TX_RX: /* Enable both transmitter and receiver */
        USART_Channel[ChannelConfig->Channel]->CR1 |=
            (1 << CR1_TE) | (1 << CR1_RE);
        break;
      case USART_MODE_TX: /* Enable transmitter only */
        USART_Channel[ChannelConfig->Channel]->CR1 |= (1 << CR1_TE);
        break;
      case USART_MODE_RX: /* Enable receiver only */
        USART_Channel[ChannelConfig->Channel]->CR1 |= (1 << CR1_RE);
        break;
      default:
        Local_u8ErrorState = NOK;
        break;
      }

      USART_Channel[ChannelConfig->Channel]->CR1 |= (1 << 13);
    }
  }

  return Local_u8ErrorState;
}
/*==================================================================================================*/
/**
 * @fn USART_InitIT
 * @brief Initialize the USART peripheral with the provided configuration and
 * interrupt mode.
 *
 * This function configures all USART parameters including clock phase,
 * polarity, master/slave mode, baud rate, data frame format, and other
 * settings.
 *
 * @param ChannelHandle Pointer to the USART handle structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_InitIT(USART_Handle_t *ChannelHandle) {
  ErrorState_t Local_u8ErrorState = OK;

  if (ChannelHandle == NULL) {
    Local_u8ErrorState = NULL_POINTER;
  } else {
    if (ChannelHandle->Channel > USART_CHANNEL_COUNT) {
      Local_u8ErrorState = NOK;
    } else {
      /* Configure Over Sampling */
      USART_Channel[ChannelHandle->Channel]->CR1 |=
          (ChannelHandle->OverSampling & 0X1) << CR1_OVER;

      /* Configure Baud Rate */
      if (ChannelHandle->OverSampling == USART_OVERSAMPLING_16) {
        /* BAUD_RATE = (fCK / (8 * (2 - Over8) * USART_DIV) */
        /* USART_DIV = (fCK / (8 * (2 - Over8) * BAUD_RATE) */
        /* USART_Fraction = Fraction Part * 16 (in oversampling 16)*/
        uint32_t Local_u32BaudRate =
            (double)(MAXIMUM_CLOCK /
                     (8.0 * (2.0 - ChannelHandle->OverSampling) *
                      ChannelHandle->BaudRate) *
                     1000);
        uint32_t Local_u32USARTDiv_Mantissa =
            (uint32_t)(Local_u32BaudRate / 1000);
        /* +500 To get Round */
        uint32_t Local_u32USARTDiv_Fraction =
            (uint32_t)((((Local_u32BaudRate % 1000) * 16) + 500) / 1000);

        USART_Channel[ChannelHandle->Channel]->BRR &= ~BRR_MASKING;
        USART_Channel[ChannelHandle->Channel]->BRR |=
            ((Local_u32USARTDiv_Mantissa & 0XFFF) << BRR_DIV_MANTISSA_SHIFTING);
        USART_Channel[ChannelHandle->Channel]->BRR |=
            (Local_u32USARTDiv_Fraction & 0XF);
      } else if (ChannelHandle->OverSampling == USART_OVERSAMPLING_8) {
        /* BAUD_RATE = (fCK / (8 * (2 - Over8) * USART_DIV) */
        /* USART_DIV = (fCK / (8 * (2 - Over8) * BAUD_RATE) */
        /* USART_Fraction = Fraction Part * 8 (in oversampling 8) */
        uint32_t Local_u32BaudRate =
            (double)(MAXIMUM_CLOCK /
                     (8.0 * (2.0 - ChannelHandle->OverSampling) *
                      ChannelHandle->BaudRate) *
                     1000);
        uint32_t Local_u32USARTDiv_Mantissa =
            (uint32_t)(Local_u32BaudRate / 1000);
        /* +500 To get Round */
        uint32_t Local_u32USARTDiv_Fraction =
            (uint32_t)((((Local_u32BaudRate % 1000) * 8) + 500) / 1000);

        USART_Channel[ChannelHandle->Channel]->BRR &= ~BRR_MASKING;
        USART_Channel[ChannelHandle->Channel]->BRR |=
            ((Local_u32USARTDiv_Mantissa & 0XFFF) << BRR_DIV_MANTISSA_SHIFTING);
        USART_Channel[ChannelHandle->Channel]->BRR |=
            (Local_u32USARTDiv_Fraction & 0X7);
      }

      /* Configure Word Length */
      USART_Channel[ChannelHandle->Channel]->CR1 |=
          (ChannelHandle->WordLength & 0X1) << CR1_WL;
      /* Configure Stop Bits */
      USART_Channel[ChannelHandle->Channel]->CR2 |=
          (ChannelHandle->StopBits & 0X3) << CR2_SB;
      /* Configure Parity */
      if (ChannelHandle->Parity != USART_PARITY_NONE) {
        /* Enable Parity */
        USART_Channel[ChannelHandle->Channel]->CR1 |= (1 << CR1_PCE);
        /* Configure Parity */
        USART_Channel[ChannelHandle->Channel]->CR1 |=
            (ChannelHandle->Parity & 0X1) << CR1_PS;
      }

      /* Configure Mode */
      switch (ChannelHandle->Mode) {
      case USART_MODE_TX_RX: /* Enable both transmitter and receiver */
        USART_Channel[ChannelHandle->Channel]->CR1 |=
            (1 << CR1_TE) | (1 << CR1_RE);
        break;
      case USART_MODE_TX: /* Enable transmitter only */
        USART_Channel[ChannelHandle->Channel]->CR1 |= (1 << CR1_TE);
        break;
      case USART_MODE_RX: /* Enable receiver only */
        USART_Channel[ChannelHandle->Channel]->CR1 |= (1 << CR1_RE);
        break;
      default:
        Local_u8ErrorState = NOK;
        break;
      }
    }

    /* Configure Interrupts */
    USART_Channel[ChannelHandle->Channel]->CR1 |= (ChannelHandle->RXNEIE & 0X1)
                                                  << CR1_RXNEIE;
    USART_Channel[ChannelHandle->Channel]->CR1 |= (ChannelHandle->TCIE & 0X1)
                                                  << CR1_TCIE;
    USART_Channel[ChannelHandle->Channel]->CR1 |= (ChannelHandle->TXEIE & 0X1)
                                                  << CR1_TXEIE;
    USART_Channel[ChannelHandle->Channel]->CR1 |= (ChannelHandle->IDLEIE & 0X1)
                                                  << CR1_IDLEIE;
    USART_Channel[ChannelHandle->Channel]->CR1 |= (ChannelHandle->PEIE & 0X1)
                                                  << CR1_PEIE;

    /* Store Callback Function */
    USART_CallBack[ChannelHandle->Channel] = ChannelHandle->pfnCallback;

    /* Enable USART */
    USART_Channel[ChannelHandle->Channel]->CR1 |= (1 << 13);
  }
  return Local_u8ErrorState;
}
/*==================================================================================================*/
/**
 * @fn USART_enumTransmit
 * @brief Transmit data through the USART interface.
 *
 * This function transmits data through the USART interface.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @param TX_Data Data to be transmitted
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_enumTransmit(USART_Config_t *ChannelConfig,
                                uint8_t TX_Data) {
  ErrorState_t Local_u8ErrorState = OK;
  ErrorState_t Local_u32TimeoutCounter = 0;

  if (ChannelConfig->Channel > USART_CHANNEL_COUNT) {
    Local_u8ErrorState = NULL_POINTER;
  } else {
    if (USART_u8State[ChannelConfig->Channel] == IDLE) {
      USART_u8State[ChannelConfig->Channel] = BUSY;

      while ((USART_Channel[ChannelConfig->Channel]->SR & (1 << SR_TXE)) >>
                     SR_TXE ==
                 0 &&
             (Local_u32TimeoutCounter != USART_u32TIMEOUT))
        ;
      { Local_u32TimeoutCounter++; }

      if (Local_u32TimeoutCounter == USART_u32TIMEOUT) {
        Local_u8ErrorState = TIMEOUT_STATE;
      } else {
        /* Store Data */
        USART_Channel[ChannelConfig->Channel]->DR = TX_Data;
      }

      USART_u8State[ChannelConfig->Channel] = IDLE;
    } else {
      Local_u8ErrorState = BUSY_STATE;
    }
  }
  return Local_u8ErrorState;
}
/*==================================================================================================*/
/**
 * @fn USART_enumTransmitString
 * @brief Transmit a string of data through the USART interface.
 *
 * This function transmits a string of data through the USART interface.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @param TX_Data Pointer to the string of data to be transmitted
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_enumTransmitString(USART_Config_t *ChannelConfig,
                                      uint8_t *TX_Data) {
  ErrorState_t Local_u8ErrorState = OK;
  uint32_t Local_u32Index = 0;

  if (ChannelConfig->Channel > USART_CHANNEL_COUNT || TX_Data == NULL) {
    Local_u8ErrorState = NULL_POINTER;
  } else {
    if (USART_u8State[ChannelConfig->Channel] == IDLE) {
      while (TX_Data[Local_u32Index] != '\0') {
        Local_u8ErrorState =
            USART_enumTransmit(ChannelConfig, TX_Data[Local_u32Index]);
        Local_u32Index++;
        if (Local_u8ErrorState != OK) {
          break;
        }
      }
    } else {
      Local_u8ErrorState = BUSY_STATE;
    }
  }
  return Local_u8ErrorState;
}
/*==================================================================================================*/
/**
 * @fn USART_enumReceive
 * @brief Receive data through the USART interface.
 *
 * This function receives data through the USART interface.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @param RX_Data Pointer to store the received data
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_enumReceive(USART_Config_t *ChannelConfig,
                               uint8_t *RX_Data) {

  ErrorState_t Local_u8ErrorState = OK;
  ErrorState_t Local_u32TimeoutCounter = 0;

  if (ChannelConfig->Channel > USART_CHANNEL_COUNT || RX_Data == NULL) {
    Local_u8ErrorState = NULL_POINTER;
  } else {
    if (USART_u8State[ChannelConfig->Channel] == IDLE) {
      USART_u8State[ChannelConfig->Channel] = BUSY;

      while ((USART_Channel[ChannelConfig->Channel]->SR & (1 << SR_RXNE)) >>
                     SR_RXNE ==
                 0 &&
             Local_u32TimeoutCounter != USART_u32TIMEOUT) {
        Local_u32TimeoutCounter++;
      }

      if (Local_u32TimeoutCounter == USART_u32TIMEOUT) {
        Local_u8ErrorState = TIMEOUT_STATE;
      } else {
        /* Store Data */
        *RX_Data = USART_Channel[ChannelConfig->Channel]->DR;
      }

      USART_u8State[ChannelConfig->Channel] = IDLE;
    } else {
      Local_u8ErrorState = BUSY_STATE;
    }
  }
  return Local_u8ErrorState;
}
/*==================================================================================================*/
/**
 * @fn USART1_IRQHandler
 * @brief USART1 interrupt handler.
 *
 * This function handles the USART1 interrupt.
 */
void USART1_IRQHandler(void) {
  if (USART_CallBack[USART_CHANNEL1] != NULL) {
    USART_CallBack[USART_CHANNEL1]();
  }
}
/*==================================================================================================*/
/**
 * @fn USART2_IRQHandler
 * @brief USART2 interrupt handler.
 *
 * This function handles the USART2 interrupt.
 */
void USART2_IRQHandler(void) {
  if (USART_CallBack[USART_CHANNEL2] != NULL) {
    USART_CallBack[USART_CHANNEL2]();
  }
}
/*==================================================================================================*/
/**
 * @fn USART3_IRQHandler
 * @brief USART3 interrupt handler.
 *
 * This function handles the USART3 interrupt.
 */
void USART3_IRQHandler(void) {
  if (USART_CallBack[USART_CHANNEL3] != NULL) {
    USART_CallBack[USART_CHANNEL3]();
  }
}
/*==================================================================================================*/
/**
 * @fn USART4_IRQHandler
 * @brief USART4 interrupt handler.
 *
 * This function handles the USART4 interrupt.
 */
void USART4_IRQHandler(void) {
  if (USART_CallBack[USART_CHANNEL4] != NULL) {
    USART_CallBack[USART_CHANNEL4]();
  }
}
/*==================================================================================================*/
/**
 * @fn USART5_IRQHandler
 * @brief USART5 interrupt handler.
 *
 * This function handles the USART5 interrupt.
 */
void USART5_IRQHandler(void) {
  if (USART_CallBack[USART_CHANNEL5] != NULL) {
    USART_CallBack[USART_CHANNEL5]();
  }
}
/*==================================================================================================*/
/**
 * @fn USART6_IRQHandler
 * @brief USART6 interrupt handler.
 *
 * This function handles the USART6 interrupt.
 */
void USART6_IRQHandler(void) {
  if (USART_CallBack[USART_CHANNEL6] != NULL) {
    USART_CallBack[USART_CHANNEL6]();
  }
}
