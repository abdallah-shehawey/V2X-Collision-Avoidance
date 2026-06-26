/*
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<   DNPW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : DNPW (Do Not Pass Warning)                      **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/DNPW/DNPW_interface.h"
#include "../Inc/Application/DNPW/DNPW_config.h"
#include "../Inc/Application/DNPW/DNPW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t     DNPW_CurrentFlag      = 0;         /* 0=Safe, 1=Warning, 2=Critical — DSRC broadcast */

/* Cycle accumulators */
static uint8_t     DNPW_FrontVehicle     = 0;         /* FrontCenterUS: car to overtake is ahead */
static uint8_t     DNPW_LeftBlocked      = 0;         /* FrontLeftUS: overtaking lane is occupied */
static uint8_t     DNPW_OppositeDetected = 0;         /* DSRC: at least one opposite-dir neighbor */
static RiskLevel_t DNPW_WorstRisk        = RISK_SAFE; /* Worst TTC from opposite neighbors */
static float       DNPW_FrontDist        = 0.0f;

/* ============ Init ============ */
void DNPW_voidInit(void)
{
  DNPW_CurrentFlag      = 0;
  DNPW_FrontVehicle     = 0;
  DNPW_LeftBlocked      = 0;
  DNPW_OppositeDetected = 0;
  DNPW_WorstRisk        = RISK_SAFE;
  DNPW_FrontDist        = 0.0f;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

void DNPW_voidBeginCycle(float front_dist, float left_dist)
{
  DNPW_FrontDist = front_dist;

  /* Gate 1: car to overtake detected ahead */
  DNPW_FrontVehicle = (front_dist > 0.0f && front_dist < DNPW_FRONT_THRESHOLD) ? 1U : 0U;

  /* Gate 2a: overtaking lane blocked by local obstacle */
  DNPW_LeftBlocked  = (left_dist  > 0.0f && left_dist  < DNPW_LEFT_THRESHOLD)  ? 1U : 0U;

  /* Reset DSRC accumulator */
  DNPW_OppositeDetected = 0;
  DNPW_WorstRisk        = RISK_SAFE;
}

void DNPW_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  if (dir != DIR_OPPOSITE)
    return;

  /* Gate 2b: oncoming traffic exists */
  DNPW_OppositeDetected = 1;

  float rel_speed = G_stHostVehicleState.Speed + n->speed;
  if (rel_speed > 0.0f && DNPW_FrontDist > 0.0f)
  {
    float ttc = SafetyEngine_CalcTTC(DNPW_FrontDist, rel_speed);
    RiskLevel_t level = SafetyEngine_EvaluateRisk(ttc, DNPW_WARNING_TTC, DNPW_CRITICAL_TTC);
    if (level > DNPW_WorstRisk)
      DNPW_WorstRisk = level;
  }
}

void DNPW_voidEndCycle(void)
{
  /* DNPW only fires in a real "about to overtake into danger" scenario, which
   * needs BOTH:
   *   1. a vehicle ahead to overtake   → front ultrasonic sees something close
   *   2. oncoming traffic in the other lane → DSRC reports an OPPOSITE-direction
   *      neighbor (a car we may not see with our own sensors)
   * If either is missing, passing isn't the situation → SAFE. */
  if (!DNPW_FrontVehicle || !DNPW_OppositeDetected)
  {
    DNPW_CurrentFlag = 0;
    DNPW_DeactivateAlert();
    return;
  }

  /* Car ahead + oncoming car → WARNING ("do not pass").
   * If the overtaking lane (front-LEFT ultrasonic) is ALSO physically blocked,
   * passing is even more dangerous → escalate to CRITICAL. */
  RiskLevel_t alert = DNPW_LeftBlocked ? RISK_CRITICAL : RISK_WARNING;

  DNPW_CurrentFlag = (uint8_t)alert;
  DNPW_ActivateAlert(alert);
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

void DNPW_voidUpdate(void)
{
  Neighbor *table   = DSRC_GetTable();
  uint8_t count     = DSRC_GetCount();
  float front_dist  = G_stHostVehicleState.FrontCenterUS;
  float left_dist   = G_stHostVehicleState.FrontLeftUS;

  DNPW_voidBeginCycle(front_dist, left_dist);

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(G_stHostVehicleState.Heading, table[i].heading);
    DNPW_voidProcessNeighbor(&table[i], dir);
  }

  DNPW_voidEndCycle();
}

/* ============ Public Getter ============ */
uint8_t DNPW_u8GetFlag(void)
{
  return DNPW_CurrentFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

static void DNPW_ActivateAlert(RiskLevel_t level)
{
#if DNPW_ENABLE_LED_ALERT
  /* LED_DNPW_ON(); */
#endif
#if DNPW_ENABLE_BUZZER
  if (level == RISK_CRITICAL) { /* BUZZER_CONTINUOUS(); */ }
  else                         { /* BUZZER_SHORT_BEEP(); */ }
#endif
}

static void DNPW_DeactivateAlert(void)
{
#if DNPW_ENABLE_LED_ALERT
  /* LED_DNPW_OFF(); */
#endif
#if DNPW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
