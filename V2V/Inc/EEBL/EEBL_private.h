#ifndef EEBL_PRIVATE_H
#define EEBL_PRIVATE_H

typedef enum
{
  EEBL_SAFE = 0,
  EEBL_WARNING,
  EEBL_CRITICAL
} EEBL_RiskLevel_t;

/* Internal functions */
static void EEBL_voidCheckEmergencyBrake(void);
static void EEBL_voidSendV2VWarning(EEBL_RiskLevel_t level);
static void EEBL_voidActivateLocalAlert(EEBL_RiskLevel_t level);

#endif
