/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    TFT_interface.h  >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : TFT                                             **
 **                                                                           **
 **===========================================================================**
 */
#ifndef _TFT_INTERFACE_H_
#define _TFT_INTERFACE_H_

#define SLEEP_OUT    0X11
#define COLOR_MODE   0X3A
#define DISPLAY_ON   0X29
#define X_DIR        0X2A
#define Y_DIR        0X2B
#define SCREEN_WRITE 0X2C

#define RGB565       0X05

/* Start & end x */
#define START_X_B0 0
#define START_X_B1 0
#define END_X_B0   0
#define END_X_B1   127

/* Start & end Y */
#define START_Y_B0 0
#define START_Y_B1 0
#define END_Y_B0   0
#define END_Y_B1   159

#define IMAGE_SIZE 20480

void HTFT_vinit(SPI_Config_t *ChannelConfig);
void HTFT_vSendCmd(SPI_Config_t *ChannelConfig, uint8_t Copy_u8Cmd);
void HTFT_vSendData(SPI_Config_t *ChannelConfig, uint8_t Copy_u8Data);
void HTFT_vDisplay(SPI_Config_t *ChannelConfig, const uint16_t *Copy_pu16Ptr);

#endif /* _TFT_INTERFACE_H_ */
