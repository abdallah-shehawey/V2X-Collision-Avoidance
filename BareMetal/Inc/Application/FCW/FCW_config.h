#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/* TTC ---> Time To Collision */
/* TTC thresholds (seconds) */
#define FCW_WARNING_TTC  (3.0f)
#define FCW_CRITICAL_TTC (2.0f)

/* CRITICAL evasive: reverse only if the rear is clear, else stop.
 * "Rear clear" = min of the 3 back US sensors >= this value (cm). */
#define FCW_BEHIND_CLEAR_CM (50.0f)

/* ===== Local (US-only) forward-obstacle detection — works without V2X ===== */
/* Distances >= this are treated as "no relevant object" (covers the out-of-range
 * sentinel). Real readings are <= ~206cm with the 2m cap, so 250 separates them. */
#define FCW_LOCAL_MAX_CM      (250.0f)
/* Ignore per-cycle distance changes smaller than this (US noise floor, cm). */
#define FCW_LOCAL_DEADZONE_CM (2.0f)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1

#endif
