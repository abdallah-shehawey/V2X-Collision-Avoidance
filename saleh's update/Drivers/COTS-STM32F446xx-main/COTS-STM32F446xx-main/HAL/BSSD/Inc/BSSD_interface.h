/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    BSSD_interface.h      >>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : BSSD                                            **
 **                                                                           **
 **===========================================================================**
 */

#ifndef BSSD_INTERFACE_H_
#define BSSD_INTERFACE_H_

#include <stdint.h>

typedef enum
{
  SSD_COMMON_CATHODE = 0,
  SSD_COMMON_ANODE = 1
} SSD_Type_t;

typedef struct
{
  SSD_Type_t Type;        /* SSD_COMMON_CATHODE or SSD_COMMON_ANODE */
  GPIO_Port_t DataPort;   /* GPIO Port for BCD data pins */
  GPIO_Pin_t StartPin;    /* Starting pin for BCD data (4 consecutive pins will be used) */
  GPIO_Port_t EnablePort; /* GPIO Port for enable pin */
  GPIO_Pin_t EnablePin;   /* GPIO Pin for enable */
} SSD_Config_t;

void SSD_vInit(const SSD_Config_t *Config);
void SSD_vDisplayNumber(const SSD_Config_t *Config, uint8_t Copy_u8Number);
void SSD_vEnable(const SSD_Config_t *Config);
void SSD_vDisable(const SSD_Config_t *Config);

#endif /* BSSD_INTERFACE_H_ */
