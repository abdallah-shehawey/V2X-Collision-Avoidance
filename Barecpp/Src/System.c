
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
#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"

#include "FreeRTOS.h"
#include "task.h"

/******************************************
 *  System Variables Required by FreeRTOS *
 ******************************************/
uint32_t SystemCoreClock = 16000000;

/******************************************
 *  Hardware Objects for Testing         *
 ******************************************/
BUZ_Config_t V2X_Buzzer = {GPIO_PORTC, GPIO_PIN4, BUZ_ACTIVE_HIGH};

US_Config_t FrontUS[3]; // The 3 front ultrasonic sensors
US_Config_t BackUS[3];  // The 3 back ultrasonic sensors

/* Communication Interfaces */
USART_Config_t ESP_UART = {USART_CHANNEL1, 115200, USART_WORDLENGTH_8B, USART_STOPBITS_1, USART_PARITY_NONE, USART_MODE_TX_RX, UART_HWCONTROL_NONE, USART_OVERSAMPLING_16};
USART_Config_t RPi_UART = {USART_CHANNEL4, 115200, USART_WORDLENGTH_8B, USART_STOPBITS_1, USART_PARITY_NONE, USART_MODE_TX_RX, UART_HWCONTROL_NONE, USART_OVERSAMPLING_16};


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
  
  /* MPU9250 SPI Enable */
  RCC_enumABPPerSts(RCC_APB2, RCC_SPI1EN,  RCC_PER_ON);

  /* UARTs Enable */
  RCC_enumABPPerSts(RCC_APB2, RCC_USART1,  RCC_PER_ON); // ESP-NOW
  RCC_enumABPPerSts(RCC_APB1, RCC_USART4EN, RCC_PER_ON); // Raspberry Pi

  
  /**** Inti HAL ****/
  // init actuators
  BUZ_Init(&V2X_Buzzer);
  
  // inti Sensors
  
  // Setup SPI GPIO for MPU9250
  GPIO_PinConfig_t SPI_Pins = {
        .Port = GPIO_PORTA,
        .Mode = GPIO_ALTFN,
        .Otype = GPIO_PUSH_PULL,
        .Speed = GPIO_VERY_HIGH_SPEED,
        .PullType = GPIO_NO_PULL,
        .AlternateFunction = GPIO_AF5
  };
  SPI_Pins.PinNum = GPIO_PIN5; GPIO_enumPinInit(&SPI_Pins);
  SPI_Pins.PinNum = GPIO_PIN6; GPIO_enumPinInit(&SPI_Pins);
  SPI_Pins.PinNum = GPIO_PIN7; GPIO_enumPinInit(&SPI_Pins);
  
  MPU9250_enumInit();

  /* Setup GPIO for ESP-NOW (USART1): PA9 (TX), PA10 (RX) -> AF7 */
  GPIO_PinConfig_t U1_Pins = {
        .Port = GPIO_PORTA,
        .Mode = GPIO_ALTFN,
        .Otype = GPIO_PUSH_PULL,
        .Speed = GPIO_VERY_HIGH_SPEED,
        .PullType = GPIO_NO_PULL,
        .AlternateFunction = GPIO_AF7
  };
  U1_Pins.PinNum = GPIO_PIN9;  GPIO_enumPinInit(&U1_Pins);
  U1_Pins.PinNum = GPIO_PIN10; GPIO_enumPinInit(&U1_Pins);
  USART_Init(&ESP_UART);

  /* Setup GPIO for Raspberry Pi (UART4): PA0 (TX), PA1 (RX) -> AF8 */
  GPIO_PinConfig_t U4_Pins = {
        .Port = GPIO_PORTA,
        .Mode = GPIO_ALTFN,
        .Otype = GPIO_PUSH_PULL,
        .Speed = GPIO_VERY_HIGH_SPEED,
        .PullType = GPIO_NO_PULL,
        .AlternateFunction = GPIO_AF8
  };
  U4_Pins.PinNum = GPIO_PIN0; GPIO_enumPinInit(&U4_Pins);
  U4_Pins.PinNum = GPIO_PIN1; GPIO_enumPinInit(&U4_Pins);
  USART_Init(&RPi_UART);

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

  /* BACK SENSORS MAPPING:
   * S4: TIM3 CH2 -> PB5 Echo, PB12 Trig (Left)
   * S5: TIM3 CH3 -> PC8 Echo, PB13 Trig (Center)
   * S6: TIM3 CH4 -> PC9 Echo, PB14 Trig (Right) 
   */
  BackUS[0] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL2, GPIO_PORTB, GPIO_PIN12, GPIO_PORTB, GPIO_PIN5};
  US_vInit(&BackUS[0]);

  BackUS[1] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL3, GPIO_PORTB, GPIO_PIN13, GPIO_PORTC, GPIO_PIN8};
  US_vInit(&BackUS[1]);

  BackUS[2] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL4, GPIO_PORTB, GPIO_PIN14, GPIO_PORTC, GPIO_PIN9};
  US_vInit(&BackUS[2]);
}


/******************************************
 *  RTOS Initialization & Task Creation  *
 ******************************************/
void RTOS_setup(void)
{
  /* Start the RTOS Scheduler */
  vTaskStartScheduler();
}

