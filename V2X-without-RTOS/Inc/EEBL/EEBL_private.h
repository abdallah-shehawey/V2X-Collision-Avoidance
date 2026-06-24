#ifndef EEBL_PRIVATE_H
#define EEBL_PRIVATE_H

#include "../System.h"

/* Internal functions */
static float       EEBL_SafeDistance(void);
static RiskLevel_t EEBL_EvaluateGap(float rear_distance);
static void        EEBL_ActivateAlert(RiskLevel_t level);
static void        EEBL_DeactivateAlert(void);

#endif
