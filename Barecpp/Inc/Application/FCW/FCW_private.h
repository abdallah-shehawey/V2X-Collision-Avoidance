#ifndef FCW_PRIVATE_H
#define FCW_PRIVATE_H

typedef enum
{
  FCW_SAFE = 0,
  FCW_WARNING,
  FCW_CRITICAL
} FCW_RiskLevel_t;

/* Internal */
static void FCW_voidCheckCollision(void);
static float FCW_f32CalculateTTC(void);
static void FCW_voidActivateAlert(FCW_RiskLevel_t level);
static void FCW_voidSendWarning(FCW_RiskLevel_t level);

#endif
