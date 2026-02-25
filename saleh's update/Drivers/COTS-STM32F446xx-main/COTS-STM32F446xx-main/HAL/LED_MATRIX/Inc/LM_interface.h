/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    LM_interface.h     >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : LED_MATRIX                                      **
 **                                                                           **
 **===========================================================================**
 */
#ifndef LM_INTERFACE_H_
#define LM_INTERFACE_H_

void HLEDMATRIX_vInit();
void HLEDMATRIX_vDisplay(uint8_t *Copy_pu8Arr);

#endif /* LM_INTERFACE_H_ */