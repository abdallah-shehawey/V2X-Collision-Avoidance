/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SSD_private.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : SSD                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef SSD_PRIVATE_H_
#define SSD_PRIVATE_H_

/* Seven segment display patterns for common cathode (active high) */
#define SSD_NUMBER_PATTERNS {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F}

/* GPIO Configuration for SSD data pins */
#define SSD_DATA_MODE GPIO_OUTPUT
#define SSD_DATA_TYPE GPIO_PUSH_PULL
#define SSD_DATA_SPEED GPIO_MEDIUM_SPEED
#define SSD_DATA_PULL GPIO_NO_PULL

/* GPIO Configuration for SSD enable pin */
#define SSD_ENABLE_MODE GPIO_OUTPUT
#define SSD_ENABLE_TYPE GPIO_PUSH_PULL
#define SSD_ENABLE_SPEED GPIO_MEDIUM_SPEED
#define SSD_ENABLE_PULL GPIO_NO_PULL

#endif /* SSD_PRIVATE_H_ */
