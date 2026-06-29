#ifndef SAFETYENGINE_INTERFACE_H
#define SAFETYENGINE_INTERFACE_H

/* ====== Direction Detection ====== */
/* Heading threshold for direction classification (degrees). Widened from 30° to
 * 40° to tolerate the heading error between the two vehicles' magnetometers, so a
 * real SAME/OPPOSITE/CROSSING pair is not misclassified as UNKNOWN when their
 * reported headings disagree by up to ~40°. */
#define HEADING_SAME_THRESHOLD     (40.0f)
#define HEADING_OPPOSITE_THRESHOLD (40.0f)
#define HEADING_CROSS_THRESHOLD    (40.0f)

typedef enum
{
  DIR_SAME,
  DIR_OPPOSITE,
  DIR_CROSSING,
  DIR_UNKNOWN
} Direction_t;

/* ====== Risk Evaluation ====== */
typedef enum
{
  RISK_SAFE = 0,
  RISK_WARNING,
  RISK_CRITICAL
} RiskLevel_t;

/* ====== Shared Safe-Distance Model ======
 * Speed-dependent gap shared by the distance-based modules (Local FCW + EEBL):
 *
 *   safe_cm = Host_Speed(m/s) * SAFE_DIST_PER_MS   (floored at MIN_SAFE_DISTANCE)
 *   crit_cm = safe_cm * CRITICAL_RATIO
 *
 * Prototype scale (small car, top speed ~0.5 m/s, corridor distances ~10-20 cm):
 *   @ 0.5 m/s -> safe = max(0.5*20, 15) = 15 cm  [warning zone: 10-15 cm]
 *   crit = safe * 0.667 = 10 cm  (the system minimum distance)
 *   Dashboard thresholds: SAFE=15cm  WARNING=10cm  CRITICAL=10cm
 *   -> Use MIN_SAFE_DISTANCE as the dominant floor. */
#define SAFE_DIST_PER_MS   (20.0f)    /* cm of safe gap per 1 m/s (prototype) */
#define MIN_SAFE_DISTANCE  (15.0f)    /* floor: SAFE zone  >= 15 cm           */
#define CRITICAL_RATIO     (0.667f)   /* CRITICAL below ~67% -> crit = 10 cm   */

/* ====== Shared Host Vehicle Data ======
 * Latched once per cycle by SafetyEngine_voidUpdate() from G_stHostVehicleState,
 * then read by the distance-based modules during their neighbor pass. */
extern float Host_Speed;              /* current speed (m/s)                    */
extern float Host_Heading;            /* current heading (0-360°)               */

/* Safe/critical gaps (cm) for the current cycle. Read-only for the modules. */
extern float SafetyEngine_SafeDist;
extern float SafetyEngine_CriticalDist;

/* ====== Public API ====== */

/**
 * @brief Initialize all safety modules (FCW/DNPW, EEBL, BSW, IMA).
 */
void SafetyEngine_voidInit(void);

/**
 * @brief Single-pass update over the DSRC neighbor table.
 *        Runs all safety modules in ONE pass and aggregates the result into the
 *        G_u16SystemFlags status word (2 bits/module) for vTask_Feedback and
 *        vTask_RPi_Comm to consume.
 */
void SafetyEngine_voidUpdate(void);

/**
 * @brief Detect direction relationship between two vehicles
 * @param my_heading     Host vehicle heading (0-360)
 * @param other_heading  Neighbor vehicle heading (0-360)
 * @return DIR_SAME, DIR_OPPOSITE, DIR_CROSSING, or DIR_UNKNOWN
 */
Direction_t SafetyEngine_DetectDirection(float my_heading, float other_heading);

/**
 * @brief Evaluate risk from a "lower value = higher risk" metric.
 *        Used by IMA for time-gap/delay thresholds.
 * @param value        Metric to evaluate (e.g. delay/time-gap in seconds)
 * @param warning_thr  Threshold for warning level
 * @param critical_thr Threshold for critical level
 * @return RISK_SAFE, RISK_WARNING, or RISK_CRITICAL
 */
RiskLevel_t SafetyEngine_EvaluateRisk(float value, float warning_thr, float critical_thr);

/**
 * @brief Assess collision risk from host speed and a measured distance.
 *
 *   safe_dist_cm  = host_speed(m/s) * dist_per_ms   (floored at min_dist)
 *   distance >= safe_dist               -> RISK_SAFE
 *   crit*safe <= distance < safe_dist   -> RISK_WARNING
 *   distance < crit*safe                -> RISK_CRITICAL
 *
 * @param host_speed   Host vehicle speed (m/s, from MPU)
 * @param distance     Measured distance to the object (cm, from ultrasonic)
 * @param dist_per_ms  cm of safe gap per 1 m/s of host speed (module-tuned)
 * @param min_dist     Minimum safe-distance floor (cm)
 * @param crit_ratio   Fraction of safe distance below which risk is CRITICAL
 * @return RISK_SAFE, RISK_WARNING, or RISK_CRITICAL
 */
RiskLevel_t SafetyEngine_AssessDistanceRisk(float host_speed, float distance,
                                            float dist_per_ms, float min_dist,
                                            float crit_ratio);

#endif
