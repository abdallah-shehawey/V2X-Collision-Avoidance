/**
 ******************************************************************************
 * @file    MPU9250_private.h
 * @author  Abdallah Saleh
 * @brief   MPU9250 register map, bit values and scale factors — driver-internal.
 * @ingroup hal_mpu9250
 *
 * @details
 * Register addresses from the InvenSense MPU-9250 register map (RM-MPU-9250A-00)
 * and the AK8963 datasheet. Nothing here is part of the driver's public API;
 * include @ref MPU9250_interface.h instead.
 *
 * @section mpu_layout How the two chips fit together
 *
 * An MPU9250 is really *two* dies in one package: the MPU6500 (accelerometer +
 * gyroscope), which the STM32 talks to directly over SPI, and a separate AK8963
 * magnetometer, which hangs off the MPU6500's own **internal I2C master** and is
 * not reachable over SPI at all.
 *
 * So reading the compass is a two-step dance: the driver programs the I2C master
 * registers (@ref I2C_SLV0_ADDR, @ref I2C_SLV0_REG, @ref I2C_SLV0_CTRL) to tell
 * the MPU6500 "go fetch these bytes from the AK8963 for me", and the result then
 * appears in the @ref EXT_SENS_DATA_00 block, which *is* readable over SPI.
 *
 * @note Multi-byte sensor values are big-endian: the `_H` register holds the high
 *       byte and `_L` the low byte, so a reading is `(H << 8) | L`. The AK8963 is
 *       the exception — it is little-endian, `_L` first.
 ******************************************************************************
 */

#ifndef MPU9250_PRIVATE_H_
#define MPU9250_PRIVATE_H_

/**
 * @addtogroup hal_mpu9250
 * @{
 */

/**
 * @name Self-test registers
 * Factory self-test values, used to check the part against its trim values.
 * The driver does not run a self-test, so these are declared but unused.
 * @{
 */
#define SELF_TEST_X_GYRO  0x00 /**< Gyroscope X-axis self-test output. */
#define SELF_TEST_Y_GYRO  0x01 /**< Gyroscope Y-axis self-test output. */
#define SELF_TEST_Z_GYRO  0x02 /**< Gyroscope Z-axis self-test output. */
#define SELF_TEST_X_ACCEL 0x0D /**< Accelerometer X-axis self-test output. */
#define SELF_TEST_Y_ACCEL 0x0E /**< Accelerometer Y-axis self-test output. */
#define SELF_TEST_Z_ACCEL 0x0F /**< Accelerometer Z-axis self-test output. */
/** @} */

/**
 * @name Gyroscope offset registers
 * A signed 16-bit bias subtracted from each gyro axis in hardware.
 * @{
 */
#define XG_OFFSET_H 0x13 /**< Gyroscope X-axis offset, high byte. */
#define XG_OFFSET_L 0x14 /**< Gyroscope X-axis offset, low byte. */
#define YG_OFFSET_H 0x15 /**< Gyroscope Y-axis offset, high byte. */
#define YG_OFFSET_L 0x16 /**< Gyroscope Y-axis offset, low byte. */
#define ZG_OFFSET_H 0x17 /**< Gyroscope Z-axis offset, high byte. */
#define ZG_OFFSET_L 0x18 /**< Gyroscope Z-axis offset, low byte. */
/** @} */

/**
 * @name Configuration registers
 * @{
 */
#define SMPLRT_DIV    0x19 /**< Sample-rate divider: output rate = internal rate / (1 + this). */
#define CONFIG        0x1A /**< Low-pass filter bandwidth and FIFO overflow behaviour. */
#define GYRO_CONFIG   0x1B /**< Gyroscope full-scale range (±250/500/1000/2000 °/s) and self-test enables. */
#define ACCEL_CONFIG  0x1C /**< Accelerometer full-scale range (±2/4/8/16 g) and self-test enables. */
#define ACCEL_CONFIG2 0x1D /**< Accelerometer low-pass filter bandwidth. */
#define LP_ACCEL_ODR  0x1E /**< Output data rate while in low-power accelerometer mode. */
#define WOM_THR       0x1F /**< Wake-on-motion threshold. */
#define FIFO_EN       0x23 /**< Which sensors are written into the FIFO (the driver polls instead, so this stays 0). */
/** @} */

/**
 * @name Internal I2C master registers
 *
 * These drive the MPU6500's own I2C master, which is the *only* path to the
 * AK8963 magnetometer — see @ref mpu_layout.
 * @{
 */
#define I2C_MST_CTRL     0x24 /**< I2C master clock speed and multi-master settings. */
#define I2C_SLV0_ADDR    0x25 /**< Slave 0: the I2C address to talk to, plus the read/write bit. */
#define I2C_SLV0_REG     0x26 /**< Slave 0: which register inside that slave to access. */
#define I2C_SLV0_CTRL    0x27 /**< Slave 0: enable, and how many bytes to transfer. */
#define I2C_SLV1_ADDR    0x28 /**< Slave 1: I2C address plus the read/write bit. */
#define I2C_SLV1_REG     0x29 /**< Slave 1: register inside that slave. */
#define I2C_SLV1_CTRL    0x2A /**< Slave 1: enable and transfer length. */
#define I2C_SLV0_DO      0x63 /**< Slave 0: the byte to write, when slave 0 is set up as a write. */
#define EXT_SENS_DATA_00 0x49 /**< First of the 24 bytes where the I2C master parks whatever it fetched — this is where the compass reading lands. */
#define I2C_MST_STATUS   0x36 /**< I2C master status: transfer done, and the per-slave NACK flags. */
/** @} */

/**
 * @name Interrupt registers
 * @{
 */
#define INT_PIN_CFG 0x37 /**< INT pin behaviour: polarity, latching, and the I2C bypass enable. */
#define INT_ENABLE  0x38 /**< Which conditions raise the INT pin (data-ready, FIFO overflow, wake-on-motion). */
#define INT_STATUS  0x3A /**< Which condition actually fired; reading it clears the flags. */
/** @} */

/**
 * @name Sensor data registers
 *
 * Big-endian: a reading is `(H << 8) | L`, and the pair must be read as one
 * burst so the two halves come from the same sample.
 * @{
 */
#define ACCEL_XOUT_H 0x3B /**< Accelerometer X, high byte. */
#define ACCEL_XOUT_L 0x3C /**< Accelerometer X, low byte. */
#define ACCEL_YOUT_H 0x3D /**< Accelerometer Y, high byte. */
#define ACCEL_YOUT_L 0x3E /**< Accelerometer Y, low byte. */
#define ACCEL_ZOUT_H 0x3F /**< Accelerometer Z, high byte. */
#define ACCEL_ZOUT_L 0x40 /**< Accelerometer Z, low byte. */
#define TEMP_OUT_H   0x41 /**< Die temperature, high byte. */
#define TEMP_OUT_L   0x42 /**< Die temperature, low byte. */
#define GYRO_XOUT_H  0x43 /**< Gyroscope X, high byte. */
#define GYRO_XOUT_L  0x44 /**< Gyroscope X, low byte. */
#define GYRO_YOUT_H  0x45 /**< Gyroscope Y, high byte. */
#define GYRO_YOUT_L  0x46 /**< Gyroscope Y, low byte. */
#define GYRO_ZOUT_H  0x47 /**< Gyroscope Z, high byte. */
#define GYRO_ZOUT_L  0x48 /**< Gyroscope Z, low byte. */
/** @} */

/**
 * @name AK8963 magnetometer registers
 *
 * Reached only through the internal I2C master — see @ref mpu_layout. Unlike the
 * MPU6500 registers above, the AK8963's data registers are **little-endian**:
 * the `_L` byte comes first.
 * @{
 */
#define AK8963_ADDR     0x0C /**< The AK8963's address on the MPU6500's internal I2C bus. */
#define AK8963_WHO_AM_I 0x00 /**< Device ID; reads back 0x48 on a healthy part. */
#define AK8963_ST1      0x02 /**< Status 1: bit 0 is DRDY, "a fresh sample is ready". */
#define AK8963_XOUT_L   0x03 /**< Magnetometer X, low byte. */
#define AK8963_XOUT_H   0x04 /**< Magnetometer X, high byte. */
#define AK8963_YOUT_L   0x05 /**< Magnetometer Y, low byte. */
#define AK8963_YOUT_H   0x06 /**< Magnetometer Y, high byte. */
#define AK8963_ZOUT_L   0x07 /**< Magnetometer Z, low byte. */
#define AK8963_ZOUT_H   0x08 /**< Magnetometer Z, high byte. */
#define AK8963_ST2      0x09 /**< Status 2: overflow flag. Reading it *ends* the measurement — it must be read after the data, or the next sample never arrives. */
#define AK8963_CNTL     0x0A /**< Control 1: operating mode (power-down, single measurement, continuous) and output bit width. */
#define AK8963_ASAX     0x10 /**< Factory sensitivity-adjustment value, X axis. */
#define AK8963_ASAY     0x11 /**< Factory sensitivity-adjustment value, Y axis. */
#define AK8963_ASAZ     0x12 /**< Factory sensitivity-adjustment value, Z axis. */
/** @} */

/**
 * @name Power and identity registers
 * @{
 */
#define USER_CTRL  0x6A /**< Enables the FIFO and the internal I2C master, and disables the I2C slave interface. */
#define PWR_MGMT_1 0x6B /**< Power management 1: device reset, sleep, and the clock source. */
#define PWR_MGMT_2 0x6C /**< Power management 2: per-axis standby for the accelerometer and gyroscope. */
#define WHO_AM_I   0x75 /**< Device ID; reads back 0x71 on a genuine MPU9250. */
/** @} */

/**
 * @name Register bit values
 * @{
 */
#define PWR_RESET     0x80 /**< @ref PWR_MGMT_1 — trigger a full device reset. */
#define CLOCK_SEL_PLL 0x01 /**< @ref PWR_MGMT_1 — use the gyro-referenced PLL, which is far more stable than the internal oscillator. */
#define I2C_IF_DIS    0x10 /**< @ref USER_CTRL — disable the I2C slave interface, so the part is SPI-only. Required on this board. */
#define I2C_MST_EN    0x20 /**< @ref USER_CTRL — enable the internal I2C master, without which the magnetometer is unreachable. */
/** @} */

/**
 * @name Scale factors
 *
 * Raw counts to physical units, for the ranges this driver configures.
 * @{
 */
#define ACCEL_SENS_2G 16384.0f /**< LSB per g at the ±2 g range: acceleration [g] = raw / this. */
#define GYRO_SENS_250 131.0f   /**< LSB per °/s at the ±250 °/s range: rate [°/s] = raw / this. */
/** @} */

/** @} */ /* end of hal_mpu9250 */

#endif /* MPU9250_PRIVATE_H_ */
