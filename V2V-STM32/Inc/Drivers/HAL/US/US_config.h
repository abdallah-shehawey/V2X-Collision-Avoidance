/**
 ******************************************************************************
 * @file    US_config.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Compile-time configuration for the US driver — the HC-SR04 ultrasonic rangefinders.
 * @ingroup hal_us
 ******************************************************************************
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
#define US_SYS_CLK_HZ 16000000UL /**< System clock the echo timing is derived from [Hz]. If this is wrong, every distance is scaled by the same factor. */
/*_______________________________________________________________________________*/
/*
 * Settle time before the trigger (microseconds)
 * TRIG is driven LOW for this long before the HIGH pulse to guarantee a clean
 * edge (no glitch from a previous measurement).
 */
#define US_TRIG_SETTLE_US 2U /**< How long TRIG is held low before the pulse [us], to guarantee a clean edge with no glitch left over from the previous measurement. */
/*_______________________________________________________________________________*/
/*
 * Trigger Pulse Duration (microseconds)
 * HC-SR04 requires a minimum 10us HIGH pulse on TRIG pin (datasheet).
 * Do NOT lower this below 10us.
 */
#define US_TRIG_PULSE_US 10U /**< Width of the TRIG pulse [us]. The HC-SR04 datasheet requires **at least** 10 us — do not lower this. */
/*_______________________________________________________________________________*/
/*
 * Maximum range (centimeters).
 * HC-SR04 useful range is ~2..400 cm. Any echo that decodes to a larger
 * distance is treated as out-of-range (object lost) and reported as a timeout.
 * This is the SINGLE source of truth for "max range":
 *   - it clamps the decoded distance in the IC ISR, and
 *   - it derives the task-level echo timeout below.
 */
#define US_MAX_RANGE_CM 400U /**< Range ceiling [cm]. The single source of truth for "out of range": it clamps the distance in the capture ISR *and* derives the task-level echo timeout. An echo decoding past it is reported as a timeout (object lost), not as a huge distance. */
/*_______________________________________________________________________________*/
/*
 * Sound Speed Factor
 * Distance (cm) = Echo_pulse_us / 58
 * (Speed of sound ~343 m/s => ~58 us/cm round-trip)
 */
#define US_SOUND_SPEED_FACTOR 58U /**< Microseconds of echo per centimetre of distance. Sound travels ~343 m/s, and the pulse makes a round trip, so distance_cm = echo_us / 58. */
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
/**
 * @brief How long the reading task sleeps waiting for one echo before giving up [ms].
 *
 * Derived from @ref US_MAX_RANGE_CM rather than hard-coded, so raising the range
 * ceiling automatically gives the echo the extra flight time it now needs. Worst
 * case, with all six sensors out of range, a full scan takes about 6 x 25 = 150 ms.
 */
#define US_TASK_TIMEOUT_MS \
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
