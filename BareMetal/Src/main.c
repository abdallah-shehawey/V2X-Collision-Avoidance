/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Abdallah Saleh
 * @brief          : Main program body
 ******************************************************************************
 **/

#include <stdint.h>
#include "System/System.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"

extern LED_Config_t Test_LED;
extern BUZ_Config_t V2X_Buzzer;
extern US_Config_t FrontUS[3];

/* ================== Global SWV Variables ================== */
volatile uint16_t G_u16DistLeft = 0;
volatile uint16_t G_u16DistCenter = 0;
volatile uint16_t G_u16DistRight = 0;
volatile uint8_t G_u8WarningState = 0; // 0: Safe, 1: Warning (Buzzer ON)

/* ================== Task Prototypes ================== */
void vTask_Sensors(void *pvParameters);
void vTask_ADAS_Core(void *pvParameters);
void vTask_Feedback(void *pvParameters);

int main(void)
{
  /* 1. Hardware Initialization */
  System_setup();

  /* 2. OS Tasks Creation */
  xTaskCreate(vTask_Sensors,   "Sensors_Task", configMINIMAL_STACK_SIZE + 50, NULL, 3, NULL);
  xTaskCreate(vTask_ADAS_Core, "ADAS_Task",    configMINIMAL_STACK_SIZE + 50, NULL, 2, NULL);
  xTaskCreate(vTask_Feedback,  "Feedback_Task",configMINIMAL_STACK_SIZE,      NULL, 1, NULL);

  /* 3. OS Initialization and start running tasks */
  RTOS_setup();

  /* 4. Should never be reached unless scheduler fails */
  for (;;);

}

/* ================== Task Implementations ================== */

/**
 * @brief Reads data from Ultrasonics and MPU9250 periodically.
 */
void vTask_Sensors(void *pvParameters)
{
  uint16_t tempDist = 0;
  for (;;)
  {
    /* Read US1 (Left) */
    if (US_u16ReadDistance_cm(&FrontUS[0], &tempDist) == OK) G_u16DistLeft = tempDist;
    vTaskDelay(pdMS_TO_TICKS(10)); // Slight delay between reading sensors
    
    /* Read US2 (Center) */
    if (US_u16ReadDistance_cm(&FrontUS[1], &tempDist) == OK) G_u16DistCenter = tempDist;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Read US3 (Right) */
    if (US_u16ReadDistance_cm(&FrontUS[2], &tempDist) == OK) G_u16DistRight = tempDist;
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Add MPU9250 Read here when calibrated */
    /* MPU9250_voidRead(); */


    /* Overall Task Period ~ 50ms */
    vTaskDelay(pdMS_TO_TICKS(20)); 
  }
}

/**
 * @brief Evaluates risk based on sensor data.
 */
void vTask_ADAS_Core(void *pvParameters)
{
  for (;;)
  {
    /* Very basic logic for testing Phase 1 */
    if (G_u16DistLeft <= 10 || G_u16DistCenter <= 10 || G_u16DistRight <= 10)
    {
      G_u8WarningState = 1; /* Danger! */
    }
    else
    {
      G_u8WarningState = 0; /* Safe */
    }
    
    vTaskDelay(pdMS_TO_TICKS(50)); /* Evaluates every 50ms */
  }
}

/**
 * @brief Handles user feedback (Buzzer & LEDs).
 */
void vTask_Feedback(void *pvParameters)
{
  for (;;)
  {
    if (G_u8WarningState == 1)
    {
      BUZ_On(&V2X_Buzzer);
      LED_TurnOn(&Test_LED);
    }
    else
    {
      BUZ_Off(&V2X_Buzzer);
      LED_TurnOff(&Test_LED);
    }
    
    /* Update UI frequently */
    vTaskDelay(pdMS_TO_TICKS(20)); 
  }
}
