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

extern LED_Config_t Test_LED;

/* ================== Task Prototypes ================== */
void vTask_Blinky(void *pvParameters);

int main(void)
{
  /* 1. Hardware Initialization */
  System_setup();

  /* 2. OS Tasks Creation */
  xTaskCreate(vTask_Blinky, "Blinky_Task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

  /* 3. OS Initialization and start running tasks */
  RTOS_setup();

  /* 4. Should never be reached unless scheduler fails */
  for (;;);
}

/* ================== Task Implementations ================== */
void vTask_Blinky(void *pvParameters)
{
  for (;;)
  {
    /* Toggle LED to indicate RTOS is running and tick rate is correct */
    LED_Toggle(&Test_LED);
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}
