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
 * Settle time before the trigger (microseconds)
 * TRIG is driven LOW for this long before the HIGH pulse to guarantee a clean
 * edge (no glitch from a previous measurement).
 */
#define US_TRIG_SETTLE_US   2U

/*_______________________________________________________________________________*/
/*
 * Trigger Pulse Duration (microseconds)
 * HC-SR04 requires a minimum 10us HIGH pulse on TRIG pin (datasheet).
 * Do NOT lower this below 10us.
 */
#define US_TRIG_PULSE_US    10U

/*_______________________________________________________________________________*/
/*
 * Maximum range (centimeters).
 * HC-SR04 useful range is ~2..400 cm. Any echo that decodes to a larger
 * distance is treated as out-of-range (object lost) and reported as a timeout.
 * This is the SINGLE source of truth for "max range":
 *   - it clamps the decoded distance in the IC ISR, and
 *   - it derives the task-level echo timeout below.
 */
#define US_MAX_RANGE_CM     400U

/*_______________________________________________________________________________*/
/*
 * Sound Speed Factor
 * Distance (cm) = Echo_pulse_us / 58
 * (Speed of sound ~343 m/s => ~58 us/cm round-trip)
 */
#define US_SOUND_SPEED_FACTOR    58U

/*_______________________________________________________________________________*/
/*
 * Task-level Echo Timeout (milliseconds) — interrupt-driven driver.
 * The reading task SLEEPS (vTask/semaphore) up to this long waiting for the
 * IC interrupt to deliver both echo edges. It is DERIVED from US_MAX_RANGE_CM so
 * the timeout and the range clamp can never drift apart:
 *   timeout_ms = ceil(MAX_RANGE_CM * 58us/cm / 1000) + 1ms slack
 *              = ceil(400 * 58 / 1000) + 1 = 24 + 1 = 25ms  → full 4m range.
 * Worst case (all 6 sensors out of range) ≈ 6 * 25 = 150ms per scan.
 */
#define US_TASK_TIMEOUT_MS  \
    ((((US_MAX_RANGE_CM * US_SOUND_SPEED_FACTOR) + 999U) / 1000U) + 1U)

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
