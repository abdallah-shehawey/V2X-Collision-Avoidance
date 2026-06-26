
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
#include "../Inc/Drivers/HAL/LED/LED_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Drivers/MCAL/NVIC/NVIC_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "FreeRTOS.h"
#include "SEGGER_SYSVIEW.h"
#include "task.h"

/******************************************
 *  System Variables Required by FreeRTOS *
 ******************************************/
uint32_t SystemCoreClock = 16000000;

/* Central Management Global Variables */

/* System-wide ADAS status word: 16-bit, 2 bits per module
 * (00 = safe, 01 = warning, 10 = critical). Full layout in System.h. */
volatile uint16_t  G_u16SystemFlags     = 0;
HostVehicleState_t G_stHostVehicleState = {0};

/* Distance to nearest intersection (cm), 0 = not near. Broadcast over DSRC for
 * the neighbors' IMA geometry.
 * NOTE: currently a stub — no source writes it yet, so it is always broadcast as
 * 0. The local IMA logic does NOT depend on it (it decides on relative speed),
 * so IMA still works. Wire a source here (map/RPi intersection geometry) before
 * any consumer relies on the broadcast value being non-zero. */
float Host_DistToIntersection = 0.0f;


/******************************************
 *  Hardware Objects for Testing         *
 ******************************************/
BUZ_Config_t V2X_Buzzer = {GPIO_PORTC, GPIO_PIN4, BUZ_ACTIVE_HIGH};
LED_Config_t FrontR_LED   = {GPIO_PORTC, GPIO_PIN0, ACTIVE_HIGH};
LED_Config_t FrontL_LED   = {GPIO_PORTC, GPIO_PIN1, ACTIVE_HIGH};
LED_Config_t BackR_LED    = {GPIO_PORTC, GPIO_PIN2, ACTIVE_HIGH};
LED_Config_t BackL_LED    = {GPIO_PORTC, GPIO_PIN3, ACTIVE_HIGH};
LED_Config_t Interior_LED = {GPIO_PORTC, GPIO_PIN7, ACTIVE_HIGH}; /* PC7 — driver dashboard */

US_Config_t FrontUS[3];
US_Config_t BackUS[3];

/* Communication Interfaces */
extern void vESP_UART_RX_Callback(void);

USART_Handle_t USART_1 = {
    .Channel = USART_CHANNEL1,
    .BaudRate = 115200,   /* was 9600 — 12x faster TX, cuts the ~20ms DSRC send to ~1.6ms.
                             ESP firmware MUST also be set to 115200 or V2X won't link. */
    .WordLength = USART_WORDLENGTH_8B,
    .StopBits = USART_STOPBITS_1,
    .Parity = USART_PARITY_NONE,
    .Mode = USART_MODE_TX_RX,
    .HardwareFlowControl = UART_HWCONTROL_NONE,
    .OverSampling = USART_OVERSAMPLING_16,
    .RXNEIE = USART_RXNEIE_EN,
    .pfnCallback = vESP_UART_RX_Callback
};

USART_Config_t RPi_UART = {USART_CHANNEL4, 115200, USART_WORDLENGTH_8B, USART_STOPBITS_1, USART_PARITY_NONE, USART_MODE_TX_RX, UART_HWCONTROL_NONE, USART_OVERSAMPLING_16};


/******************************************
 *  FreeRTOS Hooks & Callbacks           *
 ******************************************/
void vApplicationIdleHook(void)
{
	/* Runs when OS has no tasks to execute */
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	(void)xTask;
	(void)pcTaskName;
	/* Stack overflow detected — halt for debugging */
	taskDISABLE_INTERRUPTS();
	for(;;);
}

void vApplicationMallocFailedHook(void)
{
	/* Heap exhausted — halt for debugging */
	taskDISABLE_INTERRUPTS();
	for(;;);
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

	//3.Enable Timer6 for delay and Timer5 for Background Timestamping
	RCC_enumABPPerSts(RCC_APB1, RCC_TIM6EN,  RCC_PER_ON);
	RCC_enumABPPerSts(RCC_APB1, RCC_TIM5EN,  RCC_PER_ON);

	/* Start TIM5 as a continuous 32-bit background counter (1ms tick) */
	TIM_Config_t TIM5_Config = {
		.Timer = TIM_TIMER5,
		.Prescaler = 16000 - 1,
		.AutoReloadValue = 0xFFFFFFFF,
		.Mode = TIM_COUNTERMODE_UP
	};
	TIM_vInit(&TIM5_Config);
	TIM_vStart(TIM_TIMER5);

		/*                           *
		 * MPU9250 SPI configuration *
		 *                           */
	RCC_enumABPPerSts(RCC_APB2, RCC_SPI1EN,  RCC_PER_ON);
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

	    /*                           *
		 * Ultrasonics configuration *
		 *                           */

	RCC_enumABPPerSts(RCC_APB1, RCC_TIM2EN,  RCC_PER_ON);
	RCC_enumABPPerSts(RCC_APB1, RCC_TIM3EN,  RCC_PER_ON);
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

	/*                                          *
	 * ESP-NOW / DSRC (V2X) on USART1 (PA9/PA10) *
	 *   Interrupt-driven RX (vESP_UART_RX_Callback) + polled TX.  *
	 *                                          */
	RCC_enumABPPerSts(RCC_APB2, RCC_USART1, RCC_PER_ON);
	GPIO_PinConfig_t U1_Pins = {
			.Port = GPIO_PORTA,
			.Mode = GPIO_ALTFN,
			.Otype = GPIO_PUSH_PULL,
			.Speed = GPIO_VERY_HIGH_SPEED,
			.PullType = GPIO_NO_PULL,
			.AlternateFunction = GPIO_AF7
	};
	U1_Pins.PinNum = GPIO_PIN9;  GPIO_enumPinInit(&U1_Pins);  /* USART1 TX (PA9)  */
	U1_Pins.PinNum = GPIO_PIN10; GPIO_enumPinInit(&U1_Pins);  /* USART1 RX (PA10) */

	USART_InitIT(&USART_1);                  /* USART_1.Channel == CHANNEL1 */
	NVIC_vSetPriority(NVIC_USART1, 6); /* Safe for FreeRTOS (configMAX_SYSCALL_INTERRUPT_PRIORITY is 5) */
	NVIC_vEnableIRQ(NVIC_USART1);

	/*                                          *
	 * Raspberry Pi telemetry on UART4 (PA0/PA1) *
	 *   Polled TX only (ASCII telemetry line every 100ms).       *
	 *                                          */
	RCC_enumABPPerSts(RCC_APB1, RCC_USART4EN, RCC_PER_ON);
	GPIO_PinConfig_t U4_Pins = {
			.Port = GPIO_PORTA,
			.Mode = GPIO_ALTFN,
			.Otype = GPIO_PUSH_PULL,
			.Speed = GPIO_VERY_HIGH_SPEED,
			.PullType = GPIO_NO_PULL,
			.AlternateFunction = GPIO_AF8
	};
	U4_Pins.PinNum = GPIO_PIN0; GPIO_enumPinInit(&U4_Pins);  /* UART4 TX (PA0) */
	U4_Pins.PinNum = GPIO_PIN1; GPIO_enumPinInit(&U4_Pins);  /* UART4 RX (PA1) */
	USART_Init(&RPi_UART);                    /* RPi_UART.Channel == CHANNEL4 */


	/*                *
	 * init actuato   *
	 *                */
	/* Initialize LEDs */
	LED_Init(&FrontR_LED);
	LED_Init(&FrontL_LED);
	LED_Init(&BackR_LED);
	LED_Init(&BackL_LED);
	LED_Init(&Interior_LED);
	/* Initialize Buzzer */
    BUZ_Init(&V2X_Buzzer);

	/* ── Magnetometer hard-iron calibration (boot-time rotation) ──
	 * One short beep + interior LED ON = "rotate the car ~360° NOW".
	 * Keep turning until the LED goes off (~12s). Then:
	 *   two short beeps = calibration OK
	 *   one long  beep  = not enough rotation → power-cycle and retry.
	 * NOTE: runs BEFORE the IWDG is started (in main), so the long window
	 *       cannot trip the watchdog. */
	BUZ_On(&V2X_Buzzer);  TIM_vDelayMs(TIM_TIMER6, 400);  BUZ_Off(&V2X_Buzzer);
	LED_TurnOn(&Interior_LED);

	if (MPU9250_enumCalibrateMag() == OK)
	{
		LED_TurnOff(&Interior_LED);
		BUZ_On(&V2X_Buzzer); TIM_vDelayMs(TIM_TIMER6, 150); BUZ_Off(&V2X_Buzzer);
		TIM_vDelayMs(TIM_TIMER6, 120);
		BUZ_On(&V2X_Buzzer); TIM_vDelayMs(TIM_TIMER6, 150); BUZ_Off(&V2X_Buzzer);
	}
	else
	{
		LED_TurnOff(&Interior_LED);
		BUZ_On(&V2X_Buzzer); TIM_vDelayMs(TIM_TIMER6, 1000); BUZ_Off(&V2X_Buzzer);
	}

	// DSRC init
	DSRC_Init();
}


/******************************************
 *  RTOS Initialization & Task Creation  *
 ******************************************/
void SEGGER_setup(void)
{
	/*Enable cycle counter feature of the processor*/
	DWT_CTRL |= 1;
	SEGGER_SYSVIEW_Conf();
	SEGGER_SYSVIEW_Start();
}
void RTOS_setup(void)
{
	/* Start the RTOS Scheduler */
	vTaskStartScheduler();
}



