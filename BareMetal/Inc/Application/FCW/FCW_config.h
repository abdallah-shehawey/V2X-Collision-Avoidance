#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/* TTC ---> Time To Collision */
/* TTC thresholds (seconds) */
#define FCW_WARNING_TTC  (4.0f)
#define FCW_CRITICAL_TTC (2.0f)

/* Hysteresis: cycles to hold alert after danger clears (1 cycle = 50ms).
 * 3 cycles = 150ms hold before returning to SAFE. */
#define FCW_ALERT_HOLD_CYCLES (3U)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1
#endif
