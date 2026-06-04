/**
 * @file MPU9250_interface.h
 * @author Abdallah Saleh
 * @brief Header file for MPU9250 IMU driver
 * @version 1.0
 * @date 2026-02-24
 */

#ifndef MPU9250_INTERFACE_H_
#define MPU9250_INTERFACE_H_

#include "../../LIB/ErrTypes.h"

/**
 * @struct MPU9250_Data_t
 * @brief Structure to hold processed 9-axis sensor data and temperature.
 * @details Data is converted from raw bits to physical units (g, dps, uT, Celsius).
 */
typedef struct {
    float AccelX;      /**< Acceleration in X-axis [g] */
    float AccelY;      /**< Acceleration in Y-axis [g] */
    float AccelZ;      /**< Acceleration in Z-axis [g] */
    float GyroX;       /**< Angular velocity in X-axis [dps] */
    float GyroY;       /**< Angular velocity in Y-axis [dps] */
    float GyroZ;       /**< Angular velocity in Z-axis [dps] */
    float MagX;        /**< Magnetic field in X-axis [uT] */
    float MagY;        /**< Magnetic field in Y-axis [uT] */
    float MagZ;        /**< Magnetic field in Z-axis [uT] */
    float Temperature; /**< Chip temperature [Celsius] */
} MPU9250_Data_t;

/**
 * @struct MPU9250_Position_t
 * @brief Structure to hold 3D spatial coordinates.
 */
typedef struct {
    float X; /**< Current X position [meters] */
    float Y; /**< Current Y position [meters] */
    float Z; /**< Current Z position [meters] */
} MPU9250_Position_t;

/**
 * @fn ErrorState_t MPU9250_enumInit(void)
 * @brief Initializes the MPU9250 sensor and its internal I2C master.
 */
ErrorState_t MPU9250_enumInit(void);

/**
 * @fn ErrorState_t MPU9250_enumReadData(MPU9250_Data_t *Copy_pData)
 * @brief Reads all sensor data (Accel, Gyro, Mag, Temp) in a single burst.
 */
ErrorState_t MPU9250_enumReadData(MPU9250_Data_t *Copy_pData);

/**
 * @fn ErrorState_t MPU9250_enumGetHeading(MPU9250_Data_t *Copy_pData, float *Copy_pfHeading)
 * @brief Calculates the 2D heading from magnetometer data.
 */
ErrorState_t MPU9250_enumGetHeading(MPU9250_Data_t *Copy_pData, float *Copy_pfHeading);

/**
 * @fn ErrorState_t MPU9250_enumGetSpeed(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfSpeed)
 * @brief Estimates forward speed with gravity compensation.
 */
ErrorState_t MPU9250_enumGetSpeed(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfSpeed);

/**
 * @fn ErrorState_t MPU9250_enumGetPosition(MPU9250_Data_t *Copy_pData, float Copy_fSpeed, float Copy_fHeading, float Copy_fPitch, float Copy_fDt, MPU9250_Position_t *Copy_pPos)
 * @brief Updates position using Dead Reckoning and Gravity Compensation.
 */
ErrorState_t MPU9250_enumGetPosition(MPU9250_Data_t *Copy_pData, float Copy_fSpeed, float Copy_fHeading, float Copy_fPitch, float Copy_fDt, MPU9250_Position_t *Copy_pPos);

/**
 * @fn ErrorState_t MPU9250_enumGetAttitude(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfPitch, float *Copy_pfRoll)
 * @brief Calculates Pitch and Roll using a Complementary Filter.
 * @param Copy_fDt  Actual elapsed time since last call [seconds] — passed by caller for accuracy.
 */
ErrorState_t MPU9250_enumGetAttitude(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfPitch, float *Copy_pfRoll);

#endif /* MPU9250_INTERFACE_H_ */
