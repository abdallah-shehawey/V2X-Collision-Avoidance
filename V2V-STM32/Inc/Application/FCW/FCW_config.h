#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/* TTC ---> Time To Collision */
/* TTC thresholds (seconds) — tuned for a 2m x 2m arena, ~20-50 cm/s prototype.
 * WARNING 2.0s ≈ 80cm @ 40cm/s closing, CRITICAL 1.0s ≈ 40cm (~1.3 car lengths).
 * Low values also stop the 400cm "no-echo" sentinel from firing FCW (would need
 * an unrealistic >200 cm/s closing speed). */
#define FCW_WARNING_TTC  (2.0f)
#define FCW_CRITICAL_TTC (1.0f)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1
#endif
