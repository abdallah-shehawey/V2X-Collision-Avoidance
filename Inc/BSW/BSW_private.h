#ifndef BSW_PRIVATE_H
#define BSW_PRIVATE_H

typedef enum
{
  BSW_SAFE = 0,
  BSW_WARNING,
  BSW_CRITICAL
} BSW_RiskLevel_t;

/* Internal functions */
static void BSW_voidCheckBlindSpot(void);
static void BSW_voidActivateAlert(BSW_RiskLevel_t level, unsigned char side);
static void BSW_voidSendWarning(BSW_RiskLevel_t level, unsigned char side);

/* Side definition */
#define BSW_LEFT 0
#define BSW_RIGHT 1

#endif
