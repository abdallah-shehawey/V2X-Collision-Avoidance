#ifndef DNPW_PRIVATE_H
#define DNPW_PRIVATE_H

typedef enum
{
  DNPW_SAFE = 0,
  DNPW_WARNING,
  DNPW_CRITICAL
} DNPW_RiskLevel_t;

static float DNPW_f32CalculateOppositeTTC(void);
static void DNPW_voidEvaluateRisk(void);
static void DNPW_voidSendWarning(DNPW_RiskLevel_t level);

#endif