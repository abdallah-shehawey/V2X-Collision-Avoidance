#ifndef SAFETYENGINE_INTERFACE_H
#define SAFETYENGINE_INTERFACE_H

/* ====== Direction Detection ====== */
/* Heading threshold for direction classification (degrees) */
#define HEADING_SAME_THRESHOLD     (30.0f)
#define HEADING_OPPOSITE_THRESHOLD (30.0f)

typedef enum
{
  DIR_SAME,
  DIR_OPPOSITE,
  DIR_UNKNOWN
} Direction_t;

/* ====== TTC & Risk Evaluation ====== */
typedef enum
{
  RISK_SAFE = 0,
  RISK_WARNING,
  RISK_CRITICAL
} RiskLevel_t;

/* ====== Public API ====== */

/**
 * @brief Initialize all safety modules (FCW, EEBL, etc.)
 */
void SafetyEngine_voidInit(void);

/**
 * @brief Single-pass update over the DSRC neighbor table
 *        Processes FCW + EEBL (and future modules) in one loop.
 *        Call this in the main loop instead of calling each module separately.
 */
void SafetyEngine_voidUpdate(void);

/**
 * @brief Detect direction relationship between two vehicles
 * @param my_heading     Host vehicle heading (0-360)
 * @param other_heading  Neighbor vehicle heading (0-360)
 * @return DIR_SAME, DIR_OPPOSITE, or DIR_UNKNOWN
 */
Direction_t SafetyEngine_DetectDirection(float my_heading, float other_heading);

/**
 * @brief Calculate Time To Collision
 * @param distance        Distance to obstacle (cm)
 * @param relative_speed  Relative closing speed
 * @return TTC in seconds, or -1.0 if no collision risk
 */
float SafetyEngine_CalcTTC(float distance, float relative_speed);

/**
 * @brief Evaluate risk level based on TTC value
 * @param ttc          Time to collision (seconds)
 * @param warning_ttc  TTC threshold for warning level
 * @param critical_ttc TTC threshold for critical level
 * @return RISK_SAFE, RISK_WARNING, or RISK_CRITICAL
 */
RiskLevel_t SafetyEngine_EvaluateRisk(float ttc, float warning_ttc, float critical_ttc);

#endif
