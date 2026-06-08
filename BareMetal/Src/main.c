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
#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/DNPW/DNPW_interface.h"
#include "../Inc/Application/IMA/IMA_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

extern BUZ_Config_t      V2X_Buzzer;
extern USART_Config_t    RPi_UART;
extern US_Config_t FrontUS[3];
extern US_Config_t BackUS[3];
/* Motors are driven by the Raspberry Pi — no L298N on the STM32 */
/* LEDs: FrontR=PC0, FrontL=PC1, BackR=PC2, BackL=PC3, Interior(driver)=PC7 */
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED, Interior_LED;


QueueHandle_t G_xESP_RX_Queue;
SemaphoreHandle_t G_xDataMutex;
SemaphoreHandle_t G_xNeighborTableMutex;



/* ================== Task Prototypes ================== */
void vTask_SafetyEngine(void *pvParameters);
void vTask_Sensors(void *pvParameters);
void vTask_Feedback(void *pvParameters);

/* Communication Tasks */
void vTask_ESP_Comm(void *pvParameters);
void vTask_RPi_Comm(void *pvParameters);


int main(void)
{
  /*for segger*/
  vInitPrioGroupValue();
  /* 1. Hardware Initialization */
  System_setup();
  SEGGER_setup();

  /* Create Queues & Mutexes */
  G_xESP_RX_Queue = xQueueCreate(256, sizeof(uint8_t));
  G_xDataMutex = xSemaphoreCreateMutex();
  G_xNeighborTableMutex = xSemaphoreCreateMutex();

  /* Initialize all ADAS modules (FCW/EEBL/BSW/DNPW/IMA) before scheduling */
  SafetyEngine_voidInit();

  /* 2. OS Tasks Creation - Pipeline Architecture */
  /* --- High Priority: single-pass ADAS brain + V2X communication --- */
  xTaskCreate(vTask_SafetyEngine, "SafetyEngine_Task", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
  xTaskCreate(vTask_ESP_Comm,     "ESP_Comm_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);
  /* --- Medium Priority: Data Acquisition (priority 3 — US blocking preempted by priority 4) --- */
  xTaskCreate(vTask_Sensors,      "Sensors_Task",      configMINIMAL_STACK_SIZE + 256, NULL, 3, NULL);
  /* --- Low Priority: Actuator Execution & UI/RPi --- */
  xTaskCreate(vTask_Feedback,     "Feedback_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
  xTaskCreate(vTask_RPi_Comm,     "RPi_Comm_Task",     configMINIMAL_STACK_SIZE + 100, NULL, 1, NULL);

  /* 3. OS Initialization and start running tasks */
  RTOS_setup();

  /* 4. Should never be reached unless scheduler fails */
  for (;;);

}

/* ================== Task Implementations ================== */

/**
 * @brief Single-pass ADAS brain.
 *
 * Every 50ms: takes BOTH mutexes (neighbor table + host state), then
 * SafetyEngine_voidUpdate() runs detection for ALL modules in one pass and
 * aggregates the result into the G_u16SystemFlags status word (2 bits/module)
 * consumed by vTask_Feedback and vTask_RPi_Comm.
 *
 * Lock order: NeighborTable → Data. vTask_ESP_Comm takes them separately
 * (never nested) and vTask_Sensors takes Data only → no deadlock possible.
 */
void vTask_SafetyEngine(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    xSemaphoreTake(G_xNeighborTableMutex, portMAX_DELAY);
    xSemaphoreTake(G_xDataMutex,          portMAX_DELAY);

    SafetyEngine_voidUpdate();

    xSemaphoreGive(G_xDataMutex);
    xSemaphoreGive(G_xNeighborTableMutex);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
  }
}

/* Small rest after each full scan: gives lower-priority tasks guaranteed CPU
 * and sets the minimum spacing between scans. The scan itself is adaptive —
 * its length follows echo flight time (near objects → faster refresh). */
#define SENSORS_CYCLE_GAP_MS  10U

/**
 * @brief Reads ALL 6 ultrasonics + MPU9250 every cycle, then a small gap.
 *
 * The US driver is interrupt-driven: the task SLEEPS during each echo flight
 * (CPU free), so all 6 can be read back-to-back without hogging the CPU.
 * Reads are sequential → no acoustic cross-talk.
 *
 * Adaptive refresh: full scan ≈ sum of echo times (near objects → ~15-45ms,
 * all out of range → ~72ms with the 2m cap). Much faster than the old 300ms.
 *
 * All values are computed into locals first, then published in ONE short mutex
 * section. Speed stored as cm/s so ADAS TTC (distance_cm / speed_cm_s) = seconds.
 */
void vTask_Sensors(void *pvParameters)
{
  static float             local_speed_ms = 0.0f;
  static MPU9250_Position_t local_pos     = {0};
  MPU9250_Data_t  mpu_data               = {0};

  /* Interleaved order: each pair is geographically distant (front↔back same side)
   * → no two adjacent reads share an acoustic path → cross-talk eliminated. */
  const US_Config_t *sensors[6] = {
      &FrontUS[0], &BackUS[0],   /* Left:   front then back  */
      &FrontUS[1], &BackUS[1],   /* Center: front then back  */
      &FrontUS[2], &BackUS[2]    /* Right:  front then back  */
  };

  TickType_t xPrevTick = xTaskGetTickCount();

  for(;;)
  {
    /* ── Actual dt since last cycle (variable: follows scan length) ── */
    TickType_t xNow = xTaskGetTickCount();
    float dt = (float)(xNow - xPrevTick) * 0.001f;   /* ms → seconds */
    if (dt <= 0.0f) dt = 0.010f;                      /* guard: first run */
    xPrevTick = xNow;

    /* ── Read ALL 6 ultrasonics into locals (task sleeps during each echo) ── */
    float    us[6];
    uint16_t raw;
    for (uint8_t i = 0; i < 6; i++)
    {
      us[i] = 400.0f;   /* default = out of range / clear (no echo within 2m) */
      if (US_u16ReadDistance_cm(sensors[i], &raw) == OK)
        us[i] = (float)raw;
    }

    /* ── MPU9250: read and process into locals ── */
    MPU9250_enumReadData(&mpu_data);

    float pitch = 0.0f, roll = 0.0f, heading = 0.0f;
    MPU9250_enumGetAttitude(&mpu_data, dt, &pitch, &roll);
    MPU9250_enumGetHeading(&mpu_data, &heading);
    MPU9250_enumGetSpeed(&mpu_data, dt, &local_speed_ms);
    MPU9250_enumGetPosition(&mpu_data, local_speed_ms, heading, pitch, dt, &local_pos);

    /* ── Publish everything in ONE short mutex section ── */
    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
    G_stHostVehicleState.FrontLeftUS   = us[0];
    G_stHostVehicleState.BackLeftUS    = us[1];
    G_stHostVehicleState.FrontCenterUS = us[2];
    G_stHostVehicleState.BackCenterUS  = us[3];
    G_stHostVehicleState.FrontRightUS  = us[4];
    G_stHostVehicleState.BackRightUS   = us[5];
    G_stHostVehicleState.Speed   = local_speed_ms * 100.0f;  /* m/s → cm/s */
    G_stHostVehicleState.Heading = heading;
    G_stHostVehicleState.Pitch   = pitch;
    G_stHostVehicleState.Roll    = roll;
    G_stHostVehicleState.PosX    = local_pos.X;
    G_stHostVehicleState.PosY    = local_pos.Y;
    G_stHostVehicleState.PosZ    = local_pos.Z;
    xSemaphoreGive(G_xDataMutex);

    /* ── Small adaptive gap (other tasks get CPU; sets min scan spacing) ── */
    vTaskDelay(pdMS_TO_TICKS(SENSORS_CYCLE_GAP_MS));
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
    uint16_t flags = G_u16SystemFlags;

    if (flags == 0)
    {
      /* ── SAFE: everything off (motors handled by the Raspberry Pi) ── */
      LED_TurnOff(&FrontR_LED);
      LED_TurnOff(&FrontL_LED);
      LED_TurnOff(&BackR_LED);
      LED_TurnOff(&BackL_LED);
      LED_TurnOff(&Interior_LED);
      BUZ_Off(&V2X_Buzzer);
    }
    else
    {
      /* ── GENERAL alert: ANY active system → buzzer + interior LED ──
       * This is also the FULL response for BSW and DNPW (no external LEDs). */
      LED_TurnOn(&Interior_LED);
      BUZ_On(&V2X_Buzzer);

      /* Per-module status (0/1/2) from the packed word */
      uint8_t fcw  = SYS_GET(flags, SYS_FCW_POS);
      uint8_t eebl = SYS_GET(flags, SYS_EEBL_POS);
      uint8_t ima  = SYS_GET(flags, SYS_IMA_POS);

      /* Front LEDs: FCW or IMA (forward threats) */
      if (fcw || ima) { LED_TurnOn(&FrontR_LED);  LED_TurnOn(&FrontL_LED);  }
      else            { LED_TurnOff(&FrontR_LED); LED_TurnOff(&FrontL_LED); }

      /* Back LEDs: EEBL (rear threat) */
      if (eebl) { LED_TurnOn(&BackR_LED);  LED_TurnOn(&BackL_LED);  }
      else      { LED_TurnOff(&BackR_LED); LED_TurnOff(&BackL_LED); }
    }

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
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;)
  {
    RPi_Packet_t pkt;

    /* Snapshot host state under mutex */
    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
    pkt.FrontLeftUS   = G_stHostVehicleState.FrontLeftUS;
    pkt.FrontCenterUS = G_stHostVehicleState.FrontCenterUS;
    pkt.FrontRightUS  = G_stHostVehicleState.FrontRightUS;
    pkt.BackLeftUS    = G_stHostVehicleState.BackLeftUS;
    pkt.BackCenterUS  = G_stHostVehicleState.BackCenterUS;
    pkt.BackRightUS   = G_stHostVehicleState.BackRightUS;
    pkt.speed         = G_stHostVehicleState.Speed;
    pkt.heading       = G_stHostVehicleState.Heading;
    pkt.pitch         = G_stHostVehicleState.Pitch;
    pkt.roll          = G_stHostVehicleState.Roll;
    xSemaphoreGive(G_xDataMutex);

    /* 16-bit status word — atomic read, no mutex needed */
    pkt.sys_flags = G_u16SystemFlags;

    /* Framing (no checksum) */
    pkt.start = 0xAAU;
    pkt.end   = 0x55U;

    /* Send the whole frame byte by byte */
    const uint8_t *raw = (const uint8_t *)&pkt;
    for (uint16_t i = 0; i < sizeof(RPi_Packet_t); i++)
      USART_enumTransmit(&RPi_UART, raw[i]);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
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
