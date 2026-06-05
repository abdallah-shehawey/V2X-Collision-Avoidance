/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    FCW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : FCW (Forward Collision Warning)                 **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/FCW/FCW_config.h"
#include "../Inc/Application/FCW/FCW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"

/* ============ Module State ============ */
static uint8_t     FCW_CurrentFlag = 0;       /* 0=Safe, 1=Warning, 2=Critical — DSRC broadcast */
static RiskLevel_t FCW_LocalWorst  = RISK_SAFE;
static RiskLevel_t FCW_AlertLevel  = RISK_SAFE;

/* ============ Init ============ */
void FCW_voidInit(void)
{
  FCW_CurrentFlag = 0;
  FCW_LocalWorst  = RISK_SAFE;
  FCW_AlertLevel  = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

void FCW_voidBeginCycle(void)
{
  FCW_LocalWorst = RISK_SAFE;
  FCW_AlertLevel = RISK_SAFE;
}

void FCW_voidProcessNeighbor(const Neighbor *n, float front_distance, Direction_t dir)
{
  float rel_speed;

  if (dir == DIR_OPPOSITE)
    rel_speed = G_stHostVehicleState.Speed + n->speed;
  else if (dir == DIR_SAME)
    rel_speed = G_stHostVehicleState.Speed - n->speed;
  else
    return;

  if (rel_speed > 0.0f && front_distance > 0.0f)
  {
    float ttc = SafetyEngine_CalcTTC(front_distance, rel_speed);
    RiskLevel_t level = SafetyEngine_EvaluateRisk(ttc, FCW_WARNING_TTC, FCW_CRITICAL_TTC);

    if (level > FCW_LocalWorst)
      FCW_LocalWorst = level;

    if (dir == DIR_OPPOSITE)
    {
      if (level > RISK_SAFE && n->fcw_flag > 0)
        if (level > FCW_AlertLevel)
          FCW_AlertLevel = level;
    }
    else /* DIR_SAME */
    {
      if (level > FCW_AlertLevel)
        FCW_AlertLevel = level;
    }
  }
}

void FCW_voidEndCycle(void)
{
  FCW_CurrentFlag = (uint8_t)FCW_LocalWorst;

  if (FCW_AlertLevel > RISK_SAFE)
    FCW_ActivateAlert(FCW_AlertLevel);
  else
    FCW_DeactivateAlert();
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

void FCW_voidUpdate(void)
{
  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();
  float front_dist = G_stHostVehicleState.FrontCenterUS;
  float my_heading = G_stHostVehicleState.Heading;

  FCW_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(my_heading, table[i].heading);
    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
  }

  FCW_voidEndCycle();
}

/* ============ Public Getters ============ */

uint8_t FCW_u8GetFlag(void)
{
  return FCW_CurrentFlag;
}

uint8_t FCW_u8GetAlertLevel(void)
{
  return (uint8_t)FCW_AlertLevel;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

static void FCW_ActivateAlert(RiskLevel_t level)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_ON(); */
#endif

#if FCW_ENABLE_BUZZER
  if (level == RISK_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif

#if FCW_ENABLE_ADAS_REQUEST
  if (level == RISK_CRITICAL)
  {
    /* ADAS_RequestBrake(); */
  }
#endif
}

static void FCW_DeactivateAlert(void)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_OFF(); */
#endif

#if FCW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
