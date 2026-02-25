/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SPI_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SPI                                             **
 **                                                                           **
 **===========================================================================**
 */
#include "stdint.h"
#include "ErrTypes.h"
#include "STM32F446xx.h"

#include "SPI_private.h"
#include "SPI_config.h"
#include "SPI_interface.h"

/* Array of SPI port register definitions for easy access */
static SPI_RegDef_t *SPI_Channel[SPI_CHANNEL_COUNT] = {MSPI1, MSPI2, MSPI3, MSPI4};
/*Global flag for the SPI Busy State*/
static uint8_t SPI_u8State = IDLE;


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
 */
ErrorState_t SPI_enumInit(SPI_Config_t *ChannelConfig)
{
  ErrorState_t Local_u8ErrorState = OK;

  // Check for NULL pointer input
  if (ChannelConfig == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    // Validate channel number
    if(ChannelConfig->Channel > SPI_CHANNEL_COUNT)
    {
      Local_u8ErrorState = NOK;
    }
    else
    {
      // Configure CLOCK PHASE (CPHA)
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->CPHA & 0X1) << CR1_CPHA;
      // Configure Clock Polarity (CPOL)
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->CPOL & 0X1) << CR1_CPOL;
      // Configure Master/Slave Mode
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->Mode & 0X1) << CR1_MSTR;
      // Configure Baud Rate Prescaler
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->BaudRatePrescaler & 0X7) << CR1_BR;
      // Configure Bit Order (MSB/LSB first)
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->BFIRST & 0X1) << CR1_LSBFIRST;
      // Configure NSS Management (Hardware/Software)
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->NSS_MAN & 0X1) << CR1_SSM;
      // If using Software NSS, configure NSS Mode
      if (ChannelConfig->NSS_MAN == SPI_NSS_SOFTWARE)
      {
        SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->NSSI_MODE & 0X1) << CR1_SSI;
      }
      // Configure Receive Only Mode
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->RXONLY & 0X1) << CR1_RXONLY;
      // Configure Data Frame Format (8/16 bit)
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->DFF & 0X1) << CR1_DFF;
      // Configure CRC Mode
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->CRC_MODE & 0X1) << CR1_CRCEN;
      // If CRC is enabled, configure CRC Next
      if (ChannelConfig->CRC_MODE == SPI_CRCEN)
      {
        SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->CRCNEXT & 0X1) << CR1_CRCNEXT;
      }
      // Configure Bidirectional Mode
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->DIMODE & 0X1) << CR1_BIDIMODE;
      // If Bidirectional mode is enabled, configure Output Enable
      if (ChannelConfig->DIMODE == SPI_BIDIMODE)
      {
        SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->BIDIOE & 0X1) << CR1_BIDIOE;
      }
      // Enable SPI peripheral
      SPI_Channel[ChannelConfig->Channel]->CR1 |= (ChannelConfig->SPE & 0X1) << CR1_SPE;
    }
  }
  return Local_u8ErrorState;
}

/**
 * @brief Perform full-duplex SPI transaction (transmit and receive)
 * 
 * This function sends data and simultaneously receives data through the SPI interface.
 * It implements a blocking mechanism with timeout protection.
 * 
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @param TX_Data Data to be transmitted
 * @param RX_Data Pointer to store received data
 * @return ErrorState_t OK if successful, error code otherwise
 * 
 * @note This is a blocking function with timeout protection
 * @warning NULL pointer check is performed on input parameters
 */
ErrorState_t SPI_enumTrancieve(SPI_Config_t *ChannelConfig, uint16_t TX_Data, uint16_t *RX_Data)
{
  ErrorState_t Local_u8ErrorState = OK;
  uint32_t Local_u32TimeoutCounter = 0;

  // Check for NULL pointers
  if (ChannelConfig == NULL || RX_Data == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    // Validate channel number
    if (ChannelConfig->Channel > SPI_CHANNEL_COUNT)
    {
      Local_u8ErrorState = NOK;
    }
    else
    {
      // Check if SPI is not busy
      if (SPI_u8State == IDLE)
      {
        SPI_u8State = BUSY;
        while((SPI_Channel[ChannelConfig->Channel]->SR & (1 << SR_TXE)) >> SR_TXE == 0 && (Local_u32TimeoutCounter != SPI_u32TIMEOUT))
        {
          Local_u32TimeoutCounter++;
        }
        
        // Check if timeout occurred
        if (Local_u32TimeoutCounter == SPI_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
        else
        {
          // Transmit data
          SPI_Channel[ChannelConfig->Channel]->DR = TX_Data;
        }
        
        // Reset timeout counter
        Local_u32TimeoutCounter = 0;
        while((SPI_Channel[ChannelConfig->Channel]->SR & (1 << SR_RXNE)) >> SR_RXNE == 0 && (Local_u32TimeoutCounter != SPI_u32TIMEOUT))
        {
          Local_u32TimeoutCounter++;
        }
        
        // Check if timeout occurred
        if (Local_u32TimeoutCounter == SPI_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
        else
        {
          // Read received data
          *RX_Data = SPI_Channel[ChannelConfig->Channel]->DR;
        }
        
        // Mark SPI as idle
        SPI_u8State = IDLE;
      }
    }
  }
  return Local_u8ErrorState;
}


/**
 * @brief Transmit data through SPI interface
 * 
 * This function sends data through the SPI interface in a blocking manner
 * with timeout protection.
 * 
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @param TX_Data Data to be transmitted
 * @return ErrorState_t OK if successful, error code otherwise
 * 
 * @warning NULL pointer check is performed on input parameter
 */
ErrorState_t SPI_enumTransmit(SPI_Config_t *ChannelConfig, uint16_t TX_Data)
{
  ErrorState_t Local_u8ErrorState = OK;
  uint32_t Local_u32TimeoutCounter = 0;

  // Check for NULL pointer
  if (ChannelConfig == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    // Validate channel number
    if (ChannelConfig->Channel > SPI_CHANNEL_COUNT)
    {
      Local_u8ErrorState = NOK;
    }
    else
    {
      // Check if SPI is not busy
      if (SPI_u8State == IDLE)
      {
        SPI_u8State = BUSY;
        while((SPI_Channel[ChannelConfig->Channel]->SR & (1 << SR_TXE)) >> SR_TXE == 0 && (Local_u32TimeoutCounter != SPI_u32TIMEOUT))
        {
          Local_u32TimeoutCounter++;
        }
        
        // Check if timeout occurred
        if (Local_u32TimeoutCounter == SPI_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
        else
        {
          // Transmit data
          SPI_Channel[ChannelConfig->Channel]->DR = TX_Data;
        }
        
        // Mark SPI as idle
        SPI_u8State = IDLE;
      }
    }
  }
  return Local_u8ErrorState;
}


/**
 * @brief Receive data through SPI interface
 * 
 * This function receives data through the SPI interface in a blocking manner
 * with timeout protection.
 * 
 * @param ChannelConfig Pointer to the SPI configuration structure
 * @param RX_Data Pointer to store received data
 * @return ErrorState_t OK if successful, error code otherwise
 * 
 * @warning NULL pointer check is performed on input parameters
 */
ErrorState_t SPI_enumReceive(SPI_Config_t *ChannelConfig, uint16_t *RX_Data)
{
  ErrorState_t Local_u8ErrorState = OK;
  uint32_t Local_u32TimeoutCounter = 0;

  // Check for NULL pointers
  if (ChannelConfig == NULL || RX_Data == NULL)
  {
    Local_u8ErrorState = NULL_POINTER;
  }
  else
  {
    // Validate channel number
    if (ChannelConfig->Channel > SPI_CHANNEL_COUNT)
    {
      Local_u8ErrorState = NOK;
    }
    else
    {
      // Check if SPI is not busy
      if (SPI_u8State == IDLE)
      {
        SPI_u8State = BUSY;
        while((SPI_Channel[ChannelConfig->Channel]->SR & (1 << SR_RXNE)) >> SR_RXNE == 0 && (Local_u32TimeoutCounter != SPI_u32TIMEOUT))
        {
          Local_u32TimeoutCounter++;
        }
        
        // Check if timeout occurred
        if (Local_u32TimeoutCounter == SPI_u32TIMEOUT)
        {
          Local_u8ErrorState = TIMEOUT_STATE;
        }
        else
        {
          // Read received data
          *RX_Data = SPI_Channel[ChannelConfig->Channel]->DR;
        }
        
        // Mark SPI as idle
        SPI_u8State = IDLE;
      }
    }
  }
  return Local_u8ErrorState;
}
