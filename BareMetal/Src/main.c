/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Abdallah Saleh
 * @brief          : Main program body
 ******************************************************************************
 **/

#include <stdint.h>
#include <stdio.h>
#include "System/System.h"

#include "FreeRTOS.h"
#include "SEGGER_SYSVIEW.h"
#include "task.h"
#include "queue.h"

#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"
#include "../Inc/Drivers/HAL/L298N/L298N_interface.h"
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"

extern BUZ_Config_t V2X_Buzzer;
extern US_Config_t FrontUS[3];
extern US_Config_t BackUS[3];
extern L298N_MotorConfig_t RightMotor;
extern L298N_MotorConfig_t LeftMotor;
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED;

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


QueueHandle_t G_xESP_RX_Queue;


/* ================== Task Prototypes ================== */
void vTask_Sensors(void *pvParameters);
/* ADAS Subsystem Tasks */
void vTask_BSW (void *pvParameters);
void vTask_DNPW(void *pvParameters);
void vTask_EEBL(void *pvParameters);
void vTask_FCW (void *pvParameters);
void vTask_IMA (void *pvParameters);
void vTask_SDW (void *pvParameters);
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
  xTaskCreate(vTask_Sensors,   "Sensors_Task", configMINIMAL_STACK_SIZE + 256, NULL, 3, NULL);

  /* ADAS Independent Subsystems */
  xTaskCreate(vTask_EEBL,    "EEBL_Task",    configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL); /* High Priority: Emergency braking */
  xTaskCreate(vTask_FCW,     "FCW_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL); /* High Priority: Forward collision */
  xTaskCreate(vTask_BSW,     "BSW_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 3, NULL); /* Medium: Blind spot */
  xTaskCreate(vTask_DNPW,    "DNPW_Task",    configMINIMAL_STACK_SIZE + 128, NULL, 3, NULL); /* Medium: Passing Warning */
  xTaskCreate(vTask_SDW,     "SDW_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL); /* Low/Medium: Safe distance processing */
  xTaskCreate(vTask_IMA,     "IMA_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL); /* Low/Medium: Intersection */
  xTaskCreate(vTask_Feedback,"Feedback_Task",configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL); /* Low Priority: Hardware UI/Motors execution */

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
    
    /* Dt is approx 0.08 seconds (80 ms cycle) for speed processing */
    MPU9250_enumGetSpeed(&G_stMPU9250_Data, 0.08f, (float*)&G_fSpeed);
    MPU9250_enumGetPosition(&G_stMPU9250_Data, G_fSpeed, G_fHeading, G_fPitch, 0.08f, &G_stMPU9250_Pos);
    G_fAltitudeZ = G_stMPU9250_Pos.Z;


    /* Overall Task Period ~ 50ms */
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/**
 * @brief ADAS: Blind Spot Warning Task
 */
void vTask_BSW(void *pvParameters)
{
  for (;;)
  {
    /* BSW Update Logic Here */
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief ADAS: Do Not Pass Warning Task
 */
void vTask_DNPW(void *pvParameters)
{
  for (;;)
  {
    /* DNPW Update Logic Here */
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief ADAS: Emergency Electronic Brake Light Task
 */
void vTask_EEBL(void *pvParameters)
{
  for (;;)
  {
    /* EEBL Update Logic Here */
    vTaskDelay(pdMS_TO_TICKS(25)); /* Runs faster for quick emergency detection */
  }
}

/**
 * @brief ADAS: Forward Collision Warning Task
 */
void vTask_FCW(void *pvParameters)
{
  for (;;)
  {
    FCW_voidUpdate(); 
    vTaskDelay(pdMS_TO_TICKS(25)); /* Runs faster for immediate collision threat */
  }
}

/**
 * @brief ADAS: Intersection Movement Assist Task
 */
void vTask_IMA(void *pvParameters)
{
  for (;;)
  {
    /* IMA Update Logic Here */
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief ADAS: Safe Distance Warning Task
 */
void vTask_SDW(void *pvParameters)
{
  for (;;)
  {
    /* SDW Update Logic Here */
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief Centralized Hardware Manager (Muscle).
 * Executes commands decided by the ADAS logic (Brain).
 */


void vTask_Feedback(void *pvParameters)
{
  for (;;)
  {
    /* 1. Actuate Motors based on Central Intention */
    switch (G_eMotorGlobalCommand)
    {
      case CMD_MOVE_FORWARD:
        L298N_enumCarMoveForward(&RightMotor, &LeftMotor);
        LED_TurnOff(&BackR_LED);
        LED_TurnOff(&BackL_LED);
        break;
      case CMD_STOP:
        L298N_enumCarStop(&RightMotor, &LeftMotor);
        LED_TurnOn(&BackR_LED);
        LED_TurnOn(&BackL_LED);
        break;
      case CMD_STEER_RIGHT:
        L298N_enumCarMoveRight(&RightMotor, &LeftMotor);
        LED_TurnOff(&BackR_LED);
        LED_TurnOff(&BackL_LED);
        break;
      case CMD_STEER_LEFT:
        L298N_enumCarMoveLeft(&RightMotor, &LeftMotor);
        LED_TurnOff(&BackR_LED);
        LED_TurnOff(&BackL_LED);
        break;
    }

    /* 2. Actuate Feedback UI (Buzzer & LEDs) based on Risk Level */
    if (G_u8SystemRiskLevel == 0) /* Safe */
    {
      BUZ_Off(&V2X_Buzzer);
      LED_TurnOff(&FrontR_LED);
      LED_TurnOff(&FrontL_LED);
    }
    else if (G_u8SystemRiskLevel == 1) /* Warning */
    {
      BUZ_On(&V2X_Buzzer);
      LED_TurnOn(&FrontR_LED);
      LED_TurnOn(&FrontL_LED);
    }
    else if (G_u8SystemRiskLevel == 2) /* Critical */
    {
      BUZ_On(&V2X_Buzzer);
      LED_TurnOn(&FrontR_LED);
      LED_TurnOn(&FrontL_LED);
    }

    /* Keep execution consistent */
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}


/**
 * @brief Handles both RX and TX communication with ESP (V2X Network) using DSRC foundation.
 * TX: Periodically broadcasts system state via DSRC_SendNeighbor.
 * RX: Feeds incoming bytes from ISR Queue to DSRC_RxCallback, then updates Neighbor Table.
 */
void vTask_ESP_Comm(void *pvParameters)
{
  uint8_t txCounter = 0;
  uint8_t rxByte;

  for (;;)
  {
    /* 1. RX Processing via DSRC */
    /* Empty the freeRTOS queue and pass all new bytes to DSRC parser */
    while (xQueueReceive(G_xESP_RX_Queue, &rxByte, 0) == pdTRUE)
    {
      DSRC_RxCallback(rxByte);
    }
    
    /* Read exact timestamp from TIM5 background counter */
    uint32_t current_time_ms = 0;
    TIM_u32GetCounterValue(TIM_TIMER5, &current_time_ms);

    /* Update DSRC Neighbor Table with freshly parsed packets and our local timestamp */
    DSRC_Update(current_time_ms);

    /* 2. TX Processing via DSRC (Send every 100ms -> every 2nd loop of 50ms) */
    txCounter++;
    if (txCounter >= 2)
    {
      txCounter = 0;
      
      /* Build our Host Vehicle data using the DSRC Neighbor struct */
      Neighbor myState = {
          .vehicle_id = VEHICLE_ID, /* From DSRC.h */
          .pos_x = G_stMPU9250_Pos.X,
          .pos_y = G_stMPU9250_Pos.Y,
          .pos_z = G_stMPU9250_Pos.Z,
          .speed = G_fSpeed,
          .heading = G_fHeading,
          .last_update = 0 /* Not used over network, strictly local reception timestamp! */
      };
      
      /* Hand over to DSRC for Checksum calculation and UART transmission */
      DSRC_SendNeighbor(&myState);
    }

    /* Remove stale vehicles that haven't sent data in > 5000ms */
    DSRC_RemoveStale(current_time_ms);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}


void vTask_RPi_Comm(void *pvParameters)
{
 

  for (;;)
  {
   

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


/* ================== ISR callbacks ================== */
void vESP_UART_RX_Callback(void)
{
    /* Read exact byte directly from hardware Data Register (DR).
     * This avoids any software busy-flag deadlocks when TX is transmitting simultaneously!
     */
    uint8_t rxData = USART_ReceiveByteDirect(USART_CHANNEL1);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(G_xESP_RX_Queue, &rxData, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
