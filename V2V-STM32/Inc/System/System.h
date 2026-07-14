/**
 ******************************************************************************
 * @file    System.h
 * @author  Abdallah Saleh
 * @brief   System-wide contract: the board wiring map, the shared vehicle
 *          state, the packed ADAS status word, and the telemetry format.
 * @ingroup system
 *
 * @details
 * Everything in this header is shared between tasks, so it is also where the
 * concurrency rules live. Three globals cross task boundaries:
 *
 * - @ref G_stHostVehicleState — written by `Sensors_Task`, read by everyone else.
 * - @ref G_u16SystemFlags     — written by `SafetyEngine_Task`, read by
 *   `Feedback_Task` and `RPi_Comm_Task`.
 * - @ref Host_DistToIntersection — written by whatever supplies intersection
 *   geometry, read by IMA.
 *
 * Both mutexes are always taken in the order **NeighborTable → Data**, and never
 * nested from more than one place, which is what makes the task set deadlock-free
 * by construction.
 *
 * @section sys_wiring Board wiring map
 *
 * **Ultrasonic sensors — 3 front, 3 rear.** Echo pins are EXTI inputs; trigger
 * pins are plain outputs. The echo pins were chosen to leave PA0..PA3 free for
 * the UARTs.
 *
 * | ID | Location     | Echo | Trigger | Timer channel |
 * |----|--------------|------|---------|---------------|
 * | S1 | Front left   | PA15 | PB0     | TIM2_CH1      |
 * | S2 | Front centre | PB3  | PB1     | TIM2_CH2      |
 * | S3 | Front right  | PB4  | PB2     | TIM3_CH1      |
 * | S4 | Rear left    | PB5  | PB12    | TIM3_CH2      |
 * | S5 | Rear centre  | PC8  | PB13    | TIM3_CH3      |
 * | S6 | Rear right   | PC9  | PB14    | TIM3_CH4      |
 *
 * **MPU9250 IMU** — SPI1 on PA5 (SCK), PA6 (MISO/ADO), PA7 (MOSI/SDA), AF5.
 *
 * **Feedback devices** — all on port C:
 *
 * | Device   | Pin | Role                              |
 * |----------|-----|-----------------------------------|
 * | LED 1    | PC0 | Front right                       |
 * | LED 2    | PC1 | Front left                        |
 * | LED 3    | PC2 | Rear right                        |
 * | LED 4    | PC3 | Rear left                         |
 * | LED 5    | PC7 | Interior / dashboard driver alert |
 * | Buzzer   | PC4 | Audible warning                   |
 *
 * **Serial links:**
 *
 * | Port   | Pins             | Use                                      |
 * |--------|------------------|------------------------------------------|
 * | USART1 | PA9 TX, PA10 RX  | ESP32 radio bridge (ESP-NOW / V2V)       |
 * | USART2 | PA2 TX, PA3 RX   | Raspberry Pi telemetry                   |
 * | UART4  | PA0 TX, PA1 RX   | free (previously the Raspberry Pi link)  |
 *
 * @note There is **no motor driver on the STM32**. The Raspberry Pi drives the
 *       motors from the telemetry this firmware sends, so no pin here is a
 *       motor pin.
 *
 * @section sys_tasks Task set
 *
 * `configMAX_PRIORITIES` is 5, so priority 4 is the highest available to a task
 * and priority 0 belongs to the FreeRTOS idle task.
 *
 * | Task                 | Priority | Period              | Role |
 * |----------------------|----------|---------------------|------|
 * | `vTask_SafetyEngine` | 4        | 50 ms               | The **brain**: runs every ADAS module over the neighbor table in one pass and publishes @ref G_u16SystemFlags. Decides nothing about actuators. Holds both mutexes. |
 * | `vTask_ESP_Comm`     | 4        | RX ~10 ms / TX 100 ms | V2V traffic over USART1. Takes the two mutexes separately. |
 * | `vTask_Sensors`      | 3        | ~25–82 ms, adaptive | Reads all 6 ultrasonics (interrupt-driven — the task *sleeps* during each echo) and the IMU. Sequential reads, so no acoustic cross-talk. |
 * | `vTask_Feedback`     | 2        | 25 ms               | The **muscle**: renders @ref G_u16SystemFlags onto the LEDs and buzzer. |
 * | `vTask_RPi_Comm`     | 1        | 100 ms              | Streams the telemetry line to the Raspberry Pi. |
 *
 * @warning `configMAX_SYSCALL_INTERRUPT_PRIORITY` is 5, so any interrupt that
 *          calls a FreeRTOS `...FromISR` API — USART1 included — must be given an
 *          NVIC priority number of **6 or higher** (i.e. a *lower* urgency).
 ******************************************************************************
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

/**
 * @addtogroup system
 * @{
 */

/**
 * @brief Everything the sensors know about *this* vehicle, in one place.
 *
 * Produced by `Sensors_Task` once per scan and consumed by the ADAS modules, the
 * telemetry task and the DSRC broadcast. Access is serialised by the Data mutex.
 *
 * The six ultrasonic readings are distances in centimetres; a sensor that saw no
 * echo reports its out-of-range ceiling (about 200 cm) rather than 0, so "far
 * away" never looks like "touching".
 */
typedef struct
{
  float FrontLeftUS;   /**< Front-left ultrasonic distance [cm]. */
  float FrontCenterUS; /**< Front-centre ultrasonic distance [cm]. */
  float FrontRightUS;  /**< Front-right ultrasonic distance [cm]. */
  float BackLeftUS;    /**< Rear-left ultrasonic distance [cm]. */
  float BackCenterUS;  /**< Rear-centre ultrasonic distance [cm]. */
  float BackRightUS;   /**< Rear-right ultrasonic distance [cm]. */

  float Speed;   /**< Ground speed [cm/s], integrated from the IMU's accelerometer. */
  float Heading; /**< Compass heading [degrees], 0..360, from the magnetometer. */
  float Pitch;   /**< Pitch angle [degrees], from the fused accelerometer/gyro. */
  float Roll;    /**< Roll angle [degrees], from the fused accelerometer/gyro. */
  float PosX;    /**< Dead-reckoned X position [cm] since startup. */
  float PosY;    /**< Dead-reckoned Y position [cm] since startup. */
  float PosZ;    /**< Dead-reckoned Z position [cm] since startup. */
} HostVehicleState_t;

/**
 * @name ADAS status word (G_u16SystemFlags)
 *
 * @ref G_u16SystemFlags packs the result of all five ADAS modules into one
 * 16-bit word, two bits per module. `SafetyEngine_Task` is its only writer;
 * `Feedback_Task` and `RPi_Comm_Task` are its readers.
 *
 * Each 2-bit field holds @ref SYS_SAFE, @ref SYS_WARNING or @ref SYS_CRITICAL.
 * The layout is:
 *
 * | Bits  | Module |
 * |-------|--------|
 * | 1:0   | FCW — Forward Collision Warning |
 * | 3:2   | EEBL — Emergency Electronic Brake Light |
 * | 5:4   | BSW — Blind Spot Warning (warning level only) |
 * | 7:6   | DNPW — Do Not Pass Warning |
 * | 9:8   | IMA — Intersection Movement Assist |
 * | 15:10 | reserved, always 0 |
 *
 * So `0x0000` means everything is safe, `0x0002` is "FCW critical", and
 * `0x0006` is "FCW critical **and** EEBL warning" at the same time.
 *
 * Read one module's status with @ref SYS_GET, e.g.
 * `SYS_GET(G_u16SystemFlags, SYS_FCW_POS)`.
 * @{
 */
#define SYS_SAFE     0x0u /**< No hazard from this module. */
#define SYS_WARNING  0x1u /**< Caution — the driver should slow down. */
#define SYS_CRITICAL 0x2u /**< Danger — the driver should stop. */
#define SYS_MASK     0x3u /**< Mask of one 2-bit module field. */

#define SYS_FCW_POS  0u /**< Bit position of the FCW field. */
#define SYS_EEBL_POS 2u /**< Bit position of the EEBL field. */
#define SYS_BSW_POS  4u /**< Bit position of the BSW field. */
#define SYS_DNPW_POS 6u /**< Bit position of the DNPW field. */
#define SYS_IMA_POS  8u /**< Bit position of the IMA field. */

/**
 * @brief Extract one module's 2-bit status from the packed word.
 * @param flags The status word, normally @ref G_u16SystemFlags.
 * @param pos   The module's bit position, e.g. @ref SYS_FCW_POS.
 * @return @ref SYS_SAFE, @ref SYS_WARNING or @ref SYS_CRITICAL.
 */
#define SYS_GET(flags, pos) (((flags) >> (pos)) & SYS_MASK)
/** @} */

/**
 * @brief Legacy binary telemetry packet — **kept for reference only**.
 *
 * @deprecated Nothing sends this any more. `RPi_Comm_Task` streams an ASCII CSV
 * line instead, because this packed layout contains `0x00` bytes and those were
 * being lost on the UART link, which silently truncated whole runs.
 *
 * The wire format actually in use is one `\n`-terminated line every 100 ms:
 *
 * @code
 * T,speed,heading,pitch,roll,FL,FC,FR,BL,BC,BR,flags,bsw_sides\n
 * @endcode
 *
 * | Field       | Meaning |
 * |-------------|---------|
 * | `speed`     | cm/s |
 * | `heading`   | degrees, 0..360 |
 * | `pitch`, `roll` | degrees |
 * | `FL`..`BR`  | the six ultrasonic distances [cm] |
 * | `flags`     | @ref G_u16SystemFlags — 2 bits per module |
 * | `bsw_sides` | per-side blind-spot severity: bits 1:0 LEFT, bits 3:2 RIGHT |
 *
 * `bsw_sides` is sent separately because the BSW field inside `flags` is already
 * aggregated to the worst side and no longer says *which* side triggered.
 *
 * @warning The column order here must match the Raspberry Pi parser
 *          (`server.py`, `PACKET_FIELDS`) exactly. Change one side, change the other.
 */
typedef struct __attribute__((packed))
{
  uint8_t start;       /**< Start-of-frame marker, always 0xAA. */
  uint16_t sys_flags;  /**< A copy of @ref G_u16SystemFlags. */
  float speed;         /**< Ground speed [cm/s]. */
  float heading;       /**< Compass heading [degrees], 0..360. */
  float front_left;    /**< Front-left ultrasonic distance [cm]. */
  float front_center;  /**< Front-centre ultrasonic distance [cm]. */
  float front_right;   /**< Front-right ultrasonic distance [cm]. */
  float back_left;     /**< Rear-left ultrasonic distance [cm]. */
  float back_center;   /**< Rear-centre ultrasonic distance [cm]. */
  float back_right;    /**< Rear-right ultrasonic distance [cm]. */
  uint8_t end;         /**< End-of-frame marker, always 0x55. */
} RPi_Packet_t;

/*============================================================================*/
/*                              SHARED STATE                                  */
/*============================================================================*/

/**
 * @brief The packed ADAS status word — see @ref SYS_GET for how to read it.
 *
 * Written only by `SafetyEngine_Task`; read by `Feedback_Task` and
 * `RPi_Comm_Task`. `0x0000` means every module reports safe.
 */
extern volatile uint16_t G_u16SystemFlags;

/**
 * @brief The shared sensor picture of this vehicle.
 *
 * Written by `Sensors_Task`, read by the ADAS modules, the DSRC broadcast and the
 * telemetry task, all under the Data mutex.
 */
extern HostVehicleState_t G_stHostVehicleState;

/**
 * @brief Distance to the nearest intersection [cm]; 0 means "not near one".
 *
 * Read by IMA and broadcast over DSRC so other cars can reason about the same
 * junction. It is set by whatever supplies intersection geometry (the map layer
 * on the Raspberry Pi), not by this firmware.
 */
extern float Host_DistToIntersection;

#include "../Drivers/HAL/LED/LED_interface.h"

/**
 * @name Indicator LEDs
 *
 * The five LEDs, defined in `System.c` and shared across tasks. `Feedback_Task`
 * is the only task that drives them: the interior LED on any alert, the front
 * pair on an FCW-critical, the rear pair on an EEBL-critical.
 * @{
 */
extern LED_Config_t FrontR_LED;   /**< Front-right LED (PC0) — lit on an FCW-critical. */
extern LED_Config_t FrontL_LED;   /**< Front-left LED (PC1) — lit on an FCW-critical. */
extern LED_Config_t BackR_LED;    /**< Rear-right LED (PC2) — lit on an EEBL-critical. */
extern LED_Config_t BackL_LED;    /**< Rear-left LED (PC3) — lit on an EEBL-critical. */
extern LED_Config_t Interior_LED; /**< Interior/dashboard LED (PC7) — lit on any alert. */
/** @} */

/*============================================================================*/
/*                                  SETUP                                     */
/*============================================================================*/

/**
 * @brief Cortex-M4 Data Watchpoint and Trace control register.
 *
 * Enabling the DWT cycle counter through this register is what lets SEGGER
 * SystemView timestamp events; nothing in the ADAS path depends on it.
 */
#define DWT_CTRL *((volatile uint32_t *)0xE0001000)

/**
 * @brief Start SEGGER SystemView tracing (enables the DWT cycle counter first).
 * @note  Debug instrumentation only — the firmware runs fine without it.
 */
void SEGGER_setup(void);

/**
 * @brief Bring up the board: clock tree, then every MCAL and HAL peripheral.
 *
 * Runs before the scheduler starts. Enables the peripheral clocks in RCC,
 * configures the GPIO pins listed in @ref sys_wiring, and initialises the
 * ultrasonics, the IMU, the LEDs, the buzzer, both UARTs and the watchdog.
 */
void System_setup(void);

/**
 * @brief Create the mutexes and the six FreeRTOS tasks, then start the scheduler.
 *
 * @note Does not return: control passes to the scheduler. See @ref sys_tasks for
 *       the priorities and periods it hands out.
 */
void RTOS_setup(void);

/** @} */ /* end of system */

#endif /* SYSTEM_H */
