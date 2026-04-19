#ifndef EEBL_PRIVATE_H
#define EEBL_PRIVATE_H

#include "../DSRC.h"

typedef enum
{
  EEBL_SAFE = 0,
  EEBL_WARNING,
  EEBL_CRITICAL
} EEBL_RiskLevel_t;

/* Internal functions */
static float EEBL_CalcHeadingDiff(float h1, float h2);
static uint8_t EEBL_IsSameDirection(float my_heading, float other_heading);
static float EEBL_CalcTTC(float distance, float relative_speed);
static EEBL_RiskLevel_t EEBL_EvaluateRisk(float ttc);
static void EEBL_ActivateAlert(EEBL_RiskLevel_t level);
static void EEBL_DeactivateAlert(void);

#endif
