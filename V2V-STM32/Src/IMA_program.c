/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<   IMA_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : IMA (Intersection Movement Assist)              **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/IMA/IMA_interface.h"
#include "../Inc/Application/IMA/IMA_config.h"
#include "../Inc/Application/IMA/IMA_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t     IMA_CurrentFlag      = 0;         /* 0=Safe, 1=Warning — DSRC broadcast */
static uint8_t     IMA_CrossingDetected = 0;         /* At least one crossing-dir neighbor */
static uint8_t     IMA_IShouldWait      = 0;         /* Other vehicle is faster → I yield */
static RiskLevel_t IMA_WorstRisk        = RISK_SAFE;

/* ============ Init ============ */
void IMA_voidInit(void)
{
  IMA_CurrentFlag      = 0;
  IMA_CrossingDetected = 0;
  IMA_IShouldWait      = 0;
  IMA_WorstRisk        = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

void IMA_voidBeginCycle(void)
{
  IMA_CrossingDetected = 0;
  IMA_IShouldWait      = 0;
  IMA_WorstRisk        = RISK_SAFE;
}

void IMA_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  /* Gate: only crossing traffic matters at intersections */
  if (dir != DIR_CROSSING)
    return;

  /* A crossing vehicle is detected */
  IMA_CrossingDetected = 1;

  /*
   * Priority rule: higher speed = right of way.
   * If other vehicle is faster (or equal) → I yield.
   * If I am faster → I pass first, no alert.
   */
  if (G_stHostVehicleState.Speed > n->speed)
    return; /* I have priority — no alert */

  /* Other is faster or equal → I should yield */
  IMA_IShouldWait = 1;

  /* WARNING: cross traffic has priority */
  if (RISK_WARNING > IMA_WorstRisk)
    IMA_WorstRisk = RISK_WARNING;
}

void IMA_voidEndCycle(void)
{
  if (IMA_CrossingDetected && IMA_IShouldWait)
  {
    IMA_CurrentFlag = (uint8_t)IMA_WorstRisk;
    IMA_ActivateAlert(IMA_WorstRisk);
  }
  else
  {
    IMA_CurrentFlag = 0;
    IMA_DeactivateAlert();
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

void IMA_voidUpdate(void)
{
  Neighbor *table = DSRC_GetTable();
  uint8_t count   = DSRC_GetCount();

  IMA_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(G_stHostVehicleState.Heading, table[i].heading);
    IMA_voidProcessNeighbor(&table[i], dir);
  }

  IMA_voidEndCycle();
}

/* ============ Public Getter ============ */
uint8_t IMA_u8GetFlag(void)
{
  return IMA_CurrentFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

static void IMA_ActivateAlert(RiskLevel_t level)
{
  (void)level;
#if IMA_ENABLE_LED_ALERT
  /* LED_IMA_ON(); */
#endif
#if IMA_ENABLE_BUZZER
  /* BUZZER_SHORT_BEEP(); */
#endif
}

static void IMA_DeactivateAlert(void)
{
#if IMA_ENABLE_LED_ALERT
  /* LED_IMA_OFF(); */
#endif
#if IMA_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
