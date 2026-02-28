/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<     US_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Saleh                                  **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : US (Ultrasonic Distance Sensor)                 **
 **                                                                           **
 **===========================================================================**
 */

#ifndef US_INTERFACE_H_
#define US_INTERFACE_H_

#include "STD_MACROS.h"
#include "ErrTypes.h"
#include "STM32F446xx.h"

#include "GPIO_interface.h"
#include "TIM_interface.h"
/**************************************         Data Types
 * ******************************************/

/**
 * @struct US_Config_t
 * @brief  Configuration structure for a single Ultrasonic sensor instance.
 *
 * @note   Each sensor MUST be assigned its own unique Timer (TIM_TIMER2..TIM_TIMER8).
 *         This allows multiple sensors to operate simultaneously without conflict.
 *         TRIG and ECHO can be ANY available GPIO pin — no pin restriction.
 *
 * @example
 *   US_Config_t mySensor = {
 *       .Timer    = TIM_TIMER2,
 *       .TrigPort = GPIO_PORTA,
 *       .TrigPin  = GPIO_PIN0,
 *       .EchoPort = GPIO_PORTA,
 *       .EchoPin  = GPIO_PIN1
 *   };
 */
typedef struct
{
    TIM_Num_t      Timer;     /* Timer instance (TIM_TIMER1..TIM_TIMER8)       */
    TIM_Channel_t  Channel;   /* Timer Channel: TIM_CHANNEL1..TIM_CHANNEL4     */
    GPIO_Port_t    TrigPort;  /* GPIO Port of TRIG pin                         */
    GPIO_Pin_t     TrigPin;   /* GPIO Pin  of TRIG pin                         */
    GPIO_Port_t    EchoPort;  /* GPIO Port of ECHO pin (Must match Timer AF)   */
    GPIO_Pin_t     EchoPin;   /* GPIO Pin  of ECHO pin (Must match Timer AF)   */
} US_Config_t;

/**************************************         Function Prototypes
 * ******************************************/

/**
 * @fn      US_vInit
 * @brief   Initialize GPIO pins and Timer for the given ultrasonic sensor.
 * @param   pxSensor: Pointer to sensor configuration structure.
 * @return  ErrorState_t: OK if successful, NULL_POINTER / NOK on error.
 *
 * @note    - TRIG pin → Output Push-Pull
 *          - ECHO pin → Input Floating (No Pull)
 *          - RCC clock for GPIO port(s) and Timer must be enabled BEFORE calling this.
 *
 * @example
 *   US_Config_t sensor = {TIM_TIMER2, GPIO_PORTA, GPIO_PIN0, GPIO_PORTA, GPIO_PIN1};
 *   US_vInit(&sensor);
 */
ErrorState_t US_vInit(const US_Config_t *pxSensor);

/**
 * @fn      US_u16ReadDistance_cm
 * @brief   Trigger sensor and measure distance in centimeters (blocking).
 * @param   pxSensor:    Pointer to sensor configuration structure.
 * @param   pu16Dist_cm: Pointer to store the measured distance (cm).
 * @return  ErrorState_t: OK if successful,
 *                        TIMEOUT_STATE if no echo received (no object / out of range),
 *                        NULL_POINTER if any pointer is NULL,
 *                        NOK if invalid parameters.
 *
 * @note    - Effective range: ~2 cm to ~400 cm.
 *          - This function is BLOCKING.
 *
 * @example
 *   uint16_t dist;
 *   if (US_u16ReadDistance_cm(&sensor, &dist) == OK)
 *   {
 *       // use dist
 *   }
 */
ErrorState_t US_u16ReadDistance_cm(const US_Config_t *pxSensor, uint16_t *pu16Dist_cm);

#endif /* US_INTERFACE_H_ */
