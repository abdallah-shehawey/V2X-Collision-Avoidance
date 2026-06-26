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

/* ====== Magnetometer hard-iron auto-calibration (continuous) ====== */
/* Minimum field span (raw counts x ASA) required on BOTH axes before the
 * running min/max is trusted to compute the hard-iron offset. Until the car
 * has turned enough to exceed this, heading falls back to the raw (uncorrected)
 * reading. Raise it if a noisy/near-stationary span yields a bad offset. */
#define MPU9250_MAG_MIN_SPAN   (60.0f)

/* Per-sample inward relaxation of the running extremes (raw counts). Lets a
 * one-off spike decay out over time and keeps the calibration "live" instead
 * of latching forever. Keep small so real turning refreshes faster than decay. */
#define MPU9250_MAG_DECAY      (0.02f)

#endif /* MPU9250_CONFIG_H_ */
