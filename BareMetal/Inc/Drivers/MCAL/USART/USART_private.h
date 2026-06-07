/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    USART_private.h   >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : USART                                           **
 **                                                                           **
 **===========================================================================**
 */

#ifndef _USART_PRIVATE_H_
#define _USART_PRIVATE_H_

#define BRR_MASKING                 (0XFFFF)

#define BRR_DIV_MANTISSA_SHIFTING   4U

#define USART_CHANNEL_COUNT         6U

#define SR_TXE                      7U
#define SR_RXNE                     5U

#define CR1_IDLEIE                  4U
#define CR1_RXNEIE                  5U
#define CR1_TCIE                    6U
#define CR1_TXEIE                   7U
#define CR1_PEIE                    8U

#define CR1_WL                      12U
#define CR2_SB                      12U
#define CR1_PCE                     10U
#define CR1_PS                      9U
#define CR1_TE                      3U
#define CR1_RE                      2U
#define CR1_OVER                    15U

#define MAXIMUM_CLOCK               16000000UL

/* Busy-wait safety net for TXE/RXNE. At 115200 baud one byte = ~87us; worst-case
 * legit wait is ~1 byte-time. 3000 iterations gives >=4x margin even under -O3
 * while bounding a genuinely-stuck transfer to <2ms (vs ~6ms at 10000).
 * NOTE: only safe with fast UARTs (>=115200). At 9600 baud a byte needs ~1ms
 * and this would be too low — raise it back if any channel drops to a slow baud. */
#define USART_u32TIMEOUT            3000UL

#endif /* _USART_PRIVATE_H_  */
