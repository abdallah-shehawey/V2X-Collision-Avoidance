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

#endif /* MPU9250_CONFIG_H_ */
