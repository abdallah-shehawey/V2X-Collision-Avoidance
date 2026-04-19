#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/* TTC ---> Time To Collision */
/* TTC thresholds (seconds) */
#define FCW_WARNING_TTC  (3.0f)
#define FCW_CRITICAL_TTC (2.0f)

/* Heading difference threshold for opposite direction detection (degrees) */
#define FCW_OPPOSITE_HEADING_THRESHOLD (30.0f) /* |diff - 180| <= 30 → opposite */
#define FCW_SAME_HEADING_THRESHOLD     (30.0f) /* |diff| <= 30 → same direction */



/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1

#endif
