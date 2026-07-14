/**
 ******************************************************************************
 * @file    SPI_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the SPI driver — not a public header.
 * @ingroup mcal_spi
 ******************************************************************************
 */

#ifndef _SPI_PRIVATE_H_
#define _SPI_PRIVATE_H_


#define SPI_CHANNEL_COUNT 4  /**< How many SPI peripherals this MCU has. */

/** @brief Bit positions within the SPI `CR1` register. */
typedef enum
{
  CR1_CPHA,                 /**< Clock phase. */
  CR1_CPOL,                 /**< Clock polarity. */
  CR1_MSTR,                 /**< Master selection. */
  CR1_BR,                   /**< Baud-rate prescaler, 3 bits wide. */
  CR1_SPE = 6,              /**< SPI enable. */
  CR1_LSBFIRST,             /**< Frame format: LSB first. */
  CR1_SSI,                  /**< Internal slave select. */
  CR1_SSM,                  /**< Software slave management. */
  CR1_RXONLY,               /**< Receive-only mode. */
  CR1_DFF,                  /**< Data frame format: 8 or 16 bits. */
  CR1_CRCNEXT,              /**< Transmit the CRC next. */
  CR1_CRCEN,                /**< Hardware CRC enable. */
  CR1_BIDIOE,               /**< Output enable in bidirectional mode. */
  CR1_BIDIMODE,             /**< Bidirectional (one-line) mode. */
} SPI_CR1_BITS;

/** @brief Bit positions within the SPI `SR` (status) register. */
typedef enum
{
  SR_RXNE,                  /**< Receive buffer not empty — a frame is waiting. */
  SR_TXE,                   /**< Transmit buffer empty — ready for the next frame. */
  SR_CHSIDE,                /**< Channel side (I2S mode only). */
  SR_UDR,                   /**< Underrun (slave mode). */
  SR_CRCERR,                /**< CRC mismatch. */
  SR_MODF,                  /**< Mode fault — another master drove NSS low while we were master. */
  SR_OVR,                   /**< Overrun — a frame arrived before the previous one was read, and was lost. */
  SR_BSY,                   /**< Busy — a transfer is in progress. Must be clear before the peripheral is disabled. */
  SR_FRE                    /**< Frame format error. */
} SPI_SR_BITS;

#define SPI_u32TIMEOUT            10000UL        /**< Busy-wait bound for the status flags, in loop iterations. Escapes a dead peripheral rather than hanging forever. */
#endif /* _SPI_PRIVATE_H_ */
