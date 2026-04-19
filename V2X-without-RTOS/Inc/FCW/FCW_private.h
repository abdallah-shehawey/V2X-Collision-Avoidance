#ifndef FCW_PRIVATE_H
#define FCW_PRIVATE_H

#include "../DSRC.h"

typedef enum
{
  FCW_SAFE = 0,
  FCW_WARNING,
  FCW_CRITICAL
} FCW_RiskLevel_t;

typedef enum
{
  FCW_DIR_SAME,
  FCW_DIR_OPPOSITE,
  FCW_DIR_UNKNOWN
} FCW_Direction_t;

/* Internal functions */
static FCW_Direction_t FCW_DetectDirection(float my_heading, float other_heading);
static float FCW_CalcHeadingDiff(float h1, float h2);
static float FCW_CalcTTC(float distance, float relative_speed);
static void FCW_ActivateAlert(FCW_RiskLevel_t level);
static void FCW_DeactivateAlert(void);
static FCW_RiskLevel_t FCW_EvaluateRisk(float ttc);

#endif
