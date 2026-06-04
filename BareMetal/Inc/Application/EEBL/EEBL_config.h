#ifndef EEBL_CONFIG_H
#define EEBL_CONFIG_H

/* Sudden braking detection threshold (speed difference, negative = braking) */
#define EEBL_DECEL_THRESHOLD (-4.0f)

/* TTC thresholds (seconds) */
#define EEBL_WARNING_TTC  (3.0f)
#define EEBL_CRITICAL_TTC (2.0f)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Maximum rear detection range (cm) — beyond this, no vehicle behind */
#define EEBL_MAX_DETECTION_RANGE (400.0f)

/* Alerts enable */
#define EEBL_ENABLE_LED_ALERT 1
#define EEBL_ENABLE_BUZZER    1

#endif
