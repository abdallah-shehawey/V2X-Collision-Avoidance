/**
 ******************************************************************************
 * @file    USART_intreface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the USART driver — the asynchronous serial ports.
 * @ingroup mcal_usart
 ******************************************************************************
 */

#ifndef _USART_INTERFACE_H_
#define _USART_INTERFACE_H_

#include "../../LIB/ErrTypes.h"
#include <stdint.h>


/** @brief Which USART peripheral a call applies to. */
typedef enum
{
  USART_CHANNEL1,                 /**< USART1 — the ESP32 V2V link. */
  USART_CHANNEL2,                 /**< USART2 — the Raspberry Pi telemetry link. */
  USART_CHANNEL3,                 /**< USART3. */
  USART_CHANNEL4,                 /**< UART4. */
  USART_CHANNEL5,                 /**< UART5. */
  USART_CHANNEL6,                 /**< USART6. */
} USART_Channel_t;

/** @brief Data bits per frame. */
typedef enum
{
  USART_WORDLENGTH_8B,            /**< 8 data bits per frame — the usual choice. */
  USART_WORDLENGTH_9B,            /**< 9 data bits per frame; the 9th is normally the parity bit. */
} USART_WordLength_t;

/** @brief Stop bits appended to each frame. Both ends must agree. */
typedef enum
{
  USART_STOPBITS_0_5,             /**< Half a stop bit (smartcard mode). */
  USART_STOPBITS_1,               /**< One stop bit — the usual choice. */
  USART_STOPBITS_1_5,             /**< One and a half stop bits (smartcard mode). */
  USART_STOPBITS_2,               /**< Two stop bits; gives a slow receiver more time to keep up. */
} USART_StopBits_t;

/** @brief Parity checking. */
typedef enum
{
  USART_PARITY_NONE,              /**< No parity bit. Both links here run without parity. */
  USART_PARITY_ODD,               /**< Odd parity. */
  USART_PARITY_EVEN,              /**< Even parity. */
} USART_Parity_t;

/** @brief Which direction(s) of the peripheral to switch on. */
typedef enum
{
  USART_MODE_TX_RX,               /**< Transmitter and receiver both enabled. */
  USART_MODE_TX,                  /**< Transmit only. */
  USART_MODE_RX,                  /**< Receive only. */
} USART_Mode_t;

/** @brief Generic enable/disable for a USART feature. */
typedef enum
{
  USART_DIS,                      /**< Peripheral disabled. */
  USART_EN                        /**< Peripheral enabled. */
} USART_State_t;

/**
 * @brief How many times the receiver samples each bit.
 *
 * The trade is noise margin against top speed: 16x recovers the bit from a
 * majority vote and tolerates a sloppier clock, 8x doubles the achievable baud
 * rate but leaves far less room for drift.
 */
typedef enum
{
  USART_OVERSAMPLING_16,          /**< Sample each bit 16 times — more tolerant of noise and clock error. */
  USART_OVERSAMPLING_8            /**< Sample each bit 8 times — allows twice the baud rate, with less margin. */
}USART_OverSampling_t;

/** @brief RTS/CTS hardware flow control. */
typedef enum
{
  UART_HWCONTROL_NONE,            /**< No flow control. This is what both links use. */
  UART_HWCONTROL_RTS,             /**< Request-to-send only. */
  UART_HWCONTROL_CTS,             /**< Clear-to-send only. */
  UART_HWCONTROL_RTS_CTS,         /**< Both RTS and CTS. */
} USART_HardwareFlowControl_t;

/**
 * @brief Line settings for a USART used in **polled** mode.
 *
 * The classic "baud rate, word length, parity, stop bits" set. Use this when the
 * peripheral is driven by blocking send/receive calls. For an interrupt-driven
 * port — which is what USART1 needs, since the ESP32 sends unprompted — use
 * @ref USART_Handle_t instead, which adds the interrupt enables and the callback.
 */
typedef struct
{
  USART_Channel_t Channel;                           /**< Which USART peripheral (USART1..6). */
  uint32_t BaudRate;                                 /**< Baud rate in bits per second, e.g. 115200. */
  USART_WordLength_t WordLength;                     /**< 8 or 9 data bits per frame. */
  USART_StopBits_t StopBits;                         /**< Number of stop bits. */
  USART_Parity_t Parity;                             /**< Parity: none, even or odd. */
  USART_Mode_t Mode;                                 /**< Enable the transmitter, the receiver, or both. */
  USART_HardwareFlowControl_t HardwareFlowControl;   /**< RTS/CTS flow control; none on this board. */
  USART_OverSampling_t OverSampling;                 /**< 8x or 16x oversampling. 16x is more noise-tolerant; 8x allows higher baud rates. */
} USART_Config_t;

/** @brief "Byte received" interrupt enable. */
typedef enum
{
  USART_RXNEIE_DIS,               /**< No interrupt when a byte arrives. */
  USART_RXNEIE_EN                 /**< Interrupt when a byte arrives — what the ESP32 link runs on. */
} USART_RXNEIE_t;

/** @brief "Transmission complete" interrupt enable. */
typedef enum
{
  USART_TCIE_DIS,                 /**< No transmission-complete interrupt. */
  USART_TCIE_EN                   /**< Interrupt once the last bit has actually left the shift register. */
} USART_TCIE_t;

/** @brief "Transmit register empty" interrupt enable. */
typedef enum
{
  USART_TXEIE_DIS,                /**< No transmit-empty interrupt. */
  USART_TXEIE_EN                  /**< Interrupt as soon as the data register can take the next byte. */
} USART_TXEIE_t;

/** @brief "Line went idle" interrupt enable. */
typedef enum
{
  USART_IDLEIE_DIS,               /**< No idle-line interrupt. */
  USART_IDLEIE_EN                 /**< Interrupt when the line has been idle for a full frame — useful for framing variable-length messages. */
} USART_IDLEIE_t;

/** @brief "Parity error" interrupt enable. */
typedef enum
{
  USART_PEIE_DIS,                 /**< No parity-error interrupt. */
  USART_PEIE_EN                   /**< Interrupt on a parity error. */
}USART_PEIE_t;

/**
 * @brief A USART used in **interrupt-driven** mode: line settings plus the ISR hookup.
 *
 * This is @ref USART_Config_t with the five interrupt enables and a callback
 * bolted on. USART1 uses it because the ESP32 talks whenever it likes: there is
 * no request/response pattern to poll for, so a byte has to be able to interrupt
 * us.
 *
 * @note The driver calls @ref pfnCallback from the USART ISR itself. Whatever it
 *       points at must therefore be ISR-safe: short, non-blocking, and restricted
 *       to the `...FromISR` FreeRTOS API. `vESP_UART_RX_Callback` in `main.c` is
 *       the one that matters — it does nothing but enqueue the byte.
 */
typedef struct
{
  USART_Channel_t Channel;                         /**< Which USART peripheral (USART1..6). */
  uint32_t BaudRate;                               /**< Baud rate in bits per second. */
  USART_WordLength_t WordLength;                   /**< 8 or 9 data bits per frame. */
  USART_StopBits_t StopBits;                       /**< Number of stop bits. */
  USART_Parity_t Parity;                           /**< Parity: none, even or odd. */
  USART_Mode_t Mode;                               /**< Enable the transmitter, the receiver, or both. */
  USART_HardwareFlowControl_t HardwareFlowControl; /**< RTS/CTS flow control; none on this board. */
  USART_OverSampling_t OverSampling;               /**< 8x or 16x oversampling. */
  USART_RXNEIE_t RXNEIE;                           /**< Interrupt when a byte has been received. This is the one the ESP32 link needs. */
  USART_TCIE_t TCIE;                               /**< Interrupt when a transmission has fully completed. */
  USART_TXEIE_t TXEIE;                             /**< Interrupt when the transmit register is free for the next byte. */
  USART_IDLEIE_t IDLEIE;                           /**< Interrupt when the line has gone idle. */
  USART_PEIE_t PEIE;                               /**< Interrupt on a parity error. */
  void (*pfnCallback)(void);                       /**< Called from the USART ISR — see the note above. */
} USART_Handle_t;
/*==================================================================================================*/
/**
 * @brief Bring a USART up in **polled** mode: baud rate, framing, parity, direction.
 *
 * @param[in] ChannelConfig The port's line settings.
 * @return OK if the port was configured, NULL_POINTER if @p ChannelConfig was NULL.
 *
 * @note The port's clock must already be enabled in RCC. For a port that has to
 *       receive unprompted traffic, use @ref USART_InitIT instead.
 */
ErrorState_t USART_Init(USART_Config_t *ChannelConfig);
/*==================================================================================================*/
/**
 * @brief Bring a USART up in **interrupt-driven** mode.
 *
 * Applies the same line settings as @ref USART_Init, and additionally enables the
 * interrupt sources named in the handle and registers its callback. This is what
 * USART1 uses, since the ESP32 transmits unprompted.
 *
 * @param[in] ChannelHandle The port's settings, interrupt enables and ISR callback.
 * @return OK if the port was configured, NULL_POINTER if @p ChannelHandle was NULL.
 *
 * @note Enabling the interrupt here is only half the job — the IRQ must also be
 *       enabled in the NVIC (@ref NVIC_vEnableIRQ), and given a priority number of
 *       6 or higher if its callback touches the FreeRTOS API.
 */
ErrorState_t USART_InitIT(USART_Handle_t *ChannelHandle);
/*==================================================================================================*/
/**
 * @brief Send one byte, blocking until the transmit register is free.
 *
 * @param[in] ChannelConfig Which port to send on.
 * @param     TX_Data       The byte to send.
 * @retval OK            The byte was handed to the peripheral.
 * @retval NULL_POINTER  @p ChannelConfig was NULL.
 * @retval TIMEOUT_STATE The transmit register never emptied — the peripheral is stuck.
 */
ErrorState_t USART_enumTransmit(USART_Config_t *ChannelConfig, uint8_t TX_Data);
/*==================================================================================================*/
/**
 * @brief Receive one byte, blocking until one arrives.
 *
 * @param[in]  ChannelConfig Which port to read from.
 * @param[out] RX_Data       Receives the byte.
 * @retval OK            A byte was read.
 * @retval NULL_POINTER  @p ChannelConfig or @p RX_Data was NULL.
 * @retval TIMEOUT_STATE No byte arrived within the driver's busy-wait bound.
 *
 * @warning This blocks the calling task. The ESP32 link does **not** use it — it
 *          is interrupt-driven, precisely so nothing has to sit and wait for a
 *          peer that may say nothing for seconds.
 */
ErrorState_t USART_enumReceive(USART_Config_t *ChannelConfig, uint8_t *RX_Data);
/*==================================================================================================*/
/**
 * @brief Send a NUL-terminated string, one byte at a time.
 *
 * This is what carries the telemetry line to the Raspberry Pi.
 *
 * @param[in] ChannelConfig Which port to send on.
 * @param[in] TX_Data       The string to send; the terminating NUL is not transmitted.
 * @retval OK            The whole string was sent.
 * @retval NULL_POINTER  @p ChannelConfig or @p TX_Data was NULL.
 * @retval TIMEOUT_STATE The peripheral stalled part-way through.
 */
ErrorState_t USART_enumTransmitString(USART_Config_t *ChannelConfig, uint8_t *TX_Data);
/*==================================================================================================*/
/**
 * @brief Directly read the data register without checking flags or busy state.
 * @param Channel USART Channel to read from.
 * @return uint8_t The received byte.
 */
uint8_t USART_ReceiveByteDirect(USART_Channel_t Channel);
/*==================================================================================================*/
/**
 * @brief Non-blocking check: has a byte arrived (`SR.RXNE`)?
 *
 * Reads the flag only — never touches `DR`, so it cannot itself clear RXNE or
 * consume the byte. Pair it with @ref USART_ReceiveByteDirect: check this
 * first, and only call that once it reports a byte is actually waiting.
 *
 * Added for the FOTA bootloader (`V2V-STM32/Bootloader/`), which runs no
 * interrupts at all and needs to poll a UART for an incoming byte while also
 * tracking its own timeout — something @ref USART_enumReceive cannot do,
 * since it blocks internally for its own fixed busy-wait bound. Safe to use
 * anywhere else in the firmware too; it is a pure flag read with no side
 * effects on driver state.
 *
 * @param Channel USART channel to check.
 * @retval 1 A received byte is waiting in `DR`.
 * @retval 0 Nothing waiting (or @p Channel was out of range).
 */
uint8_t USART_u8IsRxNotEmpty(USART_Channel_t Channel);
/*==================================================================================================*/

#endif /* _USART_INTERFACE_H_ */
