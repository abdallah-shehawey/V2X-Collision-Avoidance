
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
 * | LED 1     | PC0 | PORTC | Status Color 1   |
 * | LED 2     | PC1 | PORTC | Status Color 2   |
 * | LED 3     | PC2 | PORTC | Status Color 3   |
 * | LED 4     | PC3 | PORTC | Status Color 4   |
 * | BUZZER    | PC4 | PORTC | Warning Sound    |
 *
 * 4. COMMUNICATION INTERFACES (UARTs)
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
 * | Task Name            | Priority | Freq/Trigger | Description                                  |
 * |----------------------|----------|--------------|----------------------------------------------|
 * | [1] vTask_ESP_RX     | 4 (MAX)  | Event/ISR    | Reads incoming V2V/V2I msgs from ESP-NOW     |
 * | [2] vTask_Sensors    | 3        | ~20-50 ms    | Updates MPU9250 speed & US Distances         |
 * | [3] vTask_ADAS_Core  | 2        | ~50 ms       | Runs FCW, EEBL, SDW, BSW, DNPW logic         |
 * | [4] vTask_RPi_RX     | 2        | Event/100 ms | Receives settings/commands from Raspberry Pi |
 * | [5] vTask_ESP_TX     | 1        | ~100 ms      | Broadcasts host state/warnings to ESP-NOW    |
 * | [6] vTask_RPi_TX     | 1        | ~200 ms      | Sends display data/warnings to Raspberry Pi  |
 * | [7] vTask_Feedback   | 1        | Queue driven | Handles User Interface (LEDs, Buzzer)        |
 *
 * NOTE: Priority 4 is the highest, 0 is the lowest (Idle task).
 * ========================================================================================
 */

/* ================== V2X Data Frame ================== */
typedef struct __attribute__((packed)) {
    uint8_t  Sender_ID;
    uint8_t  Target_ID;
    float    Speed_ms;
    float    Heading_deg;
    float    Position_Z;
    uint8_t  Vehicle_State; /* 0: Normal, 1: EEBL Active */
} V2X_Message_t;

/* Function Prototypes */
#define DWT_CTRL            *((volatile uint32_t*)0xE0001000)

void SEGGER_setup(void);

void System_setup(void);
void RTOS_setup(void);

#endif /* SYSTEM_H */
