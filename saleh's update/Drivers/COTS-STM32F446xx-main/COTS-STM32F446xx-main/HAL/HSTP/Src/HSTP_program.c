/*
 * HSTP_program.c
 *
 *  Created on: Aug 3, 2025
 *      Author: abdallah-shehawey
 */
#include "stdint.h"
#include "ErrTypes.h"

#include "GPIO_interface.h"

#include "HSTP_config.h"
#include "HSTP_private.h"
#include "HSTP_interface.h"


ErrorState_t HSTP_enumInit(void)
{
  ErrorState_t Local_ErrorState = OK;
  Local_ErrorState = GPIO_enumPinInit(&HSTP_DS);
  Local_ErrorState = GPIO_enumPinInit(&HSTP_SHCR);
  Local_ErrorState = GPIO_enumPinInit(&HSTP_STCR);
  return Local_ErrorState;
}

void HSTP_vSendData(uint8_t Copy_u8Data)
{
  HSTP_vShiftData(Copy_u8Data);
  GPIO_enumWritePinVal(HSTP_STCR_PORT, HSTP_STCR_PIN, GPIO_PIN_LOW);

  GPIO_enumWritePinVal(HSTP_STCR_PORT, HSTP_STCR_PIN, GPIO_PIN_HIGH);
}

void HSTP_vShiftData(uint8_t Copy_u8Data)
{
  uint8_t Local_u8Iterator = 0;
  for (Local_u8Iterator = 0; Local_u8Iterator < 8; Local_u8Iterator++)
  {
    GPIO_enumWritePinVal(HSTP_DS_PORT, HSTP_DS_PIN, Copy_u8Data & (1 << Local_u8Iterator));
    GPIO_enumWritePinVal(HSTP_SHCR_PORT, HSTP_SHCR_PIN, GPIO_PIN_LOW);

    GPIO_enumWritePinVal(HSTP_SHCR_PORT, HSTP_SHCR_PIN, GPIO_PIN_HIGH);
  }
}



