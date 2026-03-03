#include "System/System.h"
#include "../Inc/Drivers/MCAL/RCC/RCC_interface.h"
#include "../Inc/Drivers/MCAL/GPIO/GPIO_interface.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"

#include "FreeRTOS.h"
#include "task.h"

/******************************************
 *  System Variables Required by FreeRTOS *
 ******************************************/
uint32_t SystemCoreClock = 16000000;

/******************************************
 *  Hardware Objects for Testing         *
 ******************************************/
LED_Config_t Test_LED = {GPIO_PORTA, GPIO_PIN5, ACTIVE_HIGH}; // Nucleo built-in LED (PA5)


/******************************************
 *  FreeRTOS Hooks & Callbacks           *
 ******************************************/
void vApplicationIdleHook(void)
{
    /* Runs when OS has no tasks to execute */
}


/******************************************
 *  Tasks Prototypes                     *
 ******************************************/
void vTask_Blinky(void *pvParameters);


/******************************************
 *  System Initialization                *
 ******************************************/
void System_setup(void)
{
  // 1. Initialize System Clock (HSI 16MHz)
  RCC_enumSetSysClk(RCC_HSI_CLK);
  
  // 2. Enable Peripheral Clocks
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOAEN, RCC_PER_ON);
  
  /**** Inti HAL ****/
  // init actuators
  LED_Init(&Test_LED);
  
  // inti Sensors
  
}


/******************************************
 *  RTOS Initialization & Task Creation  *
 ******************************************/
void RTOS_setup(void)
{
  /* 1. Create System Tasks */
  xTaskCreate(vTask_Blinky, "Blinky_Task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

  /* 2. Start the Scheduler */
  vTaskStartScheduler();
}


/******************************************
 *  Tasks Implementation                 *
 ******************************************/
void vTask_Blinky(void *pvParameters)
{
  for (;;)
  {
    /* Toggle LED to indicate RTOS is running and tick rate is correct */
    LED_Toggle(&Test_LED);
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

