#ifndef EEBL_CONFIG_H
#define EEBL_CONFIG_H

/* Sudden braking detection threshold (cm/s drop per 50ms SafetyEngine cycle).
 * -20 cm/s/cycle ≈ -4 m/s² — a deliberate hard brake at prototype scale.
 * TUNE against the MPU speed-noise floor observed during Stage 4 testing:
 * raise the magnitude if noise causes false triggers. */
#define EEBL_DECEL_THRESHOLD (-20.0f)

/* TTC thresholds (seconds) — rear collision is imminent at short range */
#define EEBL_WARNING_TTC  (1.5f)
#define EEBL_CRITICAL_TTC (0.8f)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Maximum rear detection range (cm). In a 2m arena, only a car within ~1m behind
 * is an imminent rear threat. Also < the 400 "no-echo" sentinel so empty rear
 * (and the far wall ~2m) are correctly rejected. */
#define EEBL_MAX_DETECTION_RANGE (100.0f)

/* Alerts enable */
#define EEBL_ENABLE_LED_ALERT 1
#define EEBL_ENABLE_BUZZER    1

#endif
