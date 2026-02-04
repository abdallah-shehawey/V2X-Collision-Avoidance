#include "BSW/BSW_interface.h"
#include "BSW/BSW_config.h"
#include "BSW/BSW_private.h"

/* ================= Simulation Variables ================= */
static float BSW_LeftDistance = 100.0f;
static float BSW_RightDistance = 100.0f;

static unsigned char BSW_LeftSignal = 0;
static unsigned char BSW_RightSignal = 0;
/* ======================================================== */

void BSW_voidInit(void)
{
  BSW_LeftDistance = 100.0f;
  BSW_RightDistance = 100.0f;
  BSW_LeftSignal = 0;
  BSW_RightSignal = 0;
}

void BSW_voidSetSimulatedData(float leftDist, float rightDist, unsigned char leftSignal, unsigned char rightSignal)
{
  BSW_LeftDistance = leftDist;
  BSW_RightDistance = rightDist;
  BSW_LeftSignal = leftSignal;
  BSW_RightSignal = rightSignal;
}

void BSW_voidUpdate(void)
{
  BSW_voidCheckBlindSpot();
}

/* ================= Internal Logic ================= */

static void BSW_voidCheckBlindSpot(void)
{
  /* -------- LEFT SIDE -------- */
  if (BSW_LeftDistance <= BSW_CRITICAL_DISTANCE && BSW_LeftSignal)
  {
    BSW_voidActivateAlert(BSW_CRITICAL, BSW_LEFT);
    BSW_voidSendWarning(BSW_CRITICAL, BSW_LEFT);
  }
  else if (BSW_LeftDistance <= BSW_WARNING_DISTANCE)
  {
    BSW_voidActivateAlert(BSW_WARNING, BSW_LEFT);
    BSW_voidSendWarning(BSW_WARNING, BSW_LEFT);
  }

  /* -------- RIGHT SIDE -------- */
  if (BSW_RightDistance <= BSW_CRITICAL_DISTANCE && BSW_RightSignal)
  {
    BSW_voidActivateAlert(BSW_CRITICAL, BSW_RIGHT);
    BSW_voidSendWarning(BSW_CRITICAL, BSW_RIGHT);
  }
  else if (BSW_RightDistance <= BSW_WARNING_DISTANCE)
  {
    BSW_voidActivateAlert(BSW_WARNING, BSW_RIGHT);
    BSW_voidSendWarning(BSW_WARNING, BSW_RIGHT);
  }
}

static void BSW_voidActivateAlert(BSW_RiskLevel_t level, unsigned char side)
{
#if BSW_ENABLE_LED_ALERT
  /*
     if(side == BSW_LEFT)  LED_LEFT_ON();
     else                 LED_RIGHT_ON();
  */
#endif

#if BSW_ENABLE_BUZZER
  if (level == BSW_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif
}

static void BSW_voidSendWarning(BSW_RiskLevel_t level, unsigned char side)
{
  /*
      Message Example:
      Module : BSW
      Side   : LEFT / RIGHT
      Level  : WARNING / CRITICAL
  */

  /*
  V2V_SendMessage(BSW_MSG);
  ADAS_NotifyBlindSpot(side, level);
  */
}
