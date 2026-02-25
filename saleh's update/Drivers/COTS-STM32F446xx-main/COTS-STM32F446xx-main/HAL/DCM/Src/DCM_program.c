/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    DCM_program.c    >>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : STM32F446RE                                     **
 **                  SWC    : DC_MOTOR                                         **
 **                                                                           **
 **===========================================================================**
 */
#include <stdint.h>

#include "../../MCAL/GPIO/Inc/GPIO_interface.h"
#include "ErrTypes.h"
#include "STD_MACROS.h"

#include "../Inc/DCM_interface.h"

ErrorState_t DCMOTOR_Init(const DCMOTOR_Config_t *DCMOTOR_Config)
{
	ErrorState_t Local_ErrorState = OK;

	if (DCMOTOR_Config != NULL)
	{
		/* Configure both pins as output push-pull */
		GPIO_PinConfig_t PinConfig;

		/* Common configuration for both pins */
		PinConfig.Port = DCMOTOR_Config->Port;
		PinConfig.Mode = GPIO_OUTPUT;
		PinConfig.Otype = GPIO_PUSH_PULL;
		PinConfig.Speed = GPIO_MEDIUM_SPEED;
		PinConfig.PullType = GPIO_NO_PULL;

		/* Configure Pin A */
		PinConfig.PinNum = DCMOTOR_Config->PinA;
		Local_ErrorState = GPIO_enumPinInit(&PinConfig);

		if (Local_ErrorState == OK)
		{
			/* Configure Pin B */
			PinConfig.PinNum = DCMOTOR_Config->PinB;
			Local_ErrorState = GPIO_enumPinInit(&PinConfig);
		}
	}
	else
	{
		Local_ErrorState = NULL_POINTER;
	}

	return Local_ErrorState;
}

ErrorState_t DCMOTOR_Control(const DCMOTOR_Config_t *DCMOTOR_Config, uint8_t Copy_uint8State)
{
	ErrorState_t Local_ErrorState = OK;

	if (DCMOTOR_Config != NULL)
	{
		switch (Copy_uint8State)
		{
		case DCMOTOR_CW:
			GPIO_enumWritePinVal(DCMOTOR_Config->Port, DCMOTOR_Config->PinA, GPIO_PIN_LOW);
			GPIO_enumWritePinVal(DCMOTOR_Config->Port, DCMOTOR_Config->PinB, GPIO_PIN_HIGH);

			break;

		case DCMOTOR_CCW:
			GPIO_enumWritePinVal(DCMOTOR_Config->Port, DCMOTOR_Config->PinA, GPIO_PIN_HIGH);
			GPIO_enumWritePinVal(DCMOTOR_Config->Port, DCMOTOR_Config->PinB, GPIO_PIN_LOW);
			break;

		case DCMOTOR_STOP:
			GPIO_enumWritePinVal(DCMOTOR_Config->Port, DCMOTOR_Config->PinA, GPIO_PIN_LOW);
			GPIO_enumWritePinVal(DCMOTOR_Config->Port, DCMOTOR_Config->PinB, GPIO_PIN_LOW);
			break;

		default:
			Local_ErrorState = NOK;
			break;
		}
	}
	else
	{
		Local_ErrorState = NULL_POINTER;
	}

	return Local_ErrorState;
}
