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
#include "semphr.h"


#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"
#include "../Inc/Drivers/HAL/L298N/L298N_interface.h"
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/DNPW/DNPW_interface.h"
#include "../Inc/Application/IMA/IMA_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

extern BUZ_Config_t V2X_Buzzer;
extern US_Config_t FrontUS[3];
extern US_Config_t BackUS[3];
extern L298N_MotorConfig_t RightMotor;
extern L298N_MotorConfig_t LeftMotor;
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED;


QueueHandle_t G_xESP_RX_Queue;
SemaphoreHandle_t G_xDataMutex;
SemaphoreHandle_t G_xNeighborTableMutex;



/* ================== Task Prototypes ================== */
void vTask_EEBL(void *pvParameters);
void vTask_FCW(void *pvParameters);
void vTask_BSW(void *pvParameters);
void vTask_DNPW(void *pvParameters);
void vTask_IMA(void *pvParameters);
void vTask_Sensors(void *pvParameters);
void vTask_Feedback(void *pvParameters);

/* Communication Tasks */
void vTask_ESP_Comm(void *pvParameters);
void vTask_RPi_Comm(void *pvParameters);


int main(void)
{
  /* 1. Hardware Initialization */
  System_setup();
  SEGGER_setup();

  /* Create Queues & Mutexes */
  G_xESP_RX_Queue = xQueueCreate(256, sizeof(uint8_t));
  G_xDataMutex = xSemaphoreCreateMutex();
  G_xNeighborTableMutex = xSemaphoreCreateMutex();

  /* 2. OS Tasks Creation - Pipeline Architecture */
  xTaskCreate(vTask_EEBL, "EEBL_Task", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  xTaskCreate(vTask_FCW, "FCW_Task", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  xTaskCreate(vTask_BSW, "BSW_Task", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  xTaskCreate(vTask_DNPW, "DNPW_Task", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  xTaskCreate(vTask_IMA, "IMA_Task", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  /* --- Medium Priority: Data Acquisition (priority 3 — US blocking preempted by priority 4) --- */
  xTaskCreate(vTask_Sensors,      "Sensors_Task",  configMINIMAL_STACK_SIZE + 256, NULL, 3, NULL);
  xTaskCreate(vTask_ESP_Comm,     "ESP_Comm_Task", configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  /* --- Low Priority: Actuator Execution & UI/RPi --- */
  xTaskCreate(vTask_Feedback,     "Feedback_Task", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
  xTaskCreate(vTask_RPi_Comm,     "RPi_Comm_Task", configMINIMAL_STACK_SIZE + 100, NULL, 1, NULL);

  /* 3. OS Initialization and start running tasks */
  RTOS_setup();

  /* 4. Should never be reached unless scheduler fails */
  for (;;);

}

/* ================== Task Implementations ================== */

/* --- ADAS task stubs: empty for now, real logic added in the ADAS/SafetyEngine step --- */
void vTask_EEBL(void *pvParameters)
{
  for(;;) { vTaskDelay(pdMS_TO_TICKS(25)); }
}
void vTask_FCW(void *pvParameters)
{
  for(;;) { vTaskDelay(pdMS_TO_TICKS(25)); }
}
void vTask_BSW(void *pvParameters)
{
  for(;;) { vTaskDelay(pdMS_TO_TICKS(50)); }
}
void vTask_DNPW(void *pvParameters)
{
  for(;;) { vTaskDelay(pdMS_TO_TICKS(50)); }
}
void vTask_IMA(void *pvParameters)
{
  for(;;) { vTaskDelay(pdMS_TO_TICKS(50)); }
}

/**
 * @brief Reads one US sensor per cycle (round-robin) and MPU9250 every cycle.
 *
 * Period: fixed 50ms via vTaskDelayUntil — absorbs the variable US blocking time
 * (0–25ms per sensor) so the cycle is always 50ms regardless of echo timing.
 * Full US scan: 6 cycles × 50ms = 300ms.
 *
 * Speed stored as cm/s so ADAS TTC (distance_cm / speed_cm_s) yields seconds.
 */
void vTask_Sensors(void *pvParameters)
{
  static uint8_t           us_index       = 0;
  static float             local_speed_ms = 0.0f;
  static MPU9250_Position_t local_pos     = {0};
  MPU9250_Data_t  mpu_data               = {0};

  TickType_t xLastWakeTime = xTaskGetTickCount();
  TickType_t xPrevTick     = xLastWakeTime;

  for(;;)
  {
    /* ── Actual dt: time since task last woke (≈50ms in steady state) ── */
    TickType_t xNow = xTaskGetTickCount();
    float dt = (float)(xNow - xPrevTick) * 0.001f;   /* ms → seconds */
    if (dt <= 0.0f) dt = 0.050f;                      /* guard: first run */
    xPrevTick = xNow;

    /* ── Round-Robin US: one sensor per cycle (~25ms max blocking) ── */
    uint16_t dist_raw  = 0;
    float    dist_cm   = 400.0f;   /* default = out of range (US max ~400cm) */
    ErrorState_t us_err;

    if (us_index < 3)
      us_err = US_u16ReadDistance_cm(&FrontUS[us_index],      &dist_raw);
    else
      us_err = US_u16ReadDistance_cm(&BackUS[us_index - 3],   &dist_raw);

    if (us_err == OK)
      dist_cm = (float)dist_raw;
    /* TIMEOUT_STATE (no echo): dist_cm stays 400.0f — safe / clear */

    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
    switch (us_index)
    {
      case 0: G_stHostVehicleState.FrontLeftUS   = dist_cm; break;
      case 1: G_stHostVehicleState.FrontCenterUS = dist_cm; break;
      case 2: G_stHostVehicleState.FrontRightUS  = dist_cm; break;
      case 3: G_stHostVehicleState.BackLeftUS    = dist_cm; break;
      case 4: G_stHostVehicleState.BackCenterUS  = dist_cm; break;
      case 5: G_stHostVehicleState.BackRightUS   = dist_cm; break;
      default: break;
    }
    xSemaphoreGive(G_xDataMutex);

    us_index = (us_index + 1) % 6;

    /* ── MPU9250: read and process every cycle ───────────────────── */
    MPU9250_enumReadData(&mpu_data);

    float pitch = 0.0f, roll = 0.0f, heading = 0.0f;
    MPU9250_enumGetAttitude(&mpu_data, dt, &pitch, &roll);
    MPU9250_enumGetHeading(&mpu_data, &heading);
    MPU9250_enumGetSpeed(&mpu_data, dt, &local_speed_ms);
    MPU9250_enumGetPosition(&mpu_data, local_speed_ms, heading, pitch, dt, &local_pos);

    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
    G_stHostVehicleState.Speed   = local_speed_ms * 100.0f;  /* m/s → cm/s */
    G_stHostVehicleState.Heading = heading;
    G_stHostVehicleState.Pitch   = pitch;
    G_stHostVehicleState.Roll    = roll;
    G_stHostVehicleState.PosX    = local_pos.X;
    G_stHostVehicleState.PosY    = local_pos.Y;
    G_stHostVehicleState.PosZ    = local_pos.Z;
    xSemaphoreGive(G_xDataMutex);

    /* ── Fixed 50ms period: max work ~27ms → ~23ms slack guaranteed ── */
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
  }
}
/**
 * @brief Centralized Hardware Manager (Muscle).
 * Executes commands decided by the ADAS logic (Brain).
 */


void vTask_Feedback(void *pvParameters)
{
  for(;;)
  {

    /* Keep execution consistent */
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}


/**
 * @brief Handles both RX and TX communication with ESP (V2X Network) using DSRC foundation.
 *
 * RX path  (event-driven): blocks on G_xESP_RX_Queue, drains all pending bytes
 *          into DSRC state machine, then flushes assembled packets into neighbor table.
 * TX path  (periodic 100ms): builds a Neighbor from current host state + ADAS flags
 *          and broadcasts via DSRC_SendNeighbor.
 */
void vTask_ESP_Comm(void *pvParameters)
{
  TickType_t xLastTXTime = xTaskGetTickCount();

  for(;;)
  {
    /* ── RX: event-driven byte processing ───────────────────────── */
    uint8_t byte;
    if (xQueueReceive(G_xESP_RX_Queue, &byte, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      DSRC_RxCallback(byte);

      /* Drain every remaining byte without blocking */
      while (xQueueReceive(G_xESP_RX_Queue, &byte, 0) == pdTRUE)
      {
        DSRC_RxCallback(byte);
      }
    }

    /* ── Neighbor table maintenance — runs EVERY iteration (~10ms) ──
     * DSRC_Update flushes any newly-assembled packets (no-op if none).
     * DSRC_RemoveStale MUST run even when no bytes arrived, otherwise a
     * vehicle that goes silent (out of range / powered off) would never
     * be purged and would haunt the table forever. */
    xSemaphoreTake(G_xNeighborTableMutex, portMAX_DELAY);
    DSRC_Update();
    DSRC_RemoveStale((uint32_t)xTaskGetTickCount());
    xSemaphoreGive(G_xNeighborTableMutex);

    /* ── TX: periodic broadcast every 100 ms ────────────────────── */
    if ((xTaskGetTickCount() - xLastTXTime) >= pdMS_TO_TICKS(100))
    {
      xLastTXTime = xTaskGetTickCount();

      Neighbor my_data = {0};
      my_data.vehicle_id = VEHICLE_ID;
      my_data.last_update = (uint32_t)xTaskGetTickCount();

      /* Read host state under mutex */
      xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
      my_data.speed                    = G_stHostVehicleState.Speed;
      my_data.heading                  = G_stHostVehicleState.Heading;
      my_data.distance_to_intersection = G_stHostVehicleState.DistToIntersection;
      xSemaphoreGive(G_xDataMutex);

      /* ADAS flags are atomic uint8 reads — no mutex needed */
      my_data.fcw_flag  = FCW_u8GetFlag();
      my_data.dnpw_flag = DNPW_u8GetFlag();
      my_data.ima_flag  = IMA_u8GetFlag();

      DSRC_SendNeighbor(&my_data);
    }
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
