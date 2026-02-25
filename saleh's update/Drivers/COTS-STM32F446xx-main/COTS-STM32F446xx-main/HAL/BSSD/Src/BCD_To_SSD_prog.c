/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    BSSD_program.c    >>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : BSSD                                            **
 **                                                                           **
 **===========================================================================**
 */

#include "ErrTypes.h"
#include "STD_MACROS.h"

#include "GPIO_interface.h"

#include "../Inc/BSSD_interface.h"
#include "../Inc/BSSD_config.h"
#include "../Inc/BSSD_private.h"

/*___________________________________________________________________________________________________________________*/

/*
 * Breif : This Function initialize the port which connected to 7 Seg leds as output pins ( 4 Pins or Port )
 * Parameters :
            =>Copy_u8PORT --> Port Name [ SSD_PORTA ,	SSD_PORTB , SSD_PORTC , SSD_PORTD ]
 * return : void
 */
void SSD_vInit(const SSD_Config_t *Config)
{
  /* Configure BCD data pins */
  GPIO_4BinsConfig_t DataConfig = {
      .Port = Config->DataPort,
      .StartPin = Config->StartPin,
      .Mode = GPIO_OUTPUT,
      .Otype = GPIO_PUSH_PULL,
      .Speed = GPIO_MEDIUM_SPEED,
      .PullType = GPIO_NO_PULL};

  GPIO_enumPort4BitsInit(&DataConfig);

  /* Configure enable pin */
  GPIO_PinConfig_t EnableConfig = {
      .Port = Config->EnablePort,
      .PinNum = Config->EnablePin,
      .Mode = GPIO_OUTPUT,
      .Otype = GPIO_PUSH_PULL,
      .Speed = GPIO_MEDIUM_SPEED,
      .PullType = GPIO_NO_PULL};

  GPIO_enumPinInit(&EnableConfig);
}

/*___________________________________________________________________________________________________________________*/

/*
 * Breif : This Function write Number on 7 seg [ 0 : 9 ]
 * Parameters : => struct has the SSD type , data port and enable(port & pin)
 * return : void
 */
void SSD_vDisplayNumber(const SSD_Config_t *Config, uint8_t Copy_u8Number)
{
  /* Ensure number is between 0-9 */
  Copy_u8Number &= 0x0F;
  if (Copy_u8Number > 9)
    Copy_u8Number = 0;

  /* Write BCD value */
  GPIO_enumWrite4BitsVal(Config->DataPort, Config->StartPin, Copy_u8Number);
}

/*___________________________________________________________________________________________________________________*/

/*
 * Breif : This Function enable common pin
 * Parameters : => struct has the SSD type , data port and enable(port & pin)
 * return : void
 */
void SSD_vEnable(const SSD_Config_t *Config)
{
  if (Config->Type == SSD_COMMON_CATHODE)
  {
    GPIO_enumWritePinVal(Config->EnablePort, Config->EnablePin, GPIO_PIN_LOW);
  }
  else /* SSD_COMMON_ANODE */
  {
    GPIO_enumWritePinVal(Config->EnablePort, Config->EnablePin, GPIO_PIN_HIGH);
  }
}

/*___________________________________________________________________________________________________________________*/

/*
 * Breif : This Function disable common pin
 * Parameters : => struct has the SSD type , data port and enable(port & pin)
 * return : void
 */
void SSD_vDisable(const SSD_Config_t *Config)
{
  if (Config->Type == SSD_COMMON_CATHODE)
  {
    GPIO_enumWritePinVal(Config->EnablePort, Config->EnablePin, GPIO_PIN_HIGH);
  }
  else /* SSD_COMMON_ANODE */
  {
    GPIO_enumWritePinVal(Config->EnablePort, Config->EnablePin, GPIO_PIN_LOW);
  }
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
