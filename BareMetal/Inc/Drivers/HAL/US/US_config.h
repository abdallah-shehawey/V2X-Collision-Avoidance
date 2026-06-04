/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<      US_config.h      >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Saleh                                  **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : US (Ultrasonic Distance Sensor)                 **
 **                                                                           **
 **===========================================================================**
 */

#ifndef US_CONFIG_H_
#define US_CONFIG_H_

/*_______________________________________________________________________________*/
/*
 * System Clock Frequency (Hz)
 * Used to calculate timer prescaler for 1us tick.
 * Default: 16MHz (HSI)
 * Change this if you are using a different clock (e.g. 180000000UL for 180MHz PLL)
 */
#define US_SYS_CLK_HZ    16000000UL

/*_______________________________________________________________________________*/
/*
 * Trigger Pulse Duration (microseconds)
 * HC-SR04 requires a minimum 10us HIGH pulse on TRIG pin.
 * Do NOT change this unless using a different ultrasonic module.
 */
#define US_TRIG_PULSE_US    50U

/*_______________________________________________________________________________*/
/*
 * Measurement Timeout (microseconds)
 * Maximum time to wait for ECHO response.
 * HC-SR04 max range ~4m => ~23200us round-trip.
 * Set to 25000us (25ms) as safe timeout.
 */
#define US_TIMEOUT_US       25000U

/*_______________________________________________________________________________*/
/*
 * Task-level Echo Timeout (milliseconds) — interrupt-driven driver.
 * The reading task SLEEPS (vTask/semaphore) up to this long waiting for the
 * IC interrupt to deliver both echo edges. This ALSO caps the effective max
 * range: range_cm ≈ timeout_ms * 1000 / 58.
 *   12ms ≈ 2.0m  → anything farther is reported as out-of-range (clear).
 * Bounds the worst case (all 6 sensors timing out) to ~6*12 = 72ms.
 */
#define US_TASK_TIMEOUT_MS  12U

/*_______________________________________________________________________________*/
/*
 * Sound Speed Factor
 * Distance (cm) = Echo_pulse_us / 58
 * (Speed of sound ~340 m/s => 58 us/cm round-trip)
 */
#define US_SOUND_SPEED_FACTOR    58U

/*_______________________________________________________________________________*/
/*
 * TIMER / CHANNEL / PIN MAPPING TABLE (STM32F446RE Nucleo)
 * -----------------------------------------------------------------------------
 * TIMER    | CHANNEL | REDO_PIN (Echo)     | STATUS / CONFLICTS
 * ---------|---------|---------------------|-----------------------------------
 * TIM1 ADV | CH1     | PA8  (D7)           | FREE (Excellent)
 * [APB2]   | CH3     | PA10 (D2)           | FREE (Excellent)
 *          | CH4     | PA11                | FREE (Excellent)
 * ---------|---------|---------------------|-----------------------------------
 * TIM2 32B | CH2     | PA1  (A1)           | FREE (Best Choice)
 * [APB1]   | CH1/4   | PA0 / PA3           | PA0 (UserBtn), PA3 (Serial RX)
 * ---------|---------|---------------------|-----------------------------------
 * TIM3 16B | CH1     | PA6  (D12) / PB4    | FREE (D12)
 * [APB1]   | CH2     | PA7  (D11) / PB5    | FREE (D11)
 *          | CH3     | PB0                 | FREE
 *          | CH4     | PB1                 | FREE
 * ---------|---------|---------------------|-----------------------------------
 * TIM4 16B | CH1     | PB6  (D10)          | FREE
 * [APB1]   | CH2     | PB7                 | FREE
 *          | CH3/4   | PB8 / PB9           | PB8/9 Conflict with I2C
 * ---------|---------|---------------------|-----------------------------------
 * TIM5 32B | CH2     | PA1  (A1)           | FREE
 * [APB1]   | CH4     | PA3  (A2)           | Conflict with Serial RX (Debug)
 * ---------|---------|---------------------|-----------------------------------
 * TIM8 ADV | CH1     | PC6                 | FREE (Morpho Header)
 * [APB2]   | CH2     | PC7                 | FREE (Morpho Header)
 *          | CH3     | PC8                 | FREE (Morpho Header)
 *          | CH4     | PC9                 | FREE (Morpho Header)
 * -----------------------------------------------------------------------------
 * TRIG PIN: Can be ANY GPIO PIN (Output Mode). No specific hardware required.
 */

#endif /* US_CONFIG_H_ */
