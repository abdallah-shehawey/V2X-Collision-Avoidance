/**
 **===========================================================================**
 **                     US_Test_main.c — Ultrasonic Test                      **
 **                                                                           **
 **   Author : Abdallah Saleh                                                 **
 **   MCU    : STM32F446RE (Nucleo-64)                                        **
 **   Brief  : Trigger Buzzer when distance is less than 5cm                  **
 **                                                                           **
 **   Wiring:                                                                  **
 **     Ultrasonic Sensor (US):                                               **
 **       VCC  -> 5V                                                           **
 **       GND  -> GND                                                          **
 **       TRIG -> PA0                                                          **
 **       ECHO -> PA1 (Use voltage divider if needed: 5V signals to 3.3V)      **
 **                                                                           **
 **     Buzzer (Active High):                                                  **
 **       + -> PB0                                                            **
 **       - -> GND                                                            **
 **                                                                           **
 **===========================================================================**
 */

#include <stdint.h>

/* LIB Includes */
#include "STM32F446xx.h"
#include "STD_MACROS.h"
#include "ErrTypes.h"

/* MCAL Includes */
#include "RCC_interface.h"
#include "GPIO_interface.h"
#include "TIM_interface.h"

/* HAL Includes */
#include "US_interface.h"
#include "BUZ_interface.h"

/*_______________________________________________________________________________*/
/*                         Sensor & Buzzer Configuration                         */
/*_______________________________________________________________________________*/

/**
 * @brief Select timer for testing. 
 * Options: TIM_TIMER1, TIM_TIMER2, TIM_TIMER3, TIM_TIMER4, TIM_TIMER5, TIM_TIMER8
 */
#define TEST_TIMER       TIM_TIMER1 

/* Pin Definitions */
#define US_TRIG_PORT     GPIO_PORTA
#define US_TRIG_PIN      GPIO_PIN0
#define US_ECHO_PORT     GPIO_PORTA
#define US_ECHO_PIN      GPIO_PIN1

#define BUZ_PORT         GPIO_PORTB
#define BUZ_PIN          GPIO_PIN0

/* Configuration Constants */
#define ALERT_DISTANCE_CM    5U
#define MEASURE_DELAY_MS     60U

/*_______________________________________________________________________________*/
/*                              Helper: Enable RCC                               */
/*_______________________________________________________________________________*/

/**
 * @brief Enable the RCC clock for the selected timer and GPIO ports.
 */
static void App_vEnableClocks(void)
{
    /* Enable GPIOA clock (AHB1 — bit 0) */
    SET_BIT(MRCC->AHP1ENR, 0);

    /* Enable GPIOB clock (AHB1 — bit 1) */
    SET_BIT(MRCC->AHP1ENR, 1);

    /* Enable Timer clock based on selection */
#if   (TEST_TIMER == TIM_TIMER2)
    SET_BIT(MRCC->APB1ENR, 0);   /* TIM2EN (APB1) */

#elif (TEST_TIMER == TIM_TIMER3)
    SET_BIT(MRCC->APB1ENR, 1);   /* TIM3EN (APB1) */

#elif (TEST_TIMER == TIM_TIMER4)
    SET_BIT(MRCC->APB1ENR, 2);   /* TIM4EN (APB1) */

#elif (TEST_TIMER == TIM_TIMER5)
    SET_BIT(MRCC->APB1ENR, 3);   /* TIM5EN (APB1) */

#elif (TEST_TIMER == TIM_TIMER1)
    SET_BIT(MRCC->APB2ENR, 0);   /* TIM1EN (APB2) */

#elif (TEST_TIMER == TIM_TIMER8)
    SET_BIT(MRCC->APB2ENR, 1);   /* TIM8EN (APB2) */
#endif
}

/*_______________________________________________________________________________*/
/*                                    Main                                       */
/*_______________________________________________________________________________*/

int main(void)
{
    /* 1. Initialize Peripheral Clocks */
    App_vEnableClocks();

    /* 2. Configure and Initialize Ultrasonic Sensor */
    US_Config_t Sensor = {
        .Timer    = TEST_TIMER,
        .TrigPort = US_TRIG_PORT,
        .TrigPin  = US_TRIG_PIN,
        .EchoPort = US_ECHO_PORT,
        .EchoPin  = US_ECHO_PIN
    };
    US_vInit(&Sensor);

    /* 3. Configure and Initialize Buzzer */
    BUZ_Config_t Buzzer = {
        .Port        = BUZ_PORT,
        .Pin         = BUZ_PIN,
        .ActiveState = ACTIVE_HIGH
    };
    BUZ_Init(&Buzzer);

    uint16_t     Local_u16Distance_cm = 0U;
    ErrorState_t Local_Status         = OK;

    while (1)
    {
        /* Perform Distance Measurement */
        Local_Status = US_u16ReadDistance_cm(&Sensor, &Local_u16Distance_cm);

        if (Local_Status == OK)
        {
            /* Check if an object is within the alert threshold */
            if (Local_u16Distance_cm < ALERT_DISTANCE_CM)
            {
                BUZ_On(&Buzzer);    /* Distance < 5cm -> Alarm ON  */
            }
            else
            {
                BUZ_Off(&Buzzer);   /* Distance >= 5cm -> Alarm OFF */
            }
        }
        else if (Local_Status == TIMEOUT_STATE)
        {
            /* No object detected within timeout range -> Ensure Buzzer is OFF */
            BUZ_Off(&Buzzer);
        }
        
        /* 60ms delay between triggers as recommended by HC-SR04 datasheet */
        TIM_vDelayMs(TEST_TIMER, MEASURE_DELAY_MS);
    }

    return 0;
}
