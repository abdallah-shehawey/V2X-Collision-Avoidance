
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
 * | IN4       | PB11 | PORTB | Motor B Direction 2         | Left Wheels Backward    |
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
 *           publishes the GENERAL flag G_u8SystemRiskLevel (= worst confirmed alert).
 *           It makes NO movement decision. Holds both mutexes (NeighborTable → Data).
  *         • vTask_Feedback (Muscle): reads G_u8SystemRiskLevel; if 0 drives forward,
 *           else inspects per-module getters (e.g. FCW_u8GetAlertLevel). WARNING →
 *           front LEDs + buzzer; CRITICAL → also reverse if the rear is clear (min of
 *           the 3 back US >= threshold) else stop. Sole driver of the actuators and
 *           sole writer of G_eMotorGlobalCommand. Takes G_xDataMutex only (to read US).
 *       Lock usage: ESP_Comm takes the two mutexes separately, Sensors & Feedback take
 *       Data only → deadlock-free.
 * ========================================================================================
 */

/* ================== Global Intentions ================== */
typedef enum {
    CMD_MOVE_FORWARD = 0,
    CMD_STOP = 1,
    CMD_STEER_RIGHT = 2,
    CMD_STEER_LEFT = 3,
    CMD_MOVE_BACKWARD = 4
} MotorCommand_t;

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
    float DistToIntersection;
} HostVehicleState_t;

/* Global variables for centralized management */
extern volatile MotorCommand_t G_eMotorGlobalCommand;
extern volatile uint8_t G_u8SystemRiskLevel; /* 0: Safe, 1: Warning, 2: Critical */

/* Unified Host Vehicle State */
extern HostVehicleState_t G_stHostVehicleState;


/* Function Prototypes */
#define DWT_CTRL            *((volatile uint32_t*)0xE0001000)

void SEGGER_setup(void);

void System_setup(void);
void RTOS_setup(void);

#endif /* SYSTEM_H */
