#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/*
 * ====== Distance-Based Forward Collision Model ======
 * No TTC, no relative speed. Risk is judged from the host's own speed
 * (m/s, from MPU) and the FRONT ultrasonic distance (cm), via
 * SafetyEngine_AssessDistanceRisk():
 *   safe_dist_cm = Host_Speed * FCW_SAFE_DIST_PER_MS  (floored at MIN)
 *   distance >= safe              -> SAFE
 *   crit*safe <= distance < safe  -> WARNING
 *   distance <  crit*safe         -> CRITICAL
 *
 * Same calibration scale as EEBL (prototype top speed ~5 m/s):
 *   @ 2 m/s -> 70 cm safe gap   (=> PER_MS = 35)
 */
#define FCW_SAFE_DIST_PER_MS   (35.0f)
#define FCW_MIN_SAFE_DISTANCE  (20.0f)
#define FCW_CRITICAL_RATIO     (0.6f)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1

#endif
