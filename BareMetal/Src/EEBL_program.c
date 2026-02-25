/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    EEBL_program.c     >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : EEBL                                            **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/EEBL/EEBL_interface.h"
#include "../Inc/Application/EEBL/EEBL_config.h"
#include "../Inc/Application/EEBL/EEBL_private.h"

/* ================= Simulation Variables ================= */
static float EEBL_CurrentSpeed = 0.0f;
static float EEBL_PreviousSpeed = 0.0f;
static float EEBL_RearDistance = 100.0f;

/* ======================================================== */

void EEBL_voidInit(void)
{
  EEBL_CurrentSpeed = 0.0f;
  EEBL_PreviousSpeed = 0.0f;
  EEBL_RearDistance = 100.0f;
}

// void EEBL_voidSetSimulatedData(float speed, float rearDistance)
// {
//   EEBL_CurrentSpeed = speed;
//   EEBL_RearDistance = rearDistance;
// }

void EEBL_voidUpdate(void)
{
  EEBL_voidCheckEmergencyBrake();
  EEBL_PreviousSpeed = EEBL_CurrentSpeed;
}

/* ================= Internal Logic ================= */

static void EEBL_voidCheckEmergencyBrake(void)
{
  float deceleration = EEBL_CurrentSpeed - EEBL_PreviousSpeed;

  if (deceleration <= EEBL_DECEL_THRESHOLD)
  {
    if (EEBL_RearDistance <= EEBL_CRITICAL_DISTANCE)
    {
      EEBL_voidActivateLocalAlert(EEBL_CRITICAL);
      EEBL_voidSendV2VWarning(EEBL_CRITICAL);
    }
    else if (EEBL_RearDistance <= EEBL_WARNING_DISTANCE)
    {
      EEBL_voidActivateLocalAlert(EEBL_WARNING);
      EEBL_voidSendV2VWarning(EEBL_WARNING);
    }
    /* else → safe, no action */
  }
}

static void EEBL_voidActivateLocalAlert(EEBL_RiskLevel_t level)
{
#if EEBL_ENABLE_LED_ALERT
  /* LED_ON(); */
#endif

#if EEBL_ENABLE_BUZZER
  if (level == EEBL_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif

  /* Optional:
     if(level == EEBL_CRITICAL)
         ADAS_RequestBrake();
  */
}

static void EEBL_voidSendV2VWarning(EEBL_RiskLevel_t level)
{
  /*
      Message Example:
      ID      : EEBL
      Level   : WARNING / CRITICAL
      Action  : Emergency Brake
  */

  /*
  if(level == EEBL_WARNING)
      V2V_SendMessage(EEBL_WARNING_MSG);
  else
      V2V_SendMessage(EEBL_CRITICAL_MSG);
  */
}