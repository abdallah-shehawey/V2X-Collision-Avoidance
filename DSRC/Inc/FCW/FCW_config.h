#ifndef FCW_CONFIG_H
#define FCW_CONFIG_H

/* TTC ---> Time To Collision */
/* TTC thresholds (seconds) */
#define FCW_WARNING_TTC  (3.0f)
#define FCW_CRITICAL_TTC (2.0f)

/* Heading difference threshold for opposite direction detection (degrees) */
#define FCW_OPPOSITE_HEADING_THRESHOLD (30.0f) /* |diff - 180| <= 30 → opposite */
#define FCW_SAME_HEADING_THRESHOLD     (30.0f) /* |diff| <= 30 → same direction */

/* Number of ultrasonic sensors */
#define FCW_US_SENSOR_COUNT 6

/* Ultrasonic sensor indices */
#define US_FRONT       0
#define US_FRONT_RIGHT 1
#define US_FRONT_LEFT  2
#define US_REAR        3
#define US_REAR_RIGHT  4
#define US_REAR_LEFT   5

/* Enable systems */
#define FCW_ENABLE_LED_ALERT    1
#define FCW_ENABLE_BUZZER       1
#define FCW_ENABLE_ADAS_REQUEST 1

#endif
