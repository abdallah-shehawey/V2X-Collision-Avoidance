/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    FCW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : FCW                                             **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/FCW/FCW_interface.h"
#include "../Inc/FCW/FCW_config.h"
#include "../Inc/FCW/FCW_private.h"

/* ================= Simulation Variables ================= */
static float FCW_FrontDistance = 100.0f;
static float FCW_HostSpeed = 0.0f;
static float FCW_FrontVehicleSpeed = 0.0f;
/* ======================================================== */

void FCW_voidInit(void)
{
  /* get data from sensors because the car may be stopped in any time */
  // FCW_FrontDistance = 100.0f;
  // FCW_HostSpeed = 0.0f;
  // FCW_FrontVehicleSpeed = 0.0f;
}

// void FCW_voidSetSimulatedData(float frontDistance, float hostSpeed, float frontVehicleSpeed)
// {
//   FCW_FrontDistance = frontDistance;
//   FCW_HostSpeed = hostSpeed;
//   FCW_FrontVehicleSpeed = frontVehicleSpeed;
// }

void FCW_voidUpdate(void)
{
  // get data from sensors to update variables
  // get FCW_FrontVehicleSpeed from srvice
  FCW_voidCheckCollision();
}

/* ================= Core Logic ================= */

static void FCW_voidCheckCollision(void)
{
  float ttc = FCW_f32CalculateTTC();

  if (ttc > 0) /* Valid TTC */
  {
    if (ttc <= FCW_CRITICAL_TTC)
    {
      FCW_voidActivateAlert(FCW_CRITICAL);
      FCW_voidSendWarning(FCW_CRITICAL);
    }
    else if (ttc <= FCW_WARNING_TTC)
    {
      FCW_voidActivateAlert(FCW_WARNING);
      FCW_voidSendWarning(FCW_WARNING);
    }
  }
}

/* Calculate Time To Collision */
static float FCW_f32CalculateTTC(void)
{
  float relativeSpeed = FCW_HostSpeed - FCW_FrontVehicleSpeed;

  if (relativeSpeed <= 0.0f)
  {
    return -1.0f; /* No collision risk */
  }

  return (FCW_FrontDistance / relativeSpeed);
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
    Module : FCW
    TTC    : calculated
    Level  : WARNING / CRITICAL
  */

  /*
    V2V_SendMessage(FCW_MSG);
  */
}
