
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
#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"

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
BUZ_Config_t V2X_Buzzer = {GPIO_PORTC, GPIO_PIN4, BUZ_ACTIVE_HIGH};

US_Config_t FrontUS[3]; // The 3 front ultrasonic sensors


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
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOBEN, RCC_PER_ON);
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOCEN, RCC_PER_ON);
  
  /* Timers for Ultrasonics */
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM2EN,  RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM3EN,  RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM6EN,  RCC_PER_ON);

  
  /**** Inti HAL ****/
  // init actuators
  LED_Init(&Test_LED);
  BUZ_Init(&V2X_Buzzer);
  
  // inti Sensors
  
  /* FRONT SENSORS MAPPING:
   * S1: TIM2 CH1 -> PA15 Echo, PB0 Trig (Left)
   * S2: TIM2 CH2 -> PB3 Echo, PB1 Trig  (Center)
   * S3: TIM3 CH1 -> PB4 Echo, PB2 Trig  (Right) 
   */
  FrontUS[0] = (US_Config_t){TIM_TIMER2, TIM_CHANNEL1, GPIO_PORTB, GPIO_PIN0, GPIO_PORTA, GPIO_PIN15};
  US_vInit(&FrontUS[0]);

  FrontUS[1] = (US_Config_t){TIM_TIMER2, TIM_CHANNEL2, GPIO_PORTB, GPIO_PIN1, GPIO_PORTB, GPIO_PIN3};
  US_vInit(&FrontUS[1]);

  FrontUS[2] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL1, GPIO_PORTB, GPIO_PIN2, GPIO_PORTB, GPIO_PIN4};
  US_vInit(&FrontUS[2]);
}


/******************************************
 *  RTOS Initialization & Task Creation  *
 ******************************************/
void RTOS_setup(void)
{
  /* Start the RTOS Scheduler */
  vTaskStartScheduler();
}

