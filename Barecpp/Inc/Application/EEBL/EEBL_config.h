#ifndef EEBL_CONFIG_H
#define EEBL_CONFIG_H

/* Sudden braking threshold (m/s^2) */
#define EEBL_DECEL_THRESHOLD (-4.0f)

/* Distance levels (meters) */
#define EEBL_WARNING_DISTANCE (15.0f)
#define EEBL_CRITICAL_DISTANCE (5.0f)

/* Alerts enable */
#define EEBL_ENABLE_LED_ALERT 1
#define EEBL_ENABLE_BUZZER 1

#endif
