#ifndef DNPW_CONFIG_H
#define DNPW_CONFIG_H

/*
 * ====== Distance-Based Do-Not-Pass Model ======
 * No TTC, no relative speed, no closing speed.
 *
 * DNPW is an OVERTAKING warning. It activates only when:
 *   FCW_Local = 1  (vehicle ahead AND oncoming vehicle in opposite dir)
 *   FCW_DSRC  = 0  (the oncoming vehicle is NOT itself following a car,
 *                   i.e. it is just driving in its own lane)
 * => only THIS car is the one attempting to overtake.
 *
 * Once active, the SAFE/WARNING/CRITICAL level is judged from the host
 * speed (m/s) and the FRONT ultrasonic distance (cm) via
 * SafetyEngine_AssessDistanceRisk() — the faster you go / the closer the
 * car ahead, the less room to complete the pass.
 */

/* Front vehicle proximity gate (cm) — closer than this means "car ahead" */
#define DNPW_FRONT_THRESHOLD (300.0f)

/* Distance-based risk model (same scale as FCW/EEBL) */
#define DNPW_SAFE_DIST_PER_MS  (35.0f)
#define DNPW_MIN_SAFE_DISTANCE (20.0f)
#define DNPW_CRITICAL_RATIO    (0.6f)

/* ====== Alerts ====== */
#define DNPW_ENABLE_LED_ALERT 1
#define DNPW_ENABLE_BUZZER    1

#endif
