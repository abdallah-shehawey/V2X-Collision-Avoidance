/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    LM_program.c     >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : LED_MATRIX                                        **
 **                                                                           **
 **===========================================================================**
 */

#include "GPIO_interface.h"
#include "SYSTIC_interface.h"
#include "ErrTypes.h"

#include "LM_interface.h"
#include "LM_config.h"
#include "LM_private.h"

void HLEDMATRIX_vInit()
{
  GPIO_enumHalfPortInit(&LED_MATRIC_ROW);
  GPIO_enumPort8PinsInit(&LED_MATRIX_COL);
  GPIO_enumWrite8PinsVal(LED_MATRIX_COL_PORT, LED_MATRIX_COL_START_PIN, 0XFF);
}


void HLEDMATRIX_vDisplay(uint8_t *Copy_pu8Arr)
{
  for(uint8_t Local_u8Counter = 0; Local_u8Counter < 8; Local_u8Counter++)
  {
    GPIO_enumWrite8PinsVal(LED_MATRIX_ROW_PORT, LED_MATRIX_ROW_START_PIN, Copy_pu8Arr[Local_u8Counter]);
    GPIO_enumWritePinVal(LED_MATRIX_COL_PORT, Local_u8Counter, GPIO_PIN_LOW);
    SYSTIC_vDelayUs(2500);
    GPIO_enumWritePinVal(LED_MATRIX_COL_PORT, Local_u8Counter, GPIO_PIN_HIGH);
  }
}
