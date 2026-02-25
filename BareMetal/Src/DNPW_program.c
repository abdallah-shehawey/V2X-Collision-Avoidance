/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<   DNPW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : DNPW                                            **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/DNPW/DNPW_interface.h"
#include "../Inc/Application/DNPW/DNPW_config.h"
#include "../Inc/Application/DNPW/DNPW_private.h"

/* ================= Simulation Variables ================= */
static float DNPW_OppositeDistance = 200.0f;
static float DNPW_HostSpeed = 0.0f;
static float DNPW_OppositeSpeed = 0.0f;
/* ======================================================== */

void DNPW_voidInit(void)
{
  DNPW_OppositeDistance = 200.0f;
  DNPW_HostSpeed = 0.0f;
  DNPW_OppositeSpeed = 0.0f;
}

void DNPW_voidSetSimulatedData(float oppositeDistance, float hostSpeed, float oppositeSpeed)
{
  DNPW_OppositeDistance = oppositeDistance;
  DNPW_HostSpeed = hostSpeed;
  DNPW_OppositeSpeed = oppositeSpeed;
}

void DNPW_voidUpdate(void)
{
  DNPW_voidEvaluateRisk();
}

/* ================= Core Logic ================= */

static float DNPW_f32CalculateOppositeTTC(void)
{
  float relativeSpeed = DNPW_HostSpeed + DNPW_OppositeSpeed;

  if (relativeSpeed <= 0.0f)
  {
    return -1.0f;
  }

  return (DNPW_OppositeDistance / relativeSpeed);
}

static void DNPW_voidEvaluateRisk(void)
{
  float ttc = DNPW_f32CalculateOppositeTTC();

  if (ttc <= 0)
  {
    return;
  }

  if (ttc <= DNPW_CRITICAL_TTC)
  {
    DNPW_voidSendWarning(DNPW_CRITICAL);
  }
  else if (ttc <= DNPW_WARNING_TTC)
  {
    DNPW_voidSendWarning(DNPW_WARNING);
  }
}

static void DNPW_voidSendWarning(DNPW_RiskLevel_t level)
{
#if DNPW_ENABLE_V2V_WARNING
  /*
      Message:
      Type      : DNPW
      Level     : WARNING / CRITICAL
      TTC       : calculated
      Advice    : DO_NOT_OVERTAKE
  */

  /*
      V2V_Broadcast(DNPW_MSG);
  */
#endif
}
