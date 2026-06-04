#ifndef DNPW_CONFIG_H
#define DNPW_CONFIG_H

/* ====== TTC Thresholds (seconds) ====== */
#define DNPW_WARNING_TTC  (6.0f)
#define DNPW_CRITICAL_TTC (4.0f)

/* ====== Front Vehicle Proximity Threshold (cm) ====== */
/* If front ultrasonic reads < this value, a vehicle ahead is assumed */
#define DNPW_FRONT_THRESHOLD (300.0f)

/* ====== Alerts ====== */
#define DNPW_ENABLE_LED_ALERT 1
#define DNPW_ENABLE_BUZZER    1

#endif