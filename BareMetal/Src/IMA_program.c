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
static uint8_t IMA_CurrentFlag = 0; /* 0=Safe, 1=Warning, 2=Critical */

/* Cycle accumulators (reset in BeginCycle, used in ProcessNeighbor/EndCycle) */
static uint8_t     IMA_CrossingDetected = 0; /* At least one crossing-dir neighbor near intersection */
static uint8_t     IMA_IShouldWait      = 0; /* Host vehicle should yield (other is faster) */
static RiskLevel_t IMA_WorstRisk        = RISK_SAFE; /* Worst risk across all crossing neighbors */

/* ============ Init ============ */
void IMA_voidInit(void)
{
  IMA_CurrentFlag     = 0;
  IMA_CrossingDetected = 0;
  IMA_IShouldWait     = 0;
  IMA_WorstRisk       = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new IMA processing cycle
 *
 *  Reset all DSRC accumulators.
 *  IMA reads Host_Speed and Host_DistToIntersection from shared globals.
 */
void IMA_voidBeginCycle(void)
{
  /* Reset accumulators */
  IMA_CrossingDetected = 0;
  IMA_IShouldWait      = 0;
  IMA_WorstRisk        = RISK_SAFE;
}

/**
 * @brief Process one DSRC neighbor for IMA
 *
 * Decision flow (per the flowchart):
 *   Gate 1: Is this a crossing vehicle? (not same road)
 *   Gate 2: Are BOTH vehicles near the intersection? (< 20m each)
 *   Priority: Compare speeds → faster vehicle passes first
 *   Risk: Compute delay (time to reach intersection) → evaluate risk
 *
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 */
void IMA_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  /* Gate 1: Only crossing traffic is relevant for IMA */
  if (dir != DIR_CROSSING)
  {
    return;
  }

  /* Gate 2: Both vehicles must be near the intersection */
  if (G_stHostVehicleState.DistToIntersection <= 0.0f || G_stHostVehicleState.DistToIntersection > IMA_INTERSECTION_RANGE)
  {
    return;
  }
  if (n->distance_to_intersection <= 0.0f || n->distance_to_intersection > IMA_INTERSECTION_RANGE)
  {
    return;
  }

  /* Crossing vehicle near intersection confirmed */
  IMA_CrossingDetected = 1;

  /* ---- Priority Decision: who passes first? ---- */
  float delay;

  if (G_stHostVehicleState.Speed > n->speed)
  {
    /*
     * I am faster → I pass first → other should wait.
     * Compute other's delay (time for OTHER to reach intersection).
     * No alert needed for me.
     */
    if (n->speed > 0.0f)
    {
      delay = n->distance_to_intersection / n->speed;
    }
    else
    {
      /* Other vehicle is stopped → no collision risk from their side */
      return;
    }

    /* Even though I pass first, evaluate if it's still tight */
    RiskLevel_t level = SafetyEngine_EvaluateRisk(delay, IMA_WARNING_DELAY, IMA_CRITICAL_DELAY);
    (void)level; /* I pass first — no alert for me, but track for flag */
  }
  else
  {
    /*
     * Other is faster (or equal speed) → I should wait.
     * Compute MY delay (time for ME to reach intersection).
     * This is the dangerous case — I need a warning.
     */
    IMA_IShouldWait = 1;

    if (G_stHostVehicleState.Speed > 0.0f)
    {
      delay = G_stHostVehicleState.DistToIntersection / G_stHostVehicleState.Speed;
    }
    else
    {
      /* I am stopped → safe, no collision */
      return;
    }

    RiskLevel_t level = SafetyEngine_EvaluateRisk(delay, IMA_WARNING_DELAY, IMA_CRITICAL_DELAY);

    /* Track worst risk across all crossing neighbors (conservative) */
    if (level > IMA_WorstRisk)
    {
      IMA_WorstRisk = level;
    }
  }
}

/**
 * @brief End cycle — apply decision logic
 *
 * IMA alert triggers ONLY when ALL conditions are true:
 *   1. Crossing vehicle detected near intersection
 *   2. I should wait (other vehicle has priority / is faster)
 *   3. Risk level > SAFE (I'm approaching too fast)
 *
 * If I'm the faster vehicle, no alert — I pass first.
 */
void IMA_voidEndCycle(void)
{
  if (IMA_CrossingDetected && IMA_IShouldWait && (IMA_WorstRisk > RISK_SAFE))
  {
    /* Update flag for DSRC broadcast */
    IMA_CurrentFlag = (uint8_t)IMA_WorstRisk;

    /* Activate alert — "WAIT - CROSS TRAFFIC" */
    IMA_ActivateAlert(IMA_WorstRisk);
  }
  else
  {
    /* All clear — no intersection collision risk */
    IMA_CurrentFlag = 0;
    IMA_DeactivateAlert();
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone IMA update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
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

/**
 * @brief Activate IMA alert — warn driver to WAIT for cross traffic
 */
static void IMA_ActivateAlert(RiskLevel_t level)
{
#if IMA_ENABLE_LED_ALERT
  /* LED_IMA_ON(); */
  /* LCD_Print("! WAIT - CROSS TRAFFIC at intersection"); */
#endif

#if IMA_ENABLE_BUZZER
  if (level == RISK_CRITICAL)
  {
    /* BUZZER_CONTINUOUS(); */
  }
  else
  {
    /* BUZZER_SHORT_BEEP(); */
  }
#endif
}

/**
 * @brief Deactivate IMA alert — intersection is clear
 */
static void IMA_DeactivateAlert(void)
{
#if IMA_ENABLE_LED_ALERT
  /* LED_IMA_OFF(); */
#endif

#if IMA_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
