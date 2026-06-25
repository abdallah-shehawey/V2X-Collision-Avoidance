#ifndef EEBL_CONFIG_H
#define EEBL_CONFIG_H

/*
 * ====== UNITS ======
 * Host_Speed is in m/s — it comes from MPU9250_enumGetSpeed() in the
 * BareMetal driver, which integrates gravity-compensated accelerometer
 * data and returns m/s. Distances (ultrasonic) are in cm.
 *
 * Prototype top speed: ~5 m/s. All numbers below are tuned for a
 * 0 .. 5 m/s range and are meant to be re-tuned on the real car —
 * just change the #defines.
 */

/*
 * Sudden braking detection threshold (m/s per cycle, negative = braking).
 * It's the drop in speed between two consecutive cycles. For a car whose
 * top speed is ~5 m/s, a -0.30 m/s step in one cycle is a clear braking
 * event. Lower the magnitude (e.g. -0.15f) to be more sensitive, raise it
 * (e.g. -0.50f) to require harder braking.
 */
#define EEBL_DECEL_THRESHOLD (-0.30f)

/*
 * Safe distance model (speed-based, NOT relative-speed based)
 * --------------------------------------------------------------
 * Safe gap grows linearly with host speed:
 *   safe_dist_cm = Host_Speed(m/s) * EEBL_SAFE_DIST_PER_MS
 *
 * EEBL_SAFE_DIST_PER_MS = "how many cm of gap per 1 m/s of speed".
 * Calibration point chosen on the real prototype:
 *   @ 2 m/s -> 70 cm  =>  PER_MS = 70 / 2 = 35
 *
 * Resulting gaps across the car's range (top speed 5 m/s):
 *   @ 5 m/s -> 175 cm   (well inside the 400 cm ultrasonic range)
 *   @ 2 m/s -> 70 cm    (calibration point)
 *   @ 1 m/s -> 35 cm
 *   @ 0.5 m/s -> 17.5 cm -> clamped to MIN floor below
 */
#define EEBL_SAFE_DIST_PER_MS (35.0f)

/* Minimum safe distance floor (cm) — applied even at very low speed.
 * Also keeps the gap above the ultrasonic's reliable near limit. */
#define EEBL_MIN_SAFE_DISTANCE (20.0f)

/*
 * Risk thresholds as a ratio of the computed safe distance:
 *   distance >= safe_dist                 -> SAFE
 *   crit_ratio*safe <= distance < safe    -> WARNING
 *   distance < crit_ratio*safe            -> CRITICAL
 * Example @ 2 m/s (safe = 70 cm, crit_ratio = 0.6):
 *   >=70 cm SAFE | 42..70 cm WARNING | <42 cm CRITICAL
 */
#define EEBL_CRITICAL_RATIO (0.6f)

/* Maximum rear detection range (cm) — beyond this, no vehicle behind */
#define EEBL_MAX_DETECTION_RANGE (400.0f)

/* Alerts enable */
#define EEBL_ENABLE_LED_ALERT 1
#define EEBL_ENABLE_BUZZER    1

#endif
