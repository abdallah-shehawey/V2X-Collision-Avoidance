/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Abdallah Saleh
 * @brief          : Main program body
 ******************************************************************************
 **/
#include <iostream>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "System/System.h"
#include "SEGGER_SYSVIEW.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"

#ifdef __cplusplus
}
#endif

extern BUZ_Config_t V2X_Buzzer;
extern US_Config_t FrontUS[3];
extern US_Config_t BackUS[3];

/* ================== Global SWV Variables ================== */
volatile uint16_t G_u16DistLeft = 0;
volatile uint16_t G_u16DistCenter = 0;
volatile uint16_t G_u16DistRight = 0;
volatile uint16_t G_u16DistBackLeft = 0;
volatile uint16_t G_u16DistBackCenter = 0;
volatile uint16_t G_u16DistBackRight = 0;
volatile uint8_t G_u8WarningState = 0; // 0: Safe, 1: Warning (Buzzer ON)

/* MPU9250 Globals */
MPU9250_Data_t G_stMPU9250_Data;
MPU9250_Position_t G_stMPU9250_Pos;
volatile float G_fSpeed = 0.0f;
volatile float G_fHeading = 0.0f;
volatile float G_fPitch = 0.0f;
volatile float G_fRoll = 0.0f;
volatile float G_fAltitudeZ = 0.0f;

/* ================== Task Prototypes ================== */
void vTask_Sensors(void *pvParameters);
void vTask_ADAS_Core(void *pvParameters);
void vTask_Feedback(void *pvParameters);

/* Communication Tasks */
void vTask_ESP_RX(void *pvParameters);
void vTask_ESP_TX(void *pvParameters);
void vTask_RPi_RX(void *pvParameters);
void vTask_RPi_TX(void *pvParameters);

int main(void)
{
	/* 1. Hardware Initialization */
	System_setup();

	SEGGER_setup();

	/* 2. OS Tasks Creation */
	/* --- Core and Hardware Tasks --- */
	xTaskCreate(vTask_Sensors,   "Sensors_Task", configMINIMAL_STACK_SIZE + 50, NULL, 3, NULL);
	xTaskCreate(vTask_ADAS_Core, "ADAS_Task",    configMINIMAL_STACK_SIZE + 50, NULL, 2, NULL);
	xTaskCreate(vTask_Feedback,  "Feedback_Task",configMINIMAL_STACK_SIZE,      NULL, 1, NULL);

	/* --- Communication Tasks --- */
	/* Highest Priority: Emergency handling from ESP-NOW */
	xTaskCreate(vTask_ESP_RX,    "ESP_RX_Task",  configMINIMAL_STACK_SIZE + 50, NULL, 4, NULL);
	/* Regular Reception */
	xTaskCreate(vTask_RPi_RX,    "RPi_RX_Task",  configMINIMAL_STACK_SIZE + 50, NULL, 2, NULL);
	/* Regular Transmissions */
	xTaskCreate(vTask_ESP_TX,    "ESP_TX_Task",  configMINIMAL_STACK_SIZE + 50, NULL, 1, NULL);
	xTaskCreate(vTask_RPi_TX,    "RPi_TX_Task",  configMINIMAL_STACK_SIZE + 50, NULL, 1, NULL);

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
	//  uint16_t tempDist = 0;
	for (;;)
	{
		//    /* Read US1 (Left) */
		//    if (US_u16ReadDistance_cm(&FrontUS[0], &tempDist) == OK) G_u16DistLeft = tempDist;
		//    vTaskDelay(pdMS_TO_TICKS(10)); // Slight delay between reading sensors
		//
		//    /* Read US2 (Center) */
		//    if (US_u16ReadDistance_cm(&FrontUS[1], &tempDist) == OK) G_u16DistCenter = tempDist;
		//    vTaskDelay(pdMS_TO_TICKS(10));
		//
		//    /* Read US3 (Right) */
		//    if (US_u16ReadDistance_cm(&FrontUS[2], &tempDist) == OK) G_u16DistRight = tempDist;
		//    vTaskDelay(pdMS_TO_TICKS(10));
		//
		//    /* Read US4 (Back Left) */
		//    if (US_u16ReadDistance_cm(&BackUS[0], &tempDist) == OK) G_u16DistBackLeft = tempDist;
		//    vTaskDelay(pdMS_TO_TICKS(10));
		//
		//    /* Read US5 (Back Center) */
		//    if (US_u16ReadDistance_cm(&BackUS[1], &tempDist) == OK) G_u16DistBackCenter = tempDist;
		//    vTaskDelay(pdMS_TO_TICKS(10));
		//
		//    /* Read US6 (Back Right) */
		//    if (US_u16ReadDistance_cm(&BackUS[2], &tempDist) == OK) G_u16DistBackRight = tempDist;
		//    vTaskDelay(pdMS_TO_TICKS(10));
		//
		//    /* Add MPU9250 Read here when calibrated */
		//    MPU9250_enumReadData(&G_stMPU9250_Data);
		//    MPU9250_enumGetAttitude(&G_stMPU9250_Data, (float*)&G_fPitch, (float*)&G_fRoll);
		//    MPU9250_enumGetHeading(&G_stMPU9250_Data, (float*)&G_fHeading);
		//    /* Dt is approx 0.05 seconds (50 ms) for speed processing */
		//    MPU9250_enumGetSpeed(&G_stMPU9250_Data, 0.05f, (float*)&G_fSpeed);
		//    MPU9250_enumGetPosition(&G_stMPU9250_Data, G_fSpeed, G_fHeading, G_fPitch, 0.05f, &G_stMPU9250_Pos);
		//    G_fAltitudeZ = G_stMPU9250_Pos.Z;
		//
		//    /* Overall Task Period ~ 50ms */
		//    vTaskDelay(pdMS_TO_TICKS(20));
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
		if (G_u16DistLeft <= 10 || G_u16DistCenter <= 10 || G_u16DistRight <= 10 ||
				G_u16DistBackLeft <= 10 || G_u16DistBackCenter <= 10 || G_u16DistBackRight <= 10)
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
		}
		else
		{
			BUZ_Off(&V2X_Buzzer);
		}

		/* Update UI frequently */
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}

/* ================== Communication Tasks (Phase 2 & 3) ================== */

/**
 * @brief Highest priority task - Waits on UART RX from ESP to process incoming Network alerts immediately.
 */
void vTask_ESP_RX(void *pvParameters)
{
	for (;;)
	{
		/* Block on an RX Semaphore / Queue in actual implementation */


		vTaskDelay(pdMS_TO_TICKS(50)); /* Placeholder */
	}
}

/**
 * @brief Periodically formats current state (Speed, Distance, Direction, etc.) and sends via ESP-NOW.
 */
void vTask_ESP_TX(void *pvParameters)
{
	for (;;)
	{
		/* Construct Frame and UART Transmit to ESP */


		vTaskDelay(pdMS_TO_TICKS(100)); /* Broadcast frequency (e.g. 10Hz) */
	}
}

/**
 * @brief Periodically formats system state bounds to be displayed on Raspberry Pi UI.
 */
void vTask_RPi_TX(void *pvParameters)
{
	for (;;)
	{
		/* Construct Display Frame and UART Transmit to RPi */


		vTaskDelay(pdMS_TO_TICKS(200)); /* Display update frequency (e.g. 5Hz) */
	}
}

/**
 * @brief Periodically checks or waits for commands/settings from the Raspberry Pi.
 */
void vTask_RPi_RX(void *pvParameters)
{
	for (;;)
	{
		/* Receive settings, mode changes, or ACKs from RPi */


		vTaskDelay(pdMS_TO_TICKS(100)); /* Check frequency */
	}
}
