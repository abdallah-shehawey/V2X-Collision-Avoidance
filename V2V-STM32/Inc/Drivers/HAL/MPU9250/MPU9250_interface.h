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
typedef struct
{
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
typedef struct
{
  float X; /**< Current X position [meters] */
  float Y; /**< Current Y position [meters] */
  float Z; /**< Current Z position [meters] */
} MPU9250_Position_t;

/**
 * @brief Bring the IMU up: reset it, configure it, and enable its internal I2C
 *        master so the magnetometer becomes reachable.
 *
 * Also verifies the `WHO_AM_I` identity register, so a mis-wired or dead part is
 * reported here rather than showing up later as plausible-looking garbage.
 *
 * @retval OK            The IMU answered and is configured.
 * @retval NOK           The identity check failed — check the SPI wiring and CS.
 * @retval TIMEOUT_STATE The part did not respond in time.
 */
ErrorState_t MPU9250_enumInit(void);

/**
 * @brief Read the accelerometer, gyroscope, magnetometer and temperature in one burst.
 *
 * Reading the whole block in a single transfer is what keeps the axes coherent —
 * three separate reads could straddle two samples and describe a vehicle
 * orientation that never existed.
 *
 * @param[out] Copy_pData Receives the converted readings, in physical units.
 * @retval OK           The data was read.
 * @retval NULL_POINTER @p Copy_pData was NULL.
 * @retval NOK          The SPI transfer failed.
 */
ErrorState_t MPU9250_enumReadData(MPU9250_Data_t *Copy_pData);

/**
 * @brief Fused compass heading: the magnetometer, corrected for hard iron, blended
 *        with the integrated Z gyro through a wrap-aware complementary filter.
 *
 * "Wrap-aware" matters: heading is circular, so 359° and 1° are two degrees
 * apart, not 358. A filter that averages them naively swings the heading right
 * through the back of the compass every time the car crosses north.
 *
 * @param[in]  Copy_pData    A fresh reading from @ref MPU9250_enumReadData.
 * @param      Copy_fDt      Time since the previous call [s]. Feeding it a wrong
 *                           value scales the gyro term and corrupts the fusion.
 * @param[out] Copy_pfHeading Receives the heading [degrees], 0..360.
 * @retval OK           The heading was updated.
 * @retval NULL_POINTER @p Copy_pData or @p Copy_pfHeading was NULL.
 *
 * @note The heading is only *absolutely* correct once
 *       @ref MPU9250_enumCalibrateMag has run. Without it the value is still
 *       smooth and still tracks turns, but it is offset from true north by
 *       however much iron is in the car.
 * @see MPU9250_HEADING_ALPHA, MPU9250_GYROZ_SIGN
 */
ErrorState_t MPU9250_enumGetHeading(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfHeading);

/**
 * @brief Boot-time hard-iron calibration: learn and freeze the magnetometer's X/Y bias.
 *
 * Turn the vehicle through a full circle — one or two slow rotations — while this
 * runs. It samples the field for @ref MPU9250_MAGCAL_DURATION_MS and takes the
 * centre of the circle the readings trace out as the offset.
 *
 * @retval OK  The car turned far enough; the offsets were computed and applied.
 * @retval NOK The field span was under @ref MPU9250_MAG_MIN_SPAN, meaning the car
 *             did not really turn. The previous offsets are left untouched rather
 *             than replaced with a meaningless one.
 */
ErrorState_t MPU9250_enumCalibrateMag(void);

/**
 * @brief Estimate forward speed by integrating acceleration, with gravity removed.
 *
 * Gravity compensation is not optional: a car on a 5° slope reads a persistent
 * ~0.09 g along its axis, and integrating that unremoved invents about 0.9 m/s of
 * speed every second the car sits still.
 *
 * @param[in]  Copy_pData   A fresh reading from @ref MPU9250_enumReadData.
 * @param      Copy_fDt     Time since the previous call [s].
 * @param[out] Copy_pfSpeed Receives the speed [cm/s], the unit the ADAS modules
 *                          and the DSRC broadcast both expect.
 * @retval OK           The speed was updated.
 * @retval NULL_POINTER @p Copy_pData or @p Copy_pfSpeed was NULL.
 *
 * @warning This is dead reckoning from an accelerometer, so the estimate drifts:
 *          any residual bias is integrated without bound. It is good enough for
 *          the short-horizon time-to-collision maths, and it is not a substitute
 *          for a wheel encoder.
 */
ErrorState_t MPU9250_enumGetSpeed(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfSpeed);

/**
 * @brief Advance the dead-reckoned position by one step.
 *
 * Projects the current speed along the current heading and accumulates it, with
 * pitch used to separate real forward motion from the gravity component.
 *
 * @param[in]  Copy_pData    A fresh reading from @ref MPU9250_enumReadData.
 * @param      Copy_fSpeed   Current speed [cm/s], from @ref MPU9250_enumGetSpeed.
 * @param      Copy_fHeading Current heading [degrees], from @ref MPU9250_enumGetHeading.
 * @param      Copy_fPitch   Current pitch [degrees], from @ref MPU9250_enumGetAttitude.
 * @param      Copy_fDt      Time since the previous call [s].
 * @param[out] Copy_pPos     Receives the updated position.
 * @retval OK           The position was advanced.
 * @retval NULL_POINTER @p Copy_pData or @p Copy_pPos was NULL.
 *
 * @warning Inherits the drift of @ref MPU9250_enumGetSpeed and then integrates it
 *          a second time, so the position error grows quadratically. Treat it as a
 *          short-term relative estimate, never as an absolute location.
 */
ErrorState_t MPU9250_enumGetPosition(MPU9250_Data_t *Copy_pData, float Copy_fSpeed, float Copy_fHeading, float Copy_fPitch, float Copy_fDt, MPU9250_Position_t *Copy_pPos);

/**
 * @brief Pitch and roll, from a complementary filter over the accelerometer and gyro.
 *
 * The accelerometer alone can find "down" but is corrupted by every bump; the gyro
 * alone is smooth but drifts. Blending them gives an angle that is both steady and
 * drift-free.
 *
 * @param[in]  Copy_pData   A fresh reading from @ref MPU9250_enumReadData.
 * @param      Copy_fDt     Actual time since the previous call [s]. The caller passes
 *                          the measured value rather than a nominal one, because a
 *                          jittery period would otherwise mis-scale the gyro term.
 * @param[out] Copy_pfPitch Receives the pitch [degrees].
 * @param[out] Copy_pfRoll  Receives the roll [degrees].
 * @retval OK           Pitch and roll were updated.
 * @retval NULL_POINTER Any of the pointer arguments was NULL.
 */
ErrorState_t MPU9250_enumGetAttitude(MPU9250_Data_t *Copy_pData, float Copy_fDt, float *Copy_pfPitch, float *Copy_pfRoll);

#endif /* MPU9250_INTERFACE_H_ */
