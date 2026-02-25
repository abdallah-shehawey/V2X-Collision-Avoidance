/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SPI_private.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SPI                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef _SPI_PRIVATE_H_
#define _SPI_PRIVATE_H_


#define SPI_CHANNEL_COUNT 4

typedef enum
{
  CR1_CPHA,
  CR1_CPOL,
  CR1_MSTR,
  CR1_BR,
  CR1_SPE = 6,
  CR1_LSBFIRST,
  CR1_SSI,
  CR1_SSM,
  CR1_RXONLY,
  CR1_DFF,
  CR1_CRCNEXT,
  CR1_CRCEN,
  CR1_BIDIOE,
  CR1_BIDIMODE,
} SPI_CR1_BITS;

typedef enum
{
  SR_RXNE,
  SR_TXE,
  SR_CHSIDE,
  SR_UDR,
  SR_CRCERR,
  SR_MODF,
  SR_OVR,
  SR_BSY,
  SR_FRE
} SPI_SR_BITS;

#define SPI_u32TIMEOUT            10000UL

#endif /* _SPI_PRIVATE_H_ */
