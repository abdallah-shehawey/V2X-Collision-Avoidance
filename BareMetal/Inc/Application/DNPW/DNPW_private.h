#ifndef DNPW_PRIVATE_H
#define DNPW_PRIVATE_H

#include "../SafetyEngine/SafetyEngine_interface.h"

/* Internal alert functions */
static void DNPW_ActivateAlert(RiskLevel_t level);
static void DNPW_DeactivateAlert(void);

#endif