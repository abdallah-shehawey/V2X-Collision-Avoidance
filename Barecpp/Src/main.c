/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Abdallah Saleh
 * @brief          : Main program body
 ******************************************************************************
 **/

#include <stdint.h>
#include "System/System.h"

#include "FreeRTOS.h"
#include "SEGGER_SYSVIEW.h"
#include "task.h"
#include "queue.h"

#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"

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

/* ================== V2X Data Frame ================== */
typedef struct __attribute__((packed)) {
    uint8_t  Sender_ID;
    uint8_t  Target_ID;
    float    Speed_ms;
    float    Heading_deg;
    float    Position_Z;
    uint8_t  Vehicle_State; /* 0: Normal, 1: EEBL Active */
} V2X_Message_t;

QueueHandle_t G_xESP_RX_Queue;
V2X_Message_t G_stIncomingV2XMsg;

/* ================== Task Prototypes ================== */
void vTask_Sensors(void *pvParameters);
void vTask_ADAS_Core(void *pvParameters);
void vTask_Feedback(void *pvParameters);

/* Communication Tasks */
void vTask_ESP_Comm(void *pvParameters);
void vTask_RPi_Comm(void *pvParameters);

int main(void)
{
  /* 1. Hardware Initialization */
  System_setup();
  SEGGER_setup();

  /* Create Queues */
  G_xESP_RX_Queue = xQueueCreate(64, sizeof(uint8_t));

  /* 2. OS Tasks Creation */
  /* --- Core and Hardware Tasks --- */
  xTaskCreate(vTask_Sensors,   "Sensors_Task", configMINIMAL_STACK_SIZE + 50, NULL, 3, NULL);
  xTaskCreate(vTask_ADAS_Core, "ADAS_Task",    configMINIMAL_STACK_SIZE + 50, NULL, 2, NULL);
  xTaskCreate(vTask_Feedback,  "Feedback_Task",configMINIMAL_STACK_SIZE,      NULL, 1, NULL);

  /* --- Communication Tasks --- */
  /* ESP Communication: High Priority for V2X Emergency handling */
  xTaskCreate(vTask_ESP_Comm,  "ESP_Comm_Task",  configMINIMAL_STACK_SIZE + 100, NULL, 4, NULL);
  
  /* Raspberry Pi Communication: UI and Settings */
  xTaskCreate(vTask_RPi_Comm,  "RPi_Comm_Task",  configMINIMAL_STACK_SIZE + 100, NULL, 2, NULL);

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

    /* Read US4 (Back Left) */
    if (US_u16ReadDistance_cm(&BackUS[0], &tempDist) == OK) G_u16DistBackLeft = tempDist;
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Read US5 (Back Center) */
    if (US_u16ReadDistance_cm(&BackUS[1], &tempDist) == OK) G_u16DistBackCenter = tempDist;
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Read US6 (Back Right) */
    if (US_u16ReadDistance_cm(&BackUS[2], &tempDist) == OK) G_u16DistBackRight = tempDist;
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Add MPU9250 Read here when calibrated */
    MPU9250_enumReadData(&G_stMPU9250_Data);
    MPU9250_enumGetAttitude(&G_stMPU9250_Data, (float*)&G_fPitch, (float*)&G_fRoll);
    MPU9250_enumGetHeading(&G_stMPU9250_Data, (float*)&G_fHeading);
    
    /* Dt is approx 0.05 seconds (50 ms) for speed processing */
    MPU9250_enumGetSpeed(&G_stMPU9250_Data, 0.05f, (float*)&G_fSpeed);
    MPU9250_enumGetPosition(&G_stMPU9250_Data, G_fSpeed, G_fHeading, G_fPitch, 0.05f, &G_stMPU9250_Pos);
    G_fAltitudeZ = G_stMPU9250_Pos.Z;

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
    

    /* Update UI frequently */
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}



/**
 * @brief Handles both RX and TX communication with ESP (V2X Network).
 * TX: Periodically broadcasts system state.
 * RX: Non-blocking check for incoming alerts via Queue from ISR.
 */
void vTask_ESP_Comm(void *pvParameters)
{
  uint8_t txCounter = 0;
  uint8_t rxByte;
  uint8_t rxBuffer[sizeof(V2X_Message_t)];
  uint8_t rxIndex = 0;

  for (;;)
  {
    /* 1. RX Processing */
    while (xQueueReceive(G_xESP_RX_Queue, &rxByte, 0) == pdTRUE)
    {
      rxBuffer[rxIndex++] = rxByte;

      /* When a full dataframe is received */
      if (rxIndex >= sizeof(V2X_Message_t))
      {
        V2X_Message_t *pMsg = (V2X_Message_t*)rxBuffer;
        
        /* 0xFF is Broadcast, 0x01 is our vehicle ID */
        if(pMsg->Target_ID == 0xFF || pMsg->Target_ID == 0x01)
        {
          G_stIncomingV2XMsg = *pMsg;
          
          /* EEBL Trigger */
          if(G_stIncomingV2XMsg.Vehicle_State == 1) 
          {
             G_u8WarningState = 1; 
          }
        }
        rxIndex = 0; /* Reset for next message */
      }
    }

    /* 2. TX Processing (e.g., Send every 100ms -> every 2nd loop) */
    txCounter++;
    if (txCounter >= 2)
    {
      txCounter = 0;
      
      V2X_Message_t txMsg = {
          .Sender_ID = 0x01,
          .Target_ID = 0xFF,
          .Speed_ms = G_fSpeed,
          .Heading_deg = G_fHeading,
          .Position_Z = G_fAltitudeZ,
          .Vehicle_State = G_u8WarningState 
      };
      
      /* UART Transmit */
      uint8_t *pData = (uint8_t*)&txMsg;
      USART_Config_t tempConfig = {USART_CHANNEL1};
      for (uint8_t i = 0; i < sizeof(V2X_Message_t); i++)
      {
        USART_enumTransmit(&tempConfig, pData[i]);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/* ================== ISR callbacks ================== */
void vESP_UART_RX_Callback(void)
{
    uint8_t rxData;
    USART_Config_t tempConfig = {USART_CHANNEL1};
    
    if (USART_enumReceive(&tempConfig, &rxData) == OK)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(G_xESP_RX_Queue, &rxData, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief Handles both RX and TX communication with Raspberry Pi (UI Interface).
 * TX: Periodically sends system state bounds to display.
 * RX: Checks for settings or mode changes from the Pi.
 */
void vTask_RPi_Comm(void *pvParameters)
{
  uint8_t txCounter = 0;

  for (;;)
  {
    /* 1. RX Processing (Non-Blocking) */
    /* Receive settings, mode changes, or ACKs from RPi */

    /* 2. TX Processing (e.g., Send every 200ms -> every 2nd loop) */
    txCounter++;
    if (txCounter >= 2)
    {
      /* Construct Display Frame and UART Transmit to RPi */
      txCounter = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); /* Evaluates every 100ms */
  }
}

