/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SDW_program.c     >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : SDW                                             **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/SDW/SDW_interface.h"
#include "../Inc/Application/SDW/SDW_config.h"
#include "../Inc/Application/SDW/SDW_private.h"

/* ================= Simulation Variables ================= */
static float SDW_FrontDist = 100.0f;
static float SDW_RearDist = 100.0f;
static float SDW_LeftDist = 100.0f;
static float SDW_RightDist = 100.0f;
static float SDW_HostSpeed = 0.0f;
/* ======================================================== */

void SDW_voidInit(void)
{
  SDW_FrontDist = SDW_RearDist = SDW_LeftDist = SDW_RightDist = 100.0f;
  SDW_HostSpeed = 0.0f;
}

// void SDW_voidSetSimulatedData(float frontDist, float rearDist, float leftDist, float rightDist, float hostSpeed)
// {
//   SDW_FrontDist = frontDist;
//   SDW_RearDist = rearDist;
//   SDW_LeftDist = leftDist;
//   SDW_RightDist = rightDist;
//   SDW_HostSpeed = hostSpeed;
// }

void SDW_voidUpdate(void)
{
  SDW_voidCheckDirection(SDW_FrontDist, SDW_FRONT);
  SDW_voidCheckDirection(SDW_RearDist, SDW_REAR);
  SDW_voidCheckDirection(SDW_LeftDist, SDW_LEFT);
  SDW_voidCheckDirection(SDW_RightDist, SDW_RIGHT);
}

/* ================= Core Logic ================= */

static float SDW_f32CalculateSafeDistance(void)
{
  return (SDW_HostSpeed * SDW_REACTION_TIME);
}

static void SDW_voidCheckDirection(float distance, unsigned char direction)
{
  float safeDist = SDW_f32CalculateSafeDistance();
  float criticalDist = safeDist * SDW_CRITICAL_RATIO;

  if (distance <= criticalDist)
  {
    SDW_voidActivateAlert(SDW_CRITICAL, direction);
  }
  else if (distance <= safeDist)
  {
    SDW_voidActivateAlert(SDW_WARNING, direction);
  }
}

static void SDW_voidActivateAlert(SDW_RiskLevel_t level, unsigned char direction)
{
#if SDW_ENABLE_LED_ALERT
  /*
    LED_ON(direction);
  */
#endif

#if SDW_ENABLE_BUZZER
  if (level == SDW_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif

#if SDW_ENABLE_ADAS_REQUEST
  if (level == SDW_CRITICAL && direction == SDW_FRONT)
  {
    /*
      ADAS_RequestBrake();
    */
  }
#endif
}
