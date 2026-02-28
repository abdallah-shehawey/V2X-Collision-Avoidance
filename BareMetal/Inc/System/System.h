#ifndef SYSTEM_H
#define SYSTEM_H

#include "../Inc/Drivers/HAL/LED/LED_interface.h"
#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"

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
 * | IMU (9-Axis)| SCK, MISO, MOSI       | PA5, PA6, PA7       | SPI1 - AF5|
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
 * | USART1    | PA9(TX), PA10(RX)    | FREE ✅ | RPI Comm             |
 * | UART4     | PA0(TX), PA1(RX)     | FREE ✅ | DSRC Comm            |
 * 
 * ========================================================================================
 */

/* Function Prototypes */
void System_setup(void);
void RTOS_setup(void);

#endif /* SYSTEM_H */