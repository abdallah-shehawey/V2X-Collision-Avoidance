#include "FCW_interface.h"
#include "FCW_config.h"
#include "FCW_private.h"

/* ================= Simulation Variables ================= */
static float FCW_FrontDistance = 100.0f;
static float FCW_HostSpeed = 0.0f;
/* ======================================================== */

//  void FCW_voidSetSimulatedData(float frontDistance, float hostSpeed)
//  {
//    FCW_FrontDistance = frontDistance;
//    FCW_HostSpeed = hostSpeed;
//  }

void FCW_voidUpdate(void)
{
  FCW_voidCheckCollision();
}

/* ================= Internal Logic ================= */

static void FCW_voidCheckCollision(void)
{
  if (FCW_FrontDistance <= FCW_CRITICAL_DISTANCE)
  {
    FCW_voidActivateAlert(FCW_CRITICAL);
    FCW_voidSendWarning(FCW_CRITICAL);
  }
  else if (FCW_FrontDistance <= FCW_WARNING_DISTANCE)
  {
    FCW_voidActivateAlert(FCW_WARNING);
    FCW_voidSendWarning(FCW_WARNING);
  }
  /* else SAFE → no action */
}

static void FCW_voidActivateAlert(FCW_RiskLevel_t level)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_ON(); */
#endif

#if FCW_ENABLE_BUZZER
  if (level == FCW_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif

#if FCW_ENABLE_ADAS_REQUEST
  if (level == FCW_CRITICAL)
  {
    /*
      ADAS_RequestBrake();
    */
  }
#endif
}

static void FCW_voidSendWarning(FCW_RiskLevel_t level)
{
  /*
Message Example:
    Module : FCW
    Level  : WARNING / CRITICAL
    Distance : FCW_FrontDistance
    Speed    : FCW_HostSpeed
  */

  /*
    V2V_SendMessage(FCW_MSG);
  */
}
