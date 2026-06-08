
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
 * | IMU (9-Axis)| SCK, MISO(ADO), MOSI(SDA)       | PA5, PA6, PA7       | SPI1 - AF5|
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
 * 4. MOTORS (L298N Driver)
 * ----------------------------------------------------------------------------------------
 * | Component | Pin  | Port  | Description                 | Note                    |
 * |-----------|------|-------|-----------------------------|-------------------------|
 * | ENA       | PA8  | PORTA | PWM Speed Control (Motor A) | Optional (Tie to 5V)    |
 * | IN1       | PC5  | PORTC | Motor A Direction 1         | Right Wheels Forward    |
 * | IN2       | PC6  | PORTC | Motor A Direction 2         | Right Wheels Backward   |
 * | IN3       | PB10 | PORTB | Motor B Direction 1         | Left Wheels Forward     |
 * | IN4       | PB15 | PORTB | Motor B Direction 2         | Left Wheels Backward    |
 * | ENB       | PA11 | PORTA | PWM Speed Control (Motor B) | Optional (Tie to 5V)    |
 *
 * 5. COMMUNICATION INTERFACES (UARTs)
 * ----------------------------------------------------------------------------------------
 * | Interface | Pin Mapping          | Status  | Description          |
 * |-----------|----------------------|---------|----------------------|
 * | USART2    | PA2(TX), PA3(RX)     | FREE ✅ | Debug / VCP          |
 * | USART1    | PA9(TX), PA10(RX)    | IN USE  | ESP-NOW (V2X Comm)   |
 * | UART4     | PA0(TX), PA1(RX)     | IN USE  | Raspberry Pi Comm    |
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
 * | [3] vTask_Feedback     | 2        | +128          | ~25 ms             | Centralized actuator manager (Motors+LEDs+BUZ)|
 * | [4] vTask_RPi_Comm     | 1 (LOW)  | +100          | ~100 ms            | Raspberry Pi communication (RX + TX)          |
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
 *           neighbor table in ONE pass; each module raises its own flag. Then it
 *           publishes the GENERAL bitmap G_u8SystemFlags (one bit per active module).
 *           It makes NO movement decision. Holds both mutexes (NeighborTable → Data).
 *         • vTask_Feedback (Muscle): reads G_u8SystemFlags; if 0 drives forward with
 *           everything off, else buzzer + interior LED ON (general driver alert) and
 *           inspects per-module getters for external indicators (FCW → front LEDs,
 *           EEBL/BSW → back LEDs) and motor (FCW CRITICAL → stop). Sole driver of the
 *           actuators and sole writer of G_eMotorGlobalCommand. Takes G_xDataMutex only.
 *       Lock usage: ESP_Comm takes the two mutexes separately, Sensors & Feedback take
 *       Data only → deadlock-free.
 * ========================================================================================
 */


/* Unified Sensor State Structure */
typedef struct {
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

/* ── System module flags (bitmap) ──
 * Each bit = one ADAS module has an active alert.
 * SafetyEngine writes, vTask_Feedback reads.
 * 0x00 = all safe. */
#define SYSFLG_FCW   (1U << 0)   /* Forward Collision Warning        */
#define SYSFLG_EEBL  (1U << 1)   /* Emergency Electronic Brake Light */
#define SYSFLG_BSW   (1U << 2)   /* Blind Spot Warning               */
#define SYSFLG_DNPW  (1U << 3)   /* Do Not Pass Warning              */
#define SYSFLG_IMA   (1U << 4)   /* Intersection Movement Assist     */

/* ── RPi telemetry packet ──
 * Sent every 100ms via UART4.
 * Protocol: START(0xAA) | sys_flags | speed_f32 | heading_f32 | 6×US_f32 | END(0x55) */
typedef struct __attribute__((packed))
{
  uint8_t  start;        /* 0xAA */
  uint8_t  sys_flags;    /* SYSFLG_* bitmap */
  float    speed;        /* cm/s */
  float    heading;      /* degrees 0-360 */
  float    front_left;   /* ultrasonic distance [cm] */
  float    front_center;
  float    front_right;
  float    back_left;
  float    back_center;
  float    back_right;
  uint8_t  end;          /* 0x55 */
} RPi_Packet_t;

/* Global variables for centralized management */

extern volatile uint8_t        G_u8SystemFlags; /* bitmap: 0 = all safe */

/* Unified Host Vehicle State */
extern HostVehicleState_t G_stHostVehicleState;

/* Hardware objects — defined in System.c, used across tasks */
#include "../Drivers/HAL/LED/LED_interface.h"
extern LED_Config_t FrontR_LED, FrontL_LED, BackR_LED, BackL_LED, Interior_LED;


/* Function Prototypes */
#define DWT_CTRL            *((volatile uint32_t*)0xE0001000)

void SEGGER_setup(void);

void System_setup(void);
void RTOS_setup(void);

#endif /* SYSTEM_H */
