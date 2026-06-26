/**
 * @file MPU9250_config.h
 * @author Abdallah Saleh
 * @brief Configuration file for MPU9250 driver
 */

#ifndef MPU9250_CONFIG_H_
#define MPU9250_CONFIG_H_

/* SPI Channel used for MPU9250 */
#define MPU9250_SPI_CHANNEL    SPI_CHANNEL1

/* GPIO Pins for SPI (should match MCAL config) */
/* Usually handled by GPIO driver, but we define CS pin here */
#define MPU9250_CS_PORT        GPIO_PORTA
#define MPU9250_CS_PIN         GPIO_PIN4

/* Full Scale Ranges */
/* 0: 2g, 1: 4g, 2: 8g, 3: 16g */
#define MPU9250_ACCEL_FS       0 

/* 0: 250dps, 1: 500dps, 2: 1000dps, 3: 2000dps */
#define MPU9250_GYRO_FS        0

/* Timer for delays (TIM_TIMER6 or TIM_TIMER7 recommended) */
#define MPU9250_DELAY_TIMER    TIM_TIMER6

/* ====== Magnetometer hard-iron calibration (boot-time rotation) ====== */
/* Length of the boot calibration window. Rotate the car a full ~360° (one or
 * two slow turns) during it. Lower it for quick bench testing. */
#define MPU9250_MAGCAL_DURATION_MS  (12000U)

/* Magnetometer poll period inside the calibration window (ms). */
#define MPU9250_MAGCAL_SAMPLE_MS    (20U)

/* Minimum field span (raw counts x ASA) required on BOTH axes to ACCEPT the
 * calibration — proves the car actually turned. If the measured span is below
 * this the cal is rejected (offsets left unchanged) and the caller is told. */
#define MPU9250_MAG_MIN_SPAN        (60.0f)

/* ====== Heading fusion (complementary filter: GyroZ + magnetometer) ====== */
/* Gyro weight per update. τ ≈ dt·α/(1−α); at dt≈50ms, 0.98 → ~2.5s mag pull-in.
 * Higher = smoother & more motor-noise-immune, but slower to correct drift. */
#define MPU9250_HEADING_ALPHA       (0.98f)

/* Sign of the GyroZ contribution. The gyro yaw-rate sign MUST match the
 * direction the magnetometer heading increases, or the two fight each other.
 * TEST: rotate the car steadily one way; if the fused heading diverges from the
 * raw magnetometer heading instead of tracking it, flip this to (-1.0f). */
#define MPU9250_GYROZ_SIGN          (+1.0f)

#endif /* MPU9250_CONFIG_H_ */
