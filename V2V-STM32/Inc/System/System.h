
/**
 ******************************************************************************
 * @file           : System.h
 * @author         : Abdallah Saleh
 * @brief          : System Header file
 ******************************************************************************
 **/

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

/*
 * ========================================================================================
 * 🚗 V2X HARDWARE WIRING MAPPING
 * ========================================================================================
 *
 * 1. ULTRASONIC SENSORS (3 Front / 3 Back)
 * ----------------------------------------------------------------------------------------
 * | ID | Location       | Echo Pin | Trig Pin | Timer Channel | Note                     |
 * |----|----------------|----------|----------|---------------|--------------------------|
 * | S1 | FRONT LEFT     | PA15     | PB0      | TIM2_CH1      | UART-Friendly (Free PA0) |
 * | S2 | FRONT CENTER   | PB3      | PB1      | TIM2_CH2      | UART-Friendly (Free PA1) |
 * | S3 | FRONT RIGHT    | PB4      | PB2      | TIM3_CH1      | UART-Friendly (Free PA2) |
 * | S4 | BACK  LEFT     | PB5      | PB12     | TIM3_CH2      | UART-Friendly (Free PA3) |
 * | S5 | BACK  CENTER   | PC8      | PB13     | TIM3_CH3      |                          |
 * | S6 | BACK  RIGHT    | PC9      | PB14     | TIM3_CH4      |                          |
 *
 * 2. MOTION SENSOR (MPU9250)
 * ----------------------------------------------------------------------------------------
 * | Sensor Type | SPI Signals           | Pins                | Mode / AF |
 * |-------------|-----------------------|---------------------|-----------|
 * | IMU (9-Axis)| SCK, MISذO(ADO), MOSI(SDA)       | PA5, PA6, PA7       | SPI1 - AF5|
 *
 * 3. FEEDBACK SYSTEM
 * ----------------------------------------------------------------------------------------
 * | Component | Pin | Port  | Description      |
 * |-----------|-----|-------|------------------|
 * | LED 1     | PC0 | PORTC | Status Color 1   | Front Right
 * | LED 2     | PC1 | PORTC | Status Color 2   | Front Left
 * | LED 3     | PC2 | PORTC | Status Color 3   | Back Right
 * | LED 4     | PC3 | PORTC | Status Color 4   | Back Left
 * | LED 5     | PC7 | PORTC | Interior Driver Alert (dashboard) |
 * | BUZZER    | PC4 | PORTC | Warning Sound    |
 *
 * NOTE: There is NO motor driver on the STM32 — the Raspberry Pi drives the
 *       motors using the telemetry this firmware sends. No motor pins are used.
 *
 * 4. COMMUNICATION INTERFACES (UARTs)
 * ----------------------------------------------------------------------------------------
 * | Interface | Pin Mapping          | Status  | Description                 |
 * |-----------|----------------------|---------|-----------------------------|
 * | USART1    | PA9(TX), PA10(RX)    | IN USE  | ESP-NOW (V2X Comm)          |
 * | USART2    | PA2(TX), PA3(RX)     | IN USE  | Raspberry Pi telemetry (USB VCP) |
 * | UART4     | PA0(TX), PA1(RX)     | FREE ✅ | (was RPi; now on USART2)    |
 *
 * ========================================================================================
 */

/*
 * ========================================================================================
 * 🧠 RTOS TASKS ARCHITECTURE & PRIORITIES (configMAX_PRIORITIES = 5)
 * ========================================================================================
 *
 * | Task Name              | Priority | Stack (+base) | Freq               | Description                                   |
 * |------------------------|----------|---------------|--------------------|-----------------------------------------------|
 * | [1] vTask_SafetyEngine | 4 (MAX)  | +256          | ~50 ms             | Single-pass ADAS brain (FCW/EEBL/BSW/DNPW/IMA)|
 * |                        |          |               |                    | + feedback aggregation → command channel      |
 * | [1] vTask_ESP_Comm     | 4 (MAX)  | +128          | ~10ms RX/100ms TX  | ESP-NOW V2X communication                     |
 * | [2] vTask_Sensors      | 3        | +256          | ~25-82 ms adaptive | All 6 US (interrupt, 2m cap) + MPU9250         |
 * | [3] vTask_Feedback     | 2        | +128          | ~25 ms             | Alert manager (LEDs + Buzzer; motors on RPi)  |
 * | [4] vTask_RPi_Comm     | 1 (LOW)  | +100          | ~100 ms            | Raspberry Pi telemetry TX (status + sensors)  |
 *
 * NOTE: Priority 4 is the highest user priority. Priority 0 is the FreeRTOS Idle task.
 *       configMAX_SYSCALL_INTERRUPT_PRIORITY = 5  → NVIC_USART1 must be set to ≥ 6.
 *       vTask_Sensors reads all 6 US per cycle (interrupt-driven: the task SLEEPS
 *       during each echo, CPU free) + MPU, then a 10ms gap. Scan is adaptive:
 *       near objects → ~25ms, all out-of-range → ~72ms (2m cap). Reads are
 *       sequential → no acoustic cross-talk. Priority 3 (producer) sits below the
 *       priority-4 ADAS/comm tasks but above the priority-2 Feedback consumer.
 *
 *       ADAS architecture = SINGLE-PASS, with a clean Brain/Muscle split:
 *         • vTask_SafetyEngine (Brain, detection-only): runs all modules over the
 *           neighbor table in ONE pass; each module reports its risk level. Then it
 *           publishes G_u16SystemFlags (2 bits/module: 00 safe / 01 warning / 10 crit).
 *           It makes NO actuator decision. Holds both mutexes (NeighborTable → Data).
 *         • vTask_Feedback (Muscle): reads G_u16SystemFlags; if 0 → everything off.
 *           Any alert → interior LED + buzzer ON. Additionally: FCW CRITICAL → front
 *           LEDs, EEBL CRITICAL → rear LEDs. All other states add no external LED.
 *           NO motor control — the Raspberry Pi drives the motors from the telemetry.
 *       Lock usage: ESP_Comm takes the two mutexes separately, Sensors & Feedback take
 *       Data only → deadlock-free.
 * ========================================================================================
 */

/* Unified Sensor State Structure */
typedef struct
{
  float FrontLeftUS;
  float FrontCenterUS;
  float FrontRightUS;
  float BackLeftUS;
  float BackCenterUS;
  float BackRightUS;

  float Speed;
  float Heading;
  float Pitch;
  float Roll;
  float PosX;
  float PosY;
  float PosZ;
} HostVehicleState_t;

/* ════════════════════════════════════════════════════════════════════════
 *  G_u16SystemFlags — system status word (2 bits per ADAS module)
 * ════════════════════════════════════════════════════════════════════════
 *  SafetyEngine writes it; vTask_Feedback and vTask_RPi_Comm read it.
 *  Each module occupies 2 bits:
 *        0b00 = SAFE      (no hazard)
 *        0b01 = WARNING   (caution — slow down)
 *        0b10 = CRITICAL  (danger — stop)
 *
 *  Bit layout (uint16_t):
 *        bits  1:0  → FCW   (Forward Collision Warning)
 *        bits  3:2  → EEBL  (Emergency Electronic Brake Light)
 *        bits  5:4  → BSW   (Blind Spot Warning)        [WARNING only]
 *        bits  7:6  → DNPW  (Do Not Pass Warning)
 *        bits  9:8  → IMA   (Intersection Movement Assist)
 *        bits 15:10 → reserved (0)
 *
 *  0x0000 = everything safe.
 *  Extract one module:  status = (G_u16SystemFlags >> SYS_xxx_POS) & SYS_MASK
 *
 *  Worked examples (binary grouped as IMA|DNPW|BSW|EEBL|FCW):
 *     0x0001  00 00 00 00 01  → FCW  WARNING
 *     0x0002  00 00 00 00 10  → FCW  CRITICAL
 *     0x0004  00 00 00 01 00  → EEBL WARNING
 *     0x0010  00 00 01 00 00  → BSW  WARNING
 *     0x0040  00 01 00 00 00  → DNPW WARNING
 *     0x0100  01 00 00 00 00  → IMA  WARNING
 *     0x0006  00 00 00 01 10  → FCW CRITICAL + EEBL WARNING
 *     0x0101  01 00 00 00 01  → FCW WARNING  + IMA WARNING
 * ════════════════════════════════════════════════════════════════════════ */
#define SYS_SAFE 0x0u
#define SYS_WARNING 0x1u
#define SYS_CRITICAL 0x2u
#define SYS_MASK 0x3u /* 2-bit field mask */

#define SYS_FCW_POS 0u
#define SYS_EEBL_POS 2u
#define SYS_BSW_POS 4u
#define SYS_DNPW_POS 6u
#define SYS_IMA_POS 8u

/* status (0/1/2) of one module from the packed word */
#define SYS_GET(flags, pos) (((flags) >> (pos)) & SYS_MASK)

/* ── RPi telemetry packet ──
 * Sent every 100ms via UART4.
 * Protocol: START(0xAA) | sys_flags | speed_f32 | heading_f32 | 6×US_f32 | END(0x55) */
typedef struct __attribute__((packed))
{
  uint8_t start;      /* 0xAA */
  uint16_t sys_flags; /* G_u16SystemFlags: 2 bits/module (00/01/10) */
  float speed;        /* cm/s */
  float heading;      /* degrees 0-360 */
  float front_left;   /* ultrasonic distance [cm] */
  float front_center;
  float front_right;
  float back_left;
  float back_center;
  float back_right;
  uint8_t end; /* 0x55 */
} RPi_Packet_t;

/* Global variables for centralized management */

extern volatile uint16_t G_u16SystemFlags; /* 2 bits/module; 0 = all safe */

/* Unified Host Vehicle State */
extern HostVehicleState_t G_stHostVehicleState;

/* Distance to nearest intersection (cm), 0 = not near. Read by IMA and broadcast
 * over DSRC. Set by whatever provides intersection geometry (map/RPi). */
extern float Host_DistToIntersection;

/* Hardware objects — defined in System.c, used across tasks */
#include "../Drivers/HAL/LED/LED_interface.h"
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED, Interior_LED;

/* Function Prototypes */
#define DWT_CTRL *((volatile uint32_t *)0xE0001000)

void SEGGER_setup(void);

void System_setup(void);
void RTOS_setup(void);

#endif /* SYSTEM_H */
