/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    TFT_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : TFT                                             **
 **                                                                           **
 **===========================================================================**
 */
#include "ErrTypes.h"
#include "GPIO_interface.h"
#include "SPI_interface.h"
#include "SYSTIC_interface.h"

#include "TFT_config.h"
#include "TFT_private.h"
#include "TFT_interface.h"


void HTFT_vinit(SPI_Config_t *ChannelConfig)
{
  GPIO_enumPinInit(&RST_PIN);
  GPIO_enumPinInit(&A0_PIN);

  /* TFT Reset Sequence */
  GPIO_enumWritePinVal(TFT_PORT, TFT_RST_PIN, GPIO_PIN_HIGH);
  SYSTIC_vDelayUs(100);
  GPIO_enumWritePinVal(TFT_PORT, TFT_RST_PIN, GPIO_PIN_LOW);
  SYSTIC_vDelayUs(1);
  GPIO_enumWritePinVal(TFT_PORT, TFT_RST_PIN, GPIO_PIN_HIGH);
  SYSTIC_vDelayUs(100);
  GPIO_enumWritePinVal(TFT_PORT, TFT_RST_PIN, GPIO_PIN_LOW );
  SYSTIC_vDelayUs(100);
  GPIO_enumWritePinVal(TFT_PORT, TFT_RST_PIN, GPIO_PIN_HIGH);
  SYSTIC_vDelayMs(120);

  /* Sleep out */
  HTFT_vSendCmd(ChannelConfig, SLEEP_OUT);
  SYSTIC_vDelayMs(10);

  /* Select Color Mode */
  HTFT_vSendCmd(ChannelConfig, COLOR_MODE);
  HTFT_vSendData(ChannelConfig, RGB565);

  /* Display on */
  HTFT_vSendCmd(ChannelConfig, DISPLAY_ON);
}



void HTFT_vSendCmd(SPI_Config_t *ChannelConfig, uint8_t Copy_u8Cmd)
{
  GPIO_enumWritePinVal(TFT_PORT, TFT_A0_PIN, GPIO_PIN_LOW);
  SPI_enumTransmit(ChannelConfig, Copy_u8Cmd);
}


void HTFT_vSendData(SPI_Config_t *ChannelConfig, uint8_t Copy_u8Data)
{
  GPIO_enumWritePinVal(TFT_PORT, TFT_A0_PIN, GPIO_PIN_HIGH);
  SPI_enumTransmit(ChannelConfig, Copy_u8Data);
}


void HTFT_vDisplay(SPI_Config_t *ChannelConfig, const uint16_t *Copy_pu16Ptr)
{
  uint16_t Local_u16Iterator;
  uint8_t Local_u8HighPart, Local_u8LowPart;

  HTFT_vSendCmd (ChannelConfig, X_DIR);
  HTFT_vSendData(ChannelConfig, START_X_B0);
  HTFT_vSendData(ChannelConfig, START_X_B1);
  HTFT_vSendData(ChannelConfig, END_X_B0);
  HTFT_vSendData(ChannelConfig, END_X_B1);

  HTFT_vSendCmd (ChannelConfig, Y_DIR);
  HTFT_vSendData(ChannelConfig, START_Y_B0);
  HTFT_vSendData(ChannelConfig, START_Y_B1);
  HTFT_vSendData(ChannelConfig, END_Y_B0);
  HTFT_vSendData(ChannelConfig, END_Y_B1);

  HTFT_vSendCmd(ChannelConfig, SCREEN_WRITE);
  for (Local_u16Iterator = 0; Local_u16Iterator < IMAGE_SIZE; Local_u16Iterator++)
  {
    Local_u8LowPart = (uint8_t)Copy_pu16Ptr[Local_u16Iterator];
    Local_u8HighPart = (uint8_t)(Copy_pu16Ptr[Local_u16Iterator] >> 8);
    HTFT_vSendData(ChannelConfig, Local_u8HighPart);
    HTFT_vSendData(ChannelConfig, Local_u8LowPart);
  }
}
