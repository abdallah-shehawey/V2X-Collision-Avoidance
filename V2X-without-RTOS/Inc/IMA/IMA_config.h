#ifndef IMA_CONFIG_H
#define IMA_CONFIG_H

/* ====== Intersection Proximity Gate (cm) ====== */
/* IMA only activates when BOTH vehicles are within this range of the intersection */
#define IMA_INTERSECTION_RANGE (2000.0f)   /* 20 meters */

/* ====== Delay Thresholds (seconds) ====== */
/* Time-to-intersection thresholds for risk evaluation */
#define IMA_WARNING_DELAY  (4.0f)
#define IMA_CRITICAL_DELAY (2.0f)

/* ====== Alerts ====== */
#define IMA_ENABLE_LED_ALERT 1
#define IMA_ENABLE_BUZZER    1

#endif
