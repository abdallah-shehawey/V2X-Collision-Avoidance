
/**
 ******************************************************************************
 * @file           : System.c
 * @author         : Abdallah Saleh
 * @brief          : System initialization and RTOS setup
 ******************************************************************************
 **/

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
  /* Start the RTOS Scheduler */
  vTaskStartScheduler();
}

