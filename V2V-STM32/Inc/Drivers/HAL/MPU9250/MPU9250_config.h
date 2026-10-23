/**
 ******************************************************************************
 * @file    MPU9250_config.h
 * @author  Abdallah Saleh
 * @brief   Compile-time tuning for the MPU9250 driver: wiring, ranges, and the
 *          two filters whose constants actually change how the car behaves.
 * @ingroup hal_mpu9250
 ******************************************************************************
 */

#ifndef MPU9250_CONFIG_H_
#define MPU9250_CONFIG_H_

/**
 * @addtogroup hal_mpu9250
 * @{
 */

/**
 * @name Wiring
 * @{
 */
/** @brief The SPI peripheral the IMU hangs off. */
#define MPU9250_SPI_CHANNEL SPI_CHANNEL1

/** @brief Port of the chip-select line. */
#define MPU9250_CS_PORT GPIO_PORTA

/** @brief Pin of the chip-select line. Driven low for the whole of each transfer. */
#define MPU9250_CS_PIN GPIO_PIN4

/**
 * @brief Timer used for the driver's microsecond delays.
 * @note  A basic timer (TIM6 or TIM7) is the right choice — it has no output
 *        pins to waste, and nothing else in the firmware needs one.
 */
#define MPU9250_DELAY_TIMER TIM_TIMER6
/** @} */

/**
 * @name Full-scale ranges
 *
 * A narrower range gives finer resolution but saturates sooner. A road vehicle
 * never pulls multiple g or spins at hundreds of degrees per second, so the
 * narrowest range of each is the right trade.
 * @{
 */
/** @brief Accelerometer range: 0 = ±2 g, 1 = ±4 g, 2 = ±8 g, 3 = ±16 g. */
#define MPU9250_ACCEL_FS 0

/** @brief Gyroscope range: 0 = ±250 °/s, 1 = ±500, 2 = ±1000, 3 = ±2000. */
#define MPU9250_GYRO_FS 0
/** @} */

/**
 * @name Magnetometer hard-iron calibration
 *
 * "Hard iron" is the constant magnetic offset the car itself adds — the motors,
 * the battery, the steel chassis. It shifts the whole field reading off centre,
 * so an uncalibrated compass reads a heading that is simply wrong, and wrong by a
 * different amount in every direction.
 *
 * The fix is to turn the car through a full circle at boot and watch the field
 * trace out a ring; the centre of that ring *is* the offset. That is what these
 * three constants govern.
 * @{
 */
/**
 * @brief Length of the boot-time calibration window [ms].
 * @note  Rotate the car through a full 360° — one or two slow turns — while this
 *        runs. Shorten it for bench testing where the compass does not matter.
 */
#define MPU9250_MAGCAL_DURATION_MS (12000U)

/** @brief How often the magnetometer is sampled inside the calibration window [ms]. */
#define MPU9250_MAGCAL_SAMPLE_MS (20U)

/**
 * @brief Minimum field span required on **both** axes before a calibration is accepted.
 *
 * This is the guard against a calibration that was never actually performed. If
 * the car sat still, the field barely moved, the "ring" is a dot, and its centre
 * is meaningless — so a span below this rejects the calibration outright and
 * leaves the previous offsets in place, rather than freezing in a garbage one.
 */
#define MPU9250_MAG_MIN_SPAN (60.0f)
/** @} */

/**
 * @name Heading fusion (complementary filter)
 *
 * Neither sensor can give the heading alone. The gyroscope is smooth and immune
 * to magnetic noise but **drifts** without bound; the magnetometer has no drift
 * but is noisy and is thrown off by the motors. So the driver integrates the gyro
 * for the short term and lets the magnetometer slowly pull it back to the truth.
 * @{
 */
/**
 * @brief Weight given to the gyroscope on each update, 0..1.
 *
 * The time constant works out at roughly `dt·α/(1−α)`, so at a 50 ms update and
 * α = 0.98 the magnetometer takes about 2.5 s to pull the heading in.
 *
 * Raise it for a smoother heading that shrugs off motor noise but corrects drift
 * more slowly; lower it to trust the compass more and the gyro less.
 */
#define MPU9250_HEADING_ALPHA (0.98f)

/**
 * @brief Sign of the gyroscope's contribution to the fused heading: +1.0f or -1.0f.
 *
 * The gyro's yaw-rate sign must increase in the *same* direction the magnetometer
 * heading increases. If it does not, the two sensors pull against each other and
 * the filter never settles.
 *
 * @note To test: rotate the car steadily one way and watch the fused heading. If
 *       it tracks the raw magnetometer heading, this is right. If it *diverges*
 *       from it, flip this to `(-1.0f)`.
 */
#define MPU9250_GYROZ_SIGN (+1.0f)
/** @} */

/** @} */ /* end of hal_mpu9250 */

#endif /* MPU9250_CONFIG_H_ */
