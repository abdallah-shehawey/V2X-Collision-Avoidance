/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    L298N_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Saleh                                  **
 **                  Date   : 2026-03-30                                      **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : L298N MOTOR DRIVER                              **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Drivers/LIB/STM32F446xx.h"
#include "../Inc/Drivers/LIB/STD_MACROS.h"
#include "../Inc/Drivers/LIB/ErrTypes.h"
#include "../Inc/Drivers/MCAL/GPIO/GPIO_interface.h"

#include "../Inc/Drivers/HAL/L298N/L298N_interface.h"
#include "../Inc/Drivers/HAL/L298N/L298N_config.h"
#include "../Inc/Drivers/HAL/L298N/L298N_private.h"

/*******************************************************************************
 *                        Single Motor Control APIs                            *
 *******************************************************************************/

ErrorState_t L298N_enumInit(const L298N_MotorConfig_t* pxMotorConfig)
{
    ErrorState_t Local_ErrorState = OK;   
    if (pxMotorConfig != NULL)
    {
        GPIO_PinConfig_t pinConfig = {
            .Mode = GPIO_OUTPUT,
            .Otype = GPIO_PUSH_PULL,
            .Speed = GPIO_MEDIUM_SPEED,
            .PullType = GPIO_NO_PULL,
            .AlternateFunction = GPIO_AF0
        };

        /* Configure EN Pin */
        pinConfig.Port = pxMotorConfig->EN_Port;
        pinConfig.PinNum = pxMotorConfig->EN_Pin;
        GPIO_enumPinInit(&pinConfig);

        /* Configure IN1 Pin */
        pinConfig.Port = pxMotorConfig->IN1_Port;
        pinConfig.PinNum = pxMotorConfig->IN1_Pin;
        GPIO_enumPinInit(&pinConfig);

        /* Configure IN2 Pin */
        pinConfig.Port = pxMotorConfig->IN2_Port;
        pinConfig.PinNum = pxMotorConfig->IN2_Pin;
        GPIO_enumPinInit(&pinConfig);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumMoveForward(const L298N_MotorConfig_t* pxMotorConfig)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxMotorConfig != NULL)
    {
        /* Enable Motor, IN1 HIGH, IN2 LOW */
        GPIO_enumWritePinVal(pxMotorConfig->EN_Port, pxMotorConfig->EN_Pin, GPIO_PIN_HIGH);
        GPIO_enumWritePinVal(pxMotorConfig->IN1_Port, pxMotorConfig->IN1_Pin, GPIO_PIN_HIGH);
        GPIO_enumWritePinVal(pxMotorConfig->IN2_Port, pxMotorConfig->IN2_Pin, GPIO_PIN_LOW);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumMoveBackward(const L298N_MotorConfig_t* pxMotorConfig)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxMotorConfig != NULL)
    {
        /* Enable Motor, IN1 LOW, IN2 HIGH */
        GPIO_enumWritePinVal(pxMotorConfig->EN_Port, pxMotorConfig->EN_Pin, GPIO_PIN_HIGH);
        GPIO_enumWritePinVal(pxMotorConfig->IN1_Port, pxMotorConfig->IN1_Pin, GPIO_PIN_LOW);
        GPIO_enumWritePinVal(pxMotorConfig->IN2_Port, pxMotorConfig->IN2_Pin, GPIO_PIN_HIGH);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumStop(const L298N_MotorConfig_t* pxMotorConfig)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxMotorConfig != NULL)
    {
        /* Disable Motor, IN1 LOW, IN2 LOW */
        GPIO_enumWritePinVal(pxMotorConfig->EN_Port, pxMotorConfig->EN_Pin, GPIO_PIN_LOW);
        GPIO_enumWritePinVal(pxMotorConfig->IN1_Port, pxMotorConfig->IN1_Pin, GPIO_PIN_LOW);
        GPIO_enumWritePinVal(pxMotorConfig->IN2_Port, pxMotorConfig->IN2_Pin, GPIO_PIN_LOW);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

/*******************************************************************************
 *                          Two Motors (Car) APIs                              *
 *******************************************************************************/

ErrorState_t L298N_enumCarMoveForward(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxRightMotor != NULL && pxLeftMotor != NULL)
    {
        L298N_enumMoveForward(pxRightMotor);
        L298N_enumMoveForward(pxLeftMotor);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumCarMoveBackward(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxRightMotor != NULL && pxLeftMotor != NULL)
    {
        L298N_enumMoveBackward(pxRightMotor);
        L298N_enumMoveBackward(pxLeftMotor);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumCarStop(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxRightMotor != NULL && pxLeftMotor != NULL)
    {
        L298N_enumStop(pxRightMotor);
        L298N_enumStop(pxLeftMotor);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumCarMoveRight(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxRightMotor != NULL && pxLeftMotor != NULL)
    {
        /* Right rotation usually means Left Motor Forward, Right Motor Backward */
        L298N_enumMoveBackward(pxRightMotor);
        L298N_enumMoveForward(pxLeftMotor);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}

ErrorState_t L298N_enumCarMoveLeft(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor)
{
    ErrorState_t Local_ErrorState = OK;
    if (pxRightMotor != NULL && pxLeftMotor != NULL)
    {
        /* Left rotation usually means Right Motor Forward, Left Motor Backward */
        L298N_enumMoveForward(pxRightMotor);
        L298N_enumMoveBackward(pxLeftMotor);
    }
    else
    {
        Local_ErrorState = NULL_POINTER;
    }
    return Local_ErrorState;
}
