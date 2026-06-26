#ifndef SAFETYENGINE_INTERFACE_H
#define SAFETYENGINE_INTERFACE_H

/* ====== Direction Detection ====== */
/* Heading threshold for direction classification (degrees) */
#define HEADING_SAME_THRESHOLD     (30.0f)
#define HEADING_OPPOSITE_THRESHOLD (30.0f)
#define HEADING_CROSS_THRESHOLD    (30.0f)

typedef enum
{
  DIR_SAME,
  DIR_OPPOSITE,
  DIR_CROSSING,
  DIR_UNKNOWN
} Direction_t;

/* ====== TTC & Risk Evaluation ====== */
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
 * Calibration (prototype top speed ~5 m/s): @ 2 m/s -> 70 cm => PER_MS = 35. */
#define SAFE_DIST_PER_MS   (35.0f)
#define MIN_SAFE_DISTANCE  (30.0f)
#define CRITICAL_RATIO     (0.6f)

/* Safe/critical gaps (cm) for the current cycle. Read-only for the modules. */
extern float SafetyEngine_SafeDist;
extern float SafetyEngine_CriticalDist;

/* ====== Public API ====== */

/**
 * @brief Initialize all safety modules (FCW, EEBL, BSW, etc.)
 */
void SafetyEngine_voidInit(void);

/**
 * @brief Run all safety modules in one pass over the DSRC neighbor table.
 *        Call this once per main-loop iteration.
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
