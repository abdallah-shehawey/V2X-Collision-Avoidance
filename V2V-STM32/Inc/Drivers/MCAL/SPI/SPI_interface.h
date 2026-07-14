/**
 ******************************************************************************
 * @file    SPI_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the SPI driver — the serial peripheral interface.
 * @ingroup mcal_spi
 ******************************************************************************
 */
#ifndef _SPI_INTERFACE_H_
#define _SPI_INTERFACE_H_


#include "stdint.h"
#include "../../LIB/ErrTypes.h"

/** @brief Which SPI peripheral a call applies to. */
typedef enum
{
  SPI_CHANNEL1,                   /**< SPI1 — the MPU9250 IMU link. */
  SPI_CHANNEL2,                   /**< SPI2. */
  SPI_CHANNEL3,                   /**< SPI3. */
  SPI_CHANNEL4,                   /**< SPI4. */
} SPI_Channel_t;

/**
 * @brief Clock phase: which SCK edge the data is sampled on.
 *
 * Together with @ref SPI_CPOL_t this picks the SPI mode (0..3). Get it wrong and
 * the peripheral samples the line while the slave is still changing it, so every
 * byte comes back shifted or garbled.
 */
typedef enum
{
  SPI_CPHA_1EDGE, /**< Sample on the **first** clock edge. With @ref SPI_CPOL_LOW this is SPI mode 0 — what the MPU9250 wants. */
  SPI_CPHA_2EDGE, /**< Sample on the **second** clock edge. */
} SPI_CPHA_t;

/**
 * @brief Clock polarity: the level SCK idles at between transfers.
 * @see SPI_CPHA_t — the two together define the SPI mode.
 */
typedef enum
{
  SPI_CPOL_LOW,  /**< SCK idles **low**. With @ref SPI_CPHA_1EDGE this is SPI mode 0. */
  SPI_CPOL_HIGH, /**< SCK idles **high**. */
} SPI_CPOL_t;

/**
 * @brief Whether this peripheral generates the clock or follows one.
 */
typedef enum
{
  SPI_MODE_SLAVE,  /**< Slave: SCK is an input, driven by the other end. */
  SPI_MODE_MASTER, /**< Master: this peripheral drives SCK and chip-select. The IMU link is a master. */
} SPI_Mode_t;

/**
 * @brief Divider from the APB clock down to SCK.
 * @note The MPU9250 tolerates 20 MHz for register access but only 1 MHz for
 *       reading its sensor registers, so the prescaler must be chosen against
 *       the *slower* of the two limits.
 */
typedef enum
{
  SPI_BAUDRATEPRESCALER_2,        /**< SCK = f_PCLK / 2 — the fastest available. */
  SPI_BAUDRATEPRESCALER_4,        /**< SCK = f_PCLK / 4. */
  SPI_BAUDRATEPRESCALER_8,        /**< SCK = f_PCLK / 8. */
  SPI_BAUDRATEPRESCALER_16,       /**< SCK = f_PCLK / 16. */
  SPI_BAUDRATEPRESCALER_32,       /**< SCK = f_PCLK / 32. */
  SPI_BAUDRATEPRESCALER_64,       /**< SCK = f_PCLK / 64. */
  SPI_BAUDRATEPRESCALER_128,      /**< SCK = f_PCLK / 128. */
  SPI_BAUDRATEPRESCALER_256,      /**< SCK = f_PCLK / 256 — the slowest available. */
} SPI_BAUDRATEPRESCALER_t;

/** @brief SPI peripheral enable. */
typedef enum
{
  SPI_SPE_DIS,                    /**< Peripheral off. */
  SPI_SPE_EN,                     /**< Peripheral on. Nothing is transferred until this is set. */
} SPI_SPE_t;

/** @brief Bit order within each frame. */
typedef enum
{
  SPI_MSBFIRST,                   /**< Most-significant bit first — what the MPU9250 expects. */
  SPI_LSBFIRST,                   /**< Least-significant bit first. */
} SPI_BFIRST_t;

/** @brief Whether the slave-select line is managed by hardware or by software. */
typedef enum
{
  SPI_NSS_HARDWARE,               /**< Slave-select is driven by the peripheral itself. */
  SPI_NSS_SOFTWARE,               /**< Slave-select is an ordinary GPIO the driver toggles. This is what the IMU link uses. */
} SPI_NSS_MAN_t;

/** @brief The internal slave-select level, when NSS is software-managed. */
typedef enum
{
  SPI_NSSI_SELECT,                /**< Internal slave-select forced low ("selected"). */
  SPI_NSSI_NOT_SELECT,            /**< Internal slave-select forced high. A master with software NSS must set this, or it detects a bus conflict and disables itself. */
} SPI_NSSI_MODE_t;

/** @brief Whether the peripheral transmits as well as receives. */
typedef enum
{
  SPI_RXONLY_DISABLE, /**< Full duplex: transmit and receive together. */
  SPI_RXONLY_ENABLE,  /**< Receive only: the output is disabled and the peripheral just listens. */
} SPI_RXONLY_t;

/** @brief Frame size. */
typedef enum
{
  SPI_DFF_8BIT,                   /**< 8-bit frames — what the IMU link uses. */
  SPI_DFF_16BIT,                  /**< 16-bit frames. */
} SPI_DFF_t;

/** @brief Whether the next word transmitted is the CRC rather than data. */
typedef enum
{
  SPI_CRCNEXT_DISABLE,            /**< Send the next data word normally. */
  SPI_CRCNEXT_ENABLE,             /**< Send the computed CRC in place of the next data word. */
} SPI_CRCNEXT_t;

/** @brief Hardware CRC calculation enable. */
typedef enum
{
  SPI_CRCDIS,                     /**< Hardware CRC off. */
  SPI_CRCEN,                      /**< Hardware CRC on. */
} SPI_CRCEN_t;

/** @brief Full-duplex (two data lines) or half-duplex (one). */
typedef enum
{
  SPI_UNIDIMODE,                  /**< Full duplex: separate MOSI and MISO lines. */
  SPI_BIDIMODE,                   /**< Half duplex: a single bidirectional data line. */
} SPI_DIMODE_t;

/** @brief In half-duplex mode, the direction of the single data line. */
typedef enum
{
  SPI_OUTPUT_DIS,                 /**< In bidirectional mode, the single line is an input (receive). */
  SPI_OUTPUT_EN,                  /**< In bidirectional mode, the single line is an output (transmit). */
} SPI_BIDIOE_t;

/**
 * @brief Everything needed to bring one SPI peripheral up.
 *
 * The fields map one-to-one onto bits of the SPI `CR1`/`CR2` registers, so this
 * struct is essentially a typed, readable form of those registers.
 *
 * @note `CPOL` and `CPHA` together choose the SPI *mode* (0..3), and they must
 *       match what the slave expects or every byte comes back garbage. The
 *       MPU9250 on this board wants mode 0 — @ref SPI_CPOL_LOW with
 *       @ref SPI_CPHA_1EDGE.
 */
typedef struct
{
  SPI_Channel_t Channel;                    /**< Which SPI peripheral (SPI1..SPI4). */
  SPI_CPHA_t CPHA;                          /**< Clock phase: which clock edge samples the data. */
  SPI_CPOL_t CPOL;                          /**< Clock polarity: the idle level of SCK. */
  SPI_Mode_t Mode;                          /**< Master or slave. */
  SPI_BAUDRATEPRESCALER_t BaudRatePrescaler;/**< Divides the APB clock down to SCK. */
  SPI_SPE_t SPE;                            /**< SPI enable — the peripheral does nothing until this is set. */
  SPI_BFIRST_t BFIRST;                      /**< Bit order: MSB or LSB first. */
  SPI_NSS_MAN_t NSS_MAN;                    /**< Whether slave-select is driven by software or by hardware. */
  SPI_NSSI_MODE_t NSSI_MODE;                /**< The software slave-select level, when NSS is software-managed. */
  SPI_RXONLY_t RXONLY;                      /**< Receive-only mode (half-duplex listen). */
  SPI_DFF_t DFF;                            /**< Frame size: 8 or 16 bits. */
  SPI_CRCEN_t CRC_MODE;                     /**< Hardware CRC calculation on/off. */
  SPI_CRCNEXT_t CRCNEXT;                    /**< Transmit the CRC instead of the next data word. */
  SPI_DIMODE_t DIMODE;                      /**< Full-duplex, or one-line bidirectional. */
  SPI_BIDIOE_t BIDIOE;                      /**< In bidirectional mode, whether the single line is an output. */
} SPI_Config_t;

//SPI_Config_t SPI1 =
//{
//    .Channel = SPI_CHANNEL1,
//    .CPHA = SPI_CPHA_1EDGE,
//    .CPOL = SPI_CPOL_HIGH,
//    .Mode = SPI_MODE_MASTER,
//    .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2,
//    .SPE = SPI_SPE_EN,
//    .BFIRST = SPI_MSBFIRST,
//    .NSS_MAN = SPI_NSS_SOFTWARE,
//    .NSSI_MODE = SPI_NSSI_NOT_SELECT,
//    .RXONLY = SPI_RXONLY_DISABLE,
//    .DFF = SPI_DFF_8BIT,
//    .CRC_MODE = SPI_CRCDIS,
//    .DIMODE = SPI_UNIDIMODE,
//};
/*==================================================================================================*/
/**
 * @brief Initialize the SPI peripheral with the provided configuration.
 *
 * This function configures all SPI parameters including clock phase, polarity,
 * master/slave mode, baud rate, data frame format, and other settings.
 *
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @return ErrorState_t OK if initialization successful, error code otherwise
 *
 * @warning NULL pointer check is performed on the input parameter.
 * @par Example:
 * 
 * SPI_Config_t MySPI = {SPI_CHANNEL1, SPI_CPHA_1EDGE, SPI_CPOL_LOW, SPI_MODE_MASTER,
 *                       SPI_BAUDRATEPRESCALER_2, SPI_SPE_EN, SPI_MSBFIRST, SPI_NSS_SOFTWARE,
 *                       SPI_NSSI_NOT_SELECT, SPI_RXONLY_DISABLE, SPI_DFF_8BIT, SPI_CRCDIS,
 *                       SPI_CRCNEXT_DISABLE, SPI_UNIDIMODE, SPI_OUTPUT_EN};
 * SPI_enumInit(&MySPI);
 */
ErrorState_t SPI_enumInit(SPI_Config_t *ChannelConfig);
/*==================================================================================================*/
/**
 * @brief Perform full-duplex SPI transaction (transmit and receive)
 *
 * This function sends data and simultaneously receives data through the SPI interface.
 * It implements a blocking mechanism with timeout protection.
 *
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @param TX_Data Data to be transmitted
 * @param RX_Data Pointer to store received data
 * @return ErrorState_t OK if initialization successful, error code otherwise
 *
 * @warning NULL pointer check is performed on the input parameter.
 * @par Example:
 * 
 * uint16_t rx_data;
 * SPI_enumTrancieve(&MySPI, 0x55, &rx_data);
 */
ErrorState_t SPI_enumTrancieve(SPI_Config_t *ChannelConfig, uint16_t TX_Data, uint16_t *RX_Data);
/*==================================================================================================*/
/**
 * @brief Perform full-duplex SPI transaction (transmit and receive)
 *
 * This function sends data through the SPI interface.
 * It implements a blocking mechanism with timeout protection.
 *
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @param TX_Data Data to be transmitted
 * @return ErrorState_t OK if initialization successful, error code otherwise
 *
 * @warning NULL pointer check is performed on the input parameter.
 * @par Example:
 * SPI_enumTransmit(&MySPI, 0x55);
 */
ErrorState_t SPI_enumTransmit(SPI_Config_t *ChannelConfig, uint16_t TX_Data);
/*==================================================================================================*/
/**
 * @brief Perform full-duplex SPI transaction (transmit and receive)
 *
 * This function receives data through the SPI interface.
 * It implements a blocking mechanism with timeout protection.
 *
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @param RX_Data Pointer to store received data
 * @return ErrorState_t OK if initialization successful, error code otherwise
 *
 * @warning NULL pointer check is performed on the input parameter.
 * @par Example:
 * 
 * uint16_t data;
 * SPI_enumReceive(&MySPI, &data);
 */
ErrorState_t SPI_enumReceive(SPI_Config_t *ChannelConfig, uint16_t *RX_Data);


#endif /* _SPI_INTERFACE_H_ */
