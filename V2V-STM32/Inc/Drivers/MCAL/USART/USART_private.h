/**
 ******************************************************************************
 * @file    USART_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the USART driver — not a public header.
 * @ingroup mcal_usart
 ******************************************************************************
 */

#ifndef _USART_PRIVATE_H_
#define _USART_PRIVATE_H_

#define BRR_MASKING (0XFFFF) /**< Mask of the 16 significant bits of `BRR`. */

#define BRR_DIV_MANTISSA_SHIFTING 4U         /**< Bit position of the mantissa field within `BRR`; the low 4 bits are the fraction. */
#define USART_CHANNEL_COUNT       6U         /**< How many USART/UART peripherals this MCU has. */
#define SR_TXE                    7U         /**< `SR` bit: transmit data register empty. */
#define SR_TC                     6U         /**< `SR` bit: transmission complete — the last bit has left the shift register. */
#define SR_RXNE                   5U         /**< `SR` bit: read data register not empty — a byte is waiting. */
#define CR1_IDLEIE                4U         /**< `CR1` bit: idle-line interrupt enable. */
#define CR1_RXNEIE                5U         /**< `CR1` bit: RXNE interrupt enable. */
#define CR1_TCIE                  6U         /**< `CR1` bit: transmission-complete interrupt enable. */
#define CR1_TXEIE                 7U         /**< `CR1` bit: TXE interrupt enable. */
#define CR1_PEIE                  8U         /**< `CR1` bit: parity-error interrupt enable. */
#define CR1_WL                    12U        /**< `CR1` bit: word length (0 = 8 bits, 1 = 9 bits). */
#define CR2_SB                    12U        /**< `CR2` bit: stop-bit field. */
#define CR1_PCE                   10U        /**< `CR1` bit: parity control enable. */
#define CR1_PS                    9U         /**< `CR1` bit: parity selection (0 = even, 1 = odd). */
#define CR1_TE                    3U         /**< `CR1` bit: transmitter enable. */
#define CR1_RE                    2U         /**< `CR1` bit: receiver enable. */
#define CR1_OVER                  15U        /**< `CR1` bit: oversampling mode (0 = 16x, 1 = 8x). */
#define MAXIMUM_CLOCK             16000000UL /**< Peripheral clock assumed when computing the baud-rate divider [Hz]. */
/**
 * @brief Busy-wait bound for the TXE/RXNE flags, in loop iterations.
 *
 * This is a **stuck-hardware** bound and nothing else. It must never expire
 * during a legitimate byte transfer.
 *
 * @warning It was previously 20000, which was far too small: under load it
 *          expired mid-stream while TXE was still 0, so @ref USART_enumTransmit
 *          dropped the byte *silently* and the DSRC frame arrived truncated —
 *          the ESP32 saw 14-20 bytes per packet instead of 23 and failed every
 *          checksum. A single byte takes ~87 us at 115200 baud and ~174 us at
 *          57600. The value is sized to cover many milliseconds of waiting even
 *          at -O3, while still escaping a genuinely dead peripheral.
 */
#define USART_u32TIMEOUT 2000000UL

#endif /* _USART_PRIVATE_H_  */
