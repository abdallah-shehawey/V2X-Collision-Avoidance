/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    USART_intrefac.h     >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : USART                                           **
 **                                                                           **
 **===========================================================================**
 */

#ifndef _USART_INTERFACE_H_
#define _USART_INTERFACE_H_

typedef enum
{
  USART_CHANNEL1,
  USART_CHANNEL2,
  USART_CHANNEL3,
  USART_CHANNEL4,
  USART_CHANNEL5,
  USART_CHANNEL6,
} USART_Channel_t;

typedef enum
{
  USART_WORDLENGTH_8B,
  USART_WORDLENGTH_9B,
} USART_WordLength_t;

typedef enum
{
  USART_STOPBITS_0_5,
  USART_STOPBITS_1,
  USART_STOPBITS_1_5,
  USART_STOPBITS_2,
} USART_StopBits_t;

typedef enum
{
  USART_PARITY_NONE,
  USART_PARITY_ODD,
  USART_PARITY_EVEN,
} USART_Parity_t;

typedef enum
{
  USART_MODE_TX_RX,
  USART_MODE_TX,
  USART_MODE_RX,
} USART_Mode_t;

typedef enum
{
  USART_DIS,
  USART_EN
} USART_State_t;

typedef enum
{
  USART_OVERSAMPLING_16,
  USART_OVERSAMPLING_8
}USART_OverSampling_t;

typedef enum
{
  UART_HWCONTROL_NONE,
  UART_HWCONTROL_RTS,
  UART_HWCONTROL_CTS,
  UART_HWCONTROL_RTS_CTS,
} USART_HardwareFlowControl_t;

typedef struct
{
  USART_Channel_t Channel;
  uint32_t BaudRate;
  USART_WordLength_t WordLength;
  USART_StopBits_t StopBits;
  USART_Parity_t Parity;
  USART_Mode_t Mode;
  USART_HardwareFlowControl_t HardwareFlowControl;
  USART_OverSampling_t OverSampling;
} USART_Config_t;

typedef enum
{
  USART_RXNEIE_DIS,
  USART_RXNEIE_EN
} USART_RXNEIE_t;

typedef enum
{
  USART_TCIE_DIS,
  USART_TCIE_EN
} USART_TCIE_t;

typedef enum
{
  USART_TXEIE_DIS,
  USART_TXEIE_EN
} USART_TXEIE_t;

typedef enum
{
  USART_IDLEIE_DIS,
  USART_IDLEIE_EN
} USART_IDLEIE_t;

typedef enum
{
  USART_PEIE_DIS,
  USART_PEIE_EN
}USART_PEIE_t;

typedef struct
{
  USART_Channel_t Channel;
  uint32_t BaudRate;
  USART_WordLength_t WordLength;
  USART_StopBits_t StopBits;
  USART_Parity_t Parity;
  USART_Mode_t Mode;
  USART_HardwareFlowControl_t HardwareFlowControl;
  USART_OverSampling_t OverSampling;
  USART_RXNEIE_t RXNEIE;
  USART_TCIE_t TCIE;
  USART_TXEIE_t TXEIE;
  USART_IDLEIE_t IDLEIE;
  USART_PEIE_t PEIE;
  void (*pfnCallback)(void);
} USART_Handle_t;
/*==================================================================================================*/
/**
 * @fn USART_Init
 * @brief Initialize the USART peripheral with the provided configuration.
 *
 * This function configures all USART parameters including clock phase, polarity,
 * master/slave mode, baud rate, data frame format, and other settings.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_Init(USART_Config_t *ChannelConfig);
/*==================================================================================================*/
/**
 * @fn USART_InitIT
 * @brief Initialize the USART peripheral with the provided configuration.
 *
 * This function configures all USART parameters including clock phase, polarity,
 * master/slave mode, baud rate, data frame format, and other settings.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_InitIT(USART_Handle_t *ChannelHandle);
/*==================================================================================================*/
/**
 * @fn USART_enumTransmit
 * @brief Initialize the USART peripheral with the provided configuration.
 *
 * This function configures all USART parameters including clock phase, polarity,
 * master/slave mode, baud rate, data frame format, and other settings.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_enumTransmit(USART_Config_t *ChannelConfig, uint8_t TX_Data);
/*==================================================================================================*/
/**
 * @fn USART_enumReceive
 * @brief Initialize the USART peripheral with the provided configuration.
 *
 * This function configures all USART parameters including clock phase, polarity,
 * master/slave mode, baud rate, data frame format, and other settings.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_enumReceive(USART_Config_t *ChannelConfig, uint8_t *RX_Data);
/*==================================================================================================*/
/**
 * @fn USART_enumTransmitString
 * @brief Initialize the USART peripheral with the provided configuration.
 *
 * This function configures all USART parameters including clock phase, polarity,
 * master/slave mode, baud rate, data frame format, and other settings.
 *
 * @param ChannelConfig Pointer to the USART configuration structure
 * @return ErrorState_t OK if successful, error code otherwise
 *
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t USART_enumTransmitString(USART_Config_t *ChannelConfig, uint8_t *TX_Data);
/*==================================================================================================*/

#endif /* _USART_INTERFACE_H_ */
