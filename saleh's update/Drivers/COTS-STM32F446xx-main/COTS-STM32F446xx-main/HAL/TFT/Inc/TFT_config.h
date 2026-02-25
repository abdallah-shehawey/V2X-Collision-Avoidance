/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    TFT_config.h     >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : TFT                                             **
 **                                                                           **
 **===========================================================================**
 */
#ifndef _TFT_CONFIG_H_
#define _TFT_CONFIG_H_


#define TFT_PORT    GPIO_PORTA
#define TFT_RST_PIN GPIO_PIN0
#define TFT_A0_PIN  GPIO_PIN1

/* Choose RST PIN */
GPIO_PinConfig_t RST_PIN =
{
  .Port     = TFT_PORT,
  .PinNum = TFT_RST_PIN,
  .Mode   = GPIO_OUTPUT,
  .Otype  = GPIO_PUSH_PULL,
  .PullType = GPIO_NO_PULL,
  .Speed    = GPIO_MEDIUM_SPEED,
};
/* Choose Ctrl PIN */
GPIO_PinConfig_t A0_PIN =
{
  .Port     = TFT_PORT,
  .PinNum = TFT_A0_PIN,
  .Mode   = GPIO_OUTPUT,
  .Otype  = GPIO_PUSH_PULL,
  .PullType = GPIO_NO_PULL,
  .Speed    = GPIO_MEDIUM_SPEED,
};

#endif /* _TFT_CONFIG_H_ */
