#ifndef EEBL_PRIVATE_H
#define EEBL_PRIVATE_H

#include "../SafetyEngine/SafetyEngine_interface.h"
/* Internal functions (only alert functions remain module-specific) */
static void EEBL_ActivateAlert(RiskLevel_t level);
static void EEBL_DeactivateAlert(void);

#endif
