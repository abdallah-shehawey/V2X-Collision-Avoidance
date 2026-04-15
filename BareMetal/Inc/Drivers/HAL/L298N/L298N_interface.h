/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    L298N_interface.h  >>>>>>>>>>>>>>>>>>>>>>>>>>**
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

#ifndef L298N_INTERFACE_H_
#define L298N_INTERFACE_H_

#include "../../LIB/ErrTypes.h"
#include "../../MCAL/GPIO/GPIO_interface.h"

/**
 * @struct L298N_MotorConfig_t
 * @brief  Configuration structure for a single motor connected to L298N.
 *         It allows dynamic configuration of any motor pins.
 */
typedef struct {
    GPIO_Port_t EN_Port;  /* Enable Pin Port (ENA or ENB) */
    GPIO_Pin_t  EN_Pin;   /* Enable Pin Number */
    
    GPIO_Port_t IN1_Port; /* Direction Pin 1 Port (IN1 or IN3) */
    GPIO_Pin_t  IN1_Pin;  /* Direction Pin 1 Number */
    
    GPIO_Port_t IN2_Port; /* Direction Pin 2 Port (IN2 or IN4) */
    GPIO_Pin_t  IN2_Pin;  /* Direction Pin 2 Number */
} L298N_MotorConfig_t;

/*******************************************************************************
 *                        Single Motor Control APIs                            *
 *******************************************************************************/

/**
 * @brief  Initialize a motor's GPIO pins according to the configuration.
 * @example L298N_enumInit(&RightMotor);
 */
ErrorState_t L298N_enumInit(const L298N_MotorConfig_t* pxMotorConfig);

/**
 * @brief  Move a single motor forward.
 * @example L298N_enumMoveForward(&RightMotor);
 */
ErrorState_t L298N_enumMoveForward(const L298N_MotorConfig_t* pxMotorConfig);

/**
 * @brief  Move a single motor backward.
 * @example L298N_enumMoveBackward(&RightMotor);
 */
ErrorState_t L298N_enumMoveBackward(const L298N_MotorConfig_t* pxMotorConfig);

/**
 * @brief  Stop a single motor.
 * @example L298N_enumStop(&RightMotor);
 */
ErrorState_t L298N_enumStop(const L298N_MotorConfig_t* pxMotorConfig);


/*******************************************************************************
 *                          Two Motors (Car) APIs                              *
 *******************************************************************************/

/**
 * @brief  Move the car forward (Both motors forward).
 * @example L298N_enumCarMoveForward(&RightMotor, &LeftMotor);
 */
ErrorState_t L298N_enumCarMoveForward(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor);

/**
 * @brief  Move the car backward (Both motors backward).
 * @example L298N_enumCarMoveBackward(&RightMotor, &LeftMotor);
 */
ErrorState_t L298N_enumCarMoveBackward(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor);

/**
 * @brief  Stop the car (Both motors stop).
 * @example L298N_enumCarStop(&RightMotor, &LeftMotor);
 */
ErrorState_t L298N_enumCarStop(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor);

/**
 * @brief  Steer the car right (Left motor forward, Right motor backward).
 * @example L298N_enumCarMoveRight(&RightMotor, &LeftMotor);
 */
ErrorState_t L298N_enumCarMoveRight(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor);

/**
 * @brief  Steer the car left (Right motor forward, Left motor backward).
 * @example L298N_enumCarMoveLeft(&RightMotor, &LeftMotor);
 */
ErrorState_t L298N_enumCarMoveLeft(const L298N_MotorConfig_t* pxRightMotor, const L298N_MotorConfig_t* pxLeftMotor);

#endif /* L298N_INTERFACE_H_ */
