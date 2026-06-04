#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/* TTC ---> Time To Collision */
/* TTC thresholds (seconds) */
#define FCW_WARNING_TTC  (3.0f)
#define FCW_CRITICAL_TTC (2.0f)

/* Side clearance for CRITICAL evasive steering (cm):
 * a side is "clear" to steer into if its front-side US reads >= this value */
#define FCW_SIDE_CLEAR_CM (50.0f)

/* Heading thresholds moved to System.h (shared by all safety modules) */

/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1

#endif
