#ifndef DNPW_CONFIG_H
#define DNPW_CONFIG_H

/* ====== TTC Thresholds (seconds) ====== */
/* Oncoming-traffic risk while overtaking — aligned with FCW at prototype scale */
#define DNPW_WARNING_TTC  (2.0f)
#define DNPW_CRITICAL_TTC (1.0f)

/* ====== Front Vehicle Proximity Threshold (cm) ====== */
/* Car ahead I might overtake, within ~80cm. Rejects the 400cm sentinel and the
 * front wall (~85cm when centered). */
#define DNPW_FRONT_THRESHOLD (80.0f)

/* ====== Left Lane Clear Threshold (cm) ====== */
/* Overtaking lane blocked if FrontLeftUS < 50cm (adjacent lane). */
#define DNPW_LEFT_THRESHOLD  (50.0f)

/* ====== Alerts ====== */
#define DNPW_ENABLE_LED_ALERT 1
#define DNPW_ENABLE_BUZZER    1

#endif