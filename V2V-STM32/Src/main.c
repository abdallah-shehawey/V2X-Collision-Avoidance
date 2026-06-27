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
#include "../Inc/Application/FCW_DNPW/FCW_DNPW_interface.h"
#include "../Inc/Application/EEBL/EEBL_interface.h"
#include "../Inc/Application/BSW/BSW_interface.h"
#include "../Inc/Application/IMA/IMA_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/Drivers/MCAL/IWDG/IWDG_interface.h"

extern BUZ_Config_t      V2X_Buzzer;
extern USART_Config_t    RPi_UART;
extern US_Config_t FrontUS[3];
extern US_Config_t BackUS[3];
/* LEDs: FrontR=PC0, FrontL=PC1, BackR=PC2, BackL=PC3, Interior(driver)=PC7 */
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED, Interior_LED;


QueueHandle_t G_xESP_RX_Queue;        // queue for ESP32 communication
SemaphoreHandle_t G_xDataMutex;           // for data protection (G_stHostVehicleState and G_u16SystemFlags)
SemaphoreHandle_t G_xNeighborTableMutex;  // for neighbor table protection (neighbor_table)


/* ================== Watchdog heartbeats ==================
 * Each monitored task bumps its own slot every loop. vTask_Watchdog refreshes
 * the IWDG ONLY while EVERY slot keeps advancing; if any task stalls (hard
 * fault, stack-overflow halt, infinite loop, ISR storm, or starvation) its slot
 * freezes → the refresh stops → the IWDG hardware-resets the MCU. */
enum { HB_SAFETY = 0, HB_SENSORS, HB_ESP, HB_FEEDBACK, HB_RPI, HB_COUNT };
volatile uint32_t G_au32Heartbeat[HB_COUNT] = {0};

#define WDG_TIMEOUT_MS        2000U   /* IWDG hardware reset timeout (LSI-backed) */
#define WDG_CHECK_PERIOD_MS    300U   /* how often the monitor verifies + kicks   */


/* ================== Task Prototypes ================== */
void vTask_SafetyEngine(void *pvParameters);
void vTask_Sensors(void *pvParameters);
void vTask_Feedback(void *pvParameters);

/* Communication Tasks */
void vTask_ESP_Comm(void *pvParameters);
void vTask_RPi_Comm(void *pvParameters);

/* Reliability */
void vTask_Watchdog(void *pvParameters);


int main(void)
{
  /*for segger*/
  vInitPrioGroupValue();
  /* 1. Hardware Initialization */
  System_setup();
  SEGGER_setup();

  /* Create Queues & Mutexes */
  G_xESP_RX_Queue = xQueueCreate(256, sizeof(uint8_t)); // 256 element queue (byte)
  G_xDataMutex = xSemaphoreCreateMutex();                 // for data protection (G_stHostVehicleState and G_u16SystemFlags)
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
  /* --- Lowest Priority: liveness monitor → kicks the IWDG (must run last) --- */
  xTaskCreate(vTask_Watchdog,     "Watchdog_Task",     configMINIMAL_STACK_SIZE,       NULL, 1, NULL);

  /* Start the independent watchdog LAST, just before the scheduler. From here
   * the IWDG must be kicked within WDG_TIMEOUT_MS or the MCU hardware-resets. */
  IWDG_voidInit(WDG_TIMEOUT_MS);

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
    G_au32Heartbeat[HB_SAFETY]++;

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
 * all out of range → ~150ms with the 4m cap). Much faster than the old 300ms.
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

  /* Timestamp of the PREVIOUS MPU sample. dt is measured right around the MPU
   * read (not at loop top), so it is the true interval between two consecutive
   * IMU samples — independent of the variable-length ultrasonic scan that runs
   * in between. This keeps the attitude/heading/speed/position integration exact. */
  TickType_t xPrevMpuTick = xTaskGetTickCount();

  for(;;)
  {
    G_au32Heartbeat[HB_SENSORS]++;

    /* ── Read ALL 6 ultrasonics into locals (task sleeps during each echo) ── */
    float    us[6];
    uint16_t raw;
    for (uint8_t i = 0; i < 6; i++)
    {
      us[i] = 400.0f;   /* default = out of range / clear (no echo within 4m) */
      if (US_u16ReadDistance_cm(sensors[i], &raw) == OK)
        us[i] = (float)raw;
    }

    /* ── dt = interval since the previous MPU sample (measured at the read) ── */
    TickType_t xNow = xTaskGetTickCount();
    float dt = (float)(xNow - xPrevMpuTick) * 0.001f;  /* ms → seconds */
    if (dt <= 0.0f) dt = 0.010f;                        /* guard: first run */
    xPrevMpuTick = xNow;

    /* ── MPU9250: read and process into locals ── */
    MPU9250_enumReadData(&mpu_data);

    float pitch = 0.0f, roll = 0.0f, heading = 0.0f;
    MPU9250_enumGetAttitude(&mpu_data, dt, &pitch, &roll);
    MPU9250_enumGetHeading(&mpu_data, dt, &heading);
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
 * @brief Alert manager (the "Muscle"): drives the LEDs + buzzer from the ADAS
 *        status word. It makes NO decision of its own — it only renders the
 *        severity that vTask_SafetyEngine already published. (Motors are driven
 *        by the Raspberry Pi, not here.)
 *
 * Reads G_u16SystemFlags (2 bits/module: 00 safe / 01 warning / 10 critical) and
 * maps it to the actuators exactly as agreed:
 *   • all safe (word == 0)        → everything OFF.
 *   • ANY alert (warning or crit) → interior dashboard LED + buzzer ON.
 *   • FCW  == CRITICAL            → additionally the FRONT LEDs.
 *   • EEBL == CRITICAL            → additionally the REAR  LEDs.
 *   • everything else (BSW/DNPW/IMA, or FCW/EEBL at WARNING) adds no extra LED —
 *     the interior LED + buzzer is their full response.
 */
void vTask_Feedback(void *pvParameters)
{
  for(;;)
  {
    G_au32Heartbeat[HB_FEEDBACK]++;

    /* Single atomic read of the volatile status word (uint16 read is atomic on M4). */
    uint16_t flags = G_u16SystemFlags;

    if (flags == 0)
    {
      /* ── ALL SAFE: no hazard from any module → silence everything ── */
      LED_TurnOff(&FrontR_LED);
      LED_TurnOff(&FrontL_LED);
      LED_TurnOff(&BackR_LED);
      LED_TurnOff(&BackL_LED);
      LED_TurnOff(&Interior_LED);
      BUZ_Off(&V2X_Buzzer);
    }
    else
    {
      /* ── ANY alert (any module, warning OR critical) → warn the driver ── */
      LED_TurnOn(&Interior_LED);
      BUZ_On(&V2X_Buzzer);

      /* FCW CRITICAL → front LEDs (imminent forward collision). */
      if (SYS_GET(flags, SYS_FCW_POS) == SYS_CRITICAL)
      { LED_TurnOn(&FrontR_LED);  LED_TurnOn(&FrontL_LED);  }
      else
      { LED_TurnOff(&FrontR_LED); LED_TurnOff(&FrontL_LED); }

      /* EEBL CRITICAL → rear LEDs (hard braking → warn the car behind). */
      if (SYS_GET(flags, SYS_EEBL_POS) == SYS_CRITICAL)
      { LED_TurnOn(&BackR_LED);  LED_TurnOn(&BackL_LED);  }
      else
      { LED_TurnOff(&BackR_LED); LED_TurnOff(&BackL_LED); }
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
    G_au32Heartbeat[HB_ESP]++;

    /* ── RX: event-driven byte processing ───────────────────────── */
    uint8_t byte; //  Buffer to hold received byte from ESP32
    if (xQueueReceive(G_xESP_RX_Queue, &byte, pdMS_TO_TICKS(10)) == pdTRUE) // Waits for 10ms for a byte to be received
    {
      DSRC_RxCallback(byte); // Calls the callback function to process the received byte

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
      /* last_update is intentionally left 0: the receiver overwrites it with its
       * OWN local FreeRTOS tick in update_neighbor() (a sender's clock is
       * meaningless to us), so transmitting it would only waste bytes. */

      /* Read host state AND the cooperative ADAS flags under the SAME mutex, so
       * the broadcast packet is one consistent snapshot of the last SafetyEngine
       * cycle. vTask_SafetyEngine writes these module statics while holding
       * G_xDataMutex, so taking it here closes the race — notably IMA_u8GetFlag(),
       * which reads three statics and is NOT atomic.
       *   fcw_headon_flag : head-on candidate (0/1) for the oncoming car to confirm
       *   bsw_flag        : my own front side(s) seeing a car (bit0=L, bit1=R)
       *   ima_flag        : intersection movement assist (0/1/2)
       *   distance_to_intersection : for the neighbors' IMA geometry */
      xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
      my_data.speed                    = G_stHostVehicleState.Speed;
      my_data.heading                  = G_stHostVehicleState.Heading;
      my_data.fcw_headon_flag          = FCW_GetHeadonFlag();
      my_data.bsw_flag                 = BSW_u8GetFlag();
      my_data.ima_flag                 = IMA_u8GetFlag();
      my_data.distance_to_intersection = Host_DistToIntersection;
      xSemaphoreGive(G_xDataMutex);

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
    G_au32Heartbeat[HB_RPI]++;

    float spd, hdg, pit, rol, fl, fc, fr, bl, bc, br;
    uint16_t flags;

    /* Read shared state under mutex */
    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);
    spd = G_stHostVehicleState.Speed;
    hdg = G_stHostVehicleState.Heading;
    pit = G_stHostVehicleState.Pitch;
    rol = G_stHostVehicleState.Roll;
    fl  = G_stHostVehicleState.FrontLeftUS;
    fc  = G_stHostVehicleState.FrontCenterUS;
    fr  = G_stHostVehicleState.FrontRightUS;
    bl  = G_stHostVehicleState.BackLeftUS;
    bc  = G_stHostVehicleState.BackCenterUS;
    br  = G_stHostVehicleState.BackRightUS;
    xSemaphoreGive(G_xDataMutex);

    /* 16-bit ADAS status word (2 bits/module) — volatile read is atomic, no mutex.
     * The RPi decodes each module via (flags >> SYS_xxx_POS) & 0x3. */
    flags = G_u16SystemFlags;

    /* ASCII CSV line — contains NO 0x00 bytes, so it survives the UART link that
     * was losing the binary packet's zero-runs. '\n'-delimited, trivial to parse.
     * Format: T,speed,heading,pitch,roll,FL,FC,FR,BL,BC,BR,flags\n */
    snprintf(line, sizeof(line),
             "T,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%u\n",
             spd, hdg, pit, rol, fl, fc, fr, bl, bc, br, (unsigned)flags);

    USART_enumTransmitString(&RPi_UART, (uint8_t *)line);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
  }
}


/**
 * @brief Liveness monitor + watchdog kicker (lowest priority).
 *
 * Every WDG_CHECK_PERIOD_MS it snapshots every task's heartbeat. It refreshes
 * the IWDG ONLY if ALL of them advanced since the last check. If any task
 * stalled — hard fault / stack-overflow halt / infinite loop / ISR storm /
 * starvation — its heartbeat freezes, the refresh is skipped, and the IWDG
 * hardware-resets the MCU after WDG_TIMEOUT_MS. Transient stalls shorter than
 * the IWDG timeout are tolerated (the next healthy check kicks the dog again).
 *
 * Runs at the LOWEST user priority so that CPU starvation by ANY other task
 * also stops the kicks. The task is self-covering: if IT hangs, no kicks → reset.
 */
void vTask_Watchdog(void *pvParameters)
{
  uint32_t snapshot[HB_COUNT];
  for (uint8_t i = 0; i < HB_COUNT; i++)
    snapshot[i] = G_au32Heartbeat[i];

  IWDG_voidRefresh();   /* initial kick so the window starts clean */

  TickType_t xLastWake = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(WDG_CHECK_PERIOD_MS));

    uint8_t all_alive = 1;
    for (uint8_t i = 0; i < HB_COUNT; i++)
    {
      if (G_au32Heartbeat[i] == snapshot[i]) all_alive = 0;  /* this task stalled */
      snapshot[i] = G_au32Heartbeat[i];
    }

    /* Kick the dog only while EVERY monitored task is still advancing.
     * If any stalled, deliberately skip the refresh → IWDG resets the MCU. */
    if (all_alive)
      IWDG_voidRefresh();
  }
}


/* ================== ISR callbacks ================== */
void vESP_UART_RX_Callback(void)
{
    /* Read exact byte directly from hardware Data Register (DR).
     * This avoids any software busy-flag deadlocks when TX is transmitting simultaneously!
     */
    uint8_t rxData = USART_ReceiveByteDirect(USART_CHANNEL1); /* ESP/DSRC on USART1 */

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(G_xESP_RX_Queue, &rxData, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
