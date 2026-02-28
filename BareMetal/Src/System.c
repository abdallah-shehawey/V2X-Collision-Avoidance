#include "System/System.h"
#include "../Inc/Drivers/MCAL/RCC/RCC.h"
#include "../Inc/Drivers/MCAL/GPIO/GPIO.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"
#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"


void System_setup(void)
{
  // Init RCC
  /* 1. Initialize System Clock (HSI 16MHz) */
  RCC_enumSetSysClk(RCC_HSI_CLK);
  /* 2. Enable Peripheral Clocks */
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOAEN, RCC_PER_ON);
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOBEN, RCC_PER_ON);
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOCEN, RCC_PER_ON);
  /*TIM for Ultrasonic*/
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM2EN,  RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM3EN,  RCC_PER_ON);
  /*TIM for delay*/
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM6EN,  RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM7EN,  RCC_PER_ON);
  /*For MPU9250*/
  RCC_enumABPPerSts(RCC_APB2, RCC_SPI1EN,  RCC_PER_ON);
  /* For Buzzer (Assuming Port C) */
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOCEN, RCC_PER_ON);
  


  // init GPIO

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

  /* Feedback System Initialization (Simple Style) */
  
  /* Configure LED 1 */
  LED_Config_t LED1 = {GPIO_PORTC, GPIO_PIN0, ACTIVE_HIGH};
  LED_Init(&LED1);

  /* Configure LED 2 */
  LED_Config_t LED2 = {GPIO_PORTC, GPIO_PIN1, ACTIVE_HIGH};
  LED_Init(&LED2);

  /* Configure LED 3 */
  LED_Config_t LED3 = {GPIO_PORTC, GPIO_PIN2, ACTIVE_HIGH};
  LED_Init(&LED3);

  /* Configure LED 4 */
  LED_Config_t LED4 = {GPIO_PORTC, GPIO_PIN3, ACTIVE_HIGH};
  LED_Init(&LED4);

  /* Configure Buzzer */
  BUZ_Config_t MyBuzzer = {GPIO_PORTC, GPIO_PIN4, ACTIVE_HIGH};
  BUZ_Init(&MyBuzzer);



  // inti Sensors
  MPU9250_enumInit();
  //inti Ultrasonic
  US_Config_t US[6];

	/* FRONT SENSORS (UART Friendly) */
	/* S1: TIM2 CH1 -> PA15 Echo, PB0 Trig */
	US[0] = (US_Config_t){TIM_TIMER2, TIM_CHANNEL1, GPIO_PORTB, GPIO_PIN0, GPIO_PORTA, GPIO_PIN15};
	US_vInit(&US[0]);

	/* S2: TIM2 CH2 -> PB3 Echo, PB1 Trig */
	US[1] = (US_Config_t){TIM_TIMER2, TIM_CHANNEL2, GPIO_PORTB, GPIO_PIN1, GPIO_PORTB, GPIO_PIN3};
	US_vInit(&US[1]);

	/* S3: TIM3 CH1 -> PB4 Echo, PB2 Trig */
	US[2] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL1, GPIO_PORTB, GPIO_PIN2, GPIO_PORTB, GPIO_PIN4};
	US_vInit(&US[2]);

	/* BACK SENSORS (UART Friendly) */
	/* S4: TIM3 CH2 -> PB5 Echo, PB12 Trig */
	US[3] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL2, GPIO_PORTB, GPIO_PIN12, GPIO_PORTB, GPIO_PIN5};
	US_vInit(&US[3]);

	/* S5: TIM3 CH3 -> PC8 Echo, PB13 Trig */
	US[4] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL3, GPIO_PORTB, GPIO_PIN13, GPIO_PORTC, GPIO_PIN8};
	US_vInit(&US[4]);

	/* S6: TIM3 CH4 -> PC9 Echo, PB14 Trig */
	US[5] = (US_Config_t){TIM_TIMER3, TIM_CHANNEL4, GPIO_PORTB, GPIO_PIN14, GPIO_PORTC, GPIO_PIN9};
	US_vInit(&US[5]);
}

void RTOS_setup(void)
{

}
