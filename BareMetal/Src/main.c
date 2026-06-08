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
#include "../Inc/Application/EEBL/EEBL_interface.h"
#include "../Inc/Application/BSW/BSW_interface.h"
#include "../Inc/Application/DNPW/DNPW_interface.h"
#include "../Inc/Application/IMA/IMA_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

extern BUZ_Config_t      V2X_Buzzer;
extern USART_Config_t    RPi_UART;
extern US_Config_t FrontUS[3];
extern US_Config_t BackUS[3];
/* LEDs: FrontR=PC0, FrontL=PC1, BackR=PC2, BackL=PC3, Interior(driver)=PC7 */
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED, Interior_LED;


QueueHandle_t G_xESP_RX_Queue;        // queue for ESP32 communication
SemaphoreHandle_t G_xDataMutex;           // for data protection (G_stHostVehicleState and G_u8SystemFlags)
SemaphoreHandle_t G_xNeighborTableMutex;  // for neighbor table protection (neighbor_table)



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
  G_xESP_RX_Queue = xQueueCreate(256, sizeof(uint8_t)); // 256 element queue (byte)
  G_xDataMutex = xSemaphoreCreateMutex();                 // for data protection (G_stHostVehicleState and G_u8SystemFlags)
  G_xNeighborTableMutex = xSemaphoreCreateMutex();        // for neighbor table protection (neighbor_table)

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
  xTaskCreate(vTask_RPi_Comm,     "RPi_Comm_Task",     configMINIMAL_STACK_SIZE + 256, NULL, 1, NULL);

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
 * aggregates the result into the centralized command channel
 * (G_u8SystemFlags bitmap) consumed by vTask_Feedback.
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

  TickType_t xPrevTick = xTaskGetTickCount(); // Time since last cycle

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
    //  us[i] = 400.0f;   /* default = out of range / clear (no echo within 2m) */
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
    if (G_u8SystemFlags == 0)
    {
      /* ── SAFE: all alerts off, move forward ── */
      LED_TurnOff(&FrontR_LED);
      LED_TurnOff(&FrontL_LED);
      LED_TurnOff(&BackR_LED);
      LED_TurnOff(&BackL_LED);
      LED_TurnOff(&Interior_LED);
      BUZ_Off(&V2X_Buzzer);

    }
    else
    {
      /* ── GENERAL response: ANY active system warns the driver ──
       * We reach this branch only because G_u8SystemFlags != 0, i.e. at least
       * one module raised an alert → buzzer + interior LED ON unconditionally.
       * This is also the FULL response for DNPW and IMA (no external LEDs). */
      LED_TurnOn(&Interior_LED);
      BUZ_On(&V2X_Buzzer);

      /* ── PER-SYSTEM external indicators ── */
      uint8_t fcw       = (G_u8SystemFlags & SYSFLG_FCW)  ? FCW_u8GetAlertLevel()  : 0;
      uint8_t eebl      = (G_u8SystemFlags & SYSFLG_EEBL) ? EEBL_u8GetAlertLevel() : 0;
      uint8_t bsw_left  = (G_u8SystemFlags & SYSFLG_BSW)  ? BSW_u8GetLeftFlag()    : 0;
      uint8_t bsw_right = (G_u8SystemFlags & SYSFLG_BSW)  ? BSW_u8GetRightFlag()   : 0;

      /* FCW: forward threat → front LEDs */
      if (fcw >= 1) { LED_TurnOn(&FrontR_LED);  LED_TurnOn(&FrontL_LED);  }
      else          { LED_TurnOff(&FrontR_LED); LED_TurnOff(&FrontL_LED); }

      /* BackR: EEBL rear threat OR BSW right blind-spot */
      if (eebl >= 1 || bsw_right) { LED_TurnOn(&BackR_LED);  }
      else                        { LED_TurnOff(&BackR_LED); }

      /* BackL: EEBL rear threat OR BSW left blind-spot */
      if (eebl >= 1 || bsw_left)  { LED_TurnOn(&BackL_LED);  }
      else                        { LED_TurnOff(&BackL_LED); }
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

  char line[96];

  for (;;)
  {
    float spd, hdg, fl, fc, fr, bl, bc, br;
    uint8_t flags;

    /* Read shared state under mutex */
    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
    spd = G_stHostVehicleState.Speed;
    hdg = G_stHostVehicleState.Heading;
    fl  = G_stHostVehicleState.FrontLeftUS;
    fc  = G_stHostVehicleState.FrontCenterUS;
    fr  = G_stHostVehicleState.FrontRightUS;
    bl  = G_stHostVehicleState.BackLeftUS;
    bc  = G_stHostVehicleState.BackCenterUS;
    br  = G_stHostVehicleState.BackRightUS;
    xSemaphoreGive(G_xDataMutex);

    /* G_u8SystemFlags is volatile uint8 — atomic read, no mutex needed */
    flags = G_u8SystemFlags;

    /* ASCII CSV line — contains NO 0x00 bytes, so it survives the UART link that
     * was losing the binary packet's zero-runs. '\n'-delimited, trivial to parse.
     * Format: T,speed,heading,FL,FC,FR,BL,BC,BR,flags\n */
    snprintf(line, sizeof(line),
             "T,%.1f,%.1f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%u\n",
             spd, hdg, fl, fc, fr, bl, bc, br, (unsigned)flags);

    USART_enumTransmitString(&RPi_UART, (uint8_t *)line);

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
