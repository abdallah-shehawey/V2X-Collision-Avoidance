/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    FCW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : FCW (Cooperative Forward Collision Warning)     **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/FCW/FCW_config.h"
#include "../Inc/Application/FCW/FCW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t    FCW_CurrentFlag = 0;  /* 0=Safe, 1=Warning, 2=Critical */

/* Cycle accumulators (set during BeginCycle, used during ProcessNeighbor) */
static RiskLevel_t FCW_LocalWorst = RISK_SAFE;
static RiskLevel_t FCW_AlertLevel = RISK_SAFE;

/* Persistent history for local (US-only) closing-speed detection.
 * Initialized high (>= FCW_LOCAL_MAX_CM) so the first cycle is skipped. */
static float FCW_PrevFrontCm = 999.0f;

/* ============ Init ============ */
void FCW_voidInit(void)
{
  FCW_CurrentFlag = 0;
  FCW_LocalWorst  = RISK_SAFE;
  FCW_AlertLevel  = RISK_SAFE;
  FCW_PrevFrontCm = 999.0f;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new processing cycle — reset accumulators
 */
void FCW_voidBeginCycle(void)
{
  FCW_LocalWorst = RISK_SAFE;
  FCW_AlertLevel = RISK_SAFE;
}

/**
 * @brief Process one DSRC neighbor for FCW
 *
 * Uses pre-computed direction from SafetyEngine.
 * Updates both:
 *   - local_worst → for DSRC flag broadcast (always set)
 *   - alert_level → cooperative for opposite dir, local for same dir
 */
void FCW_voidProcessNeighbor(const Neighbor *n, float front_distance, Direction_t dir)
{
  float rel_speed;

  if (dir == DIR_OPPOSITE)
  {
    /* Both approaching → speeds add up */
    rel_speed = G_stHostVehicleState.Speed + n->speed;
  }
  else if (dir == DIR_SAME)
  {
    /* Following → closing speed = difference */
    rel_speed = G_stHostVehicleState.Speed - n->speed;
  }
  else
  {
    /* DIR_CROSSING and DIR_UNKNOWN → perpendicular/crossing and unknown traffic, skip for FCW */
    return;
  }

  if (rel_speed > 0.0f && front_distance > 0.0f)
  {
    float ttc = SafetyEngine_CalcTTC(front_distance, rel_speed);
    RiskLevel_t level = SafetyEngine_EvaluateRisk(ttc, FCW_WARNING_TTC, FCW_CRITICAL_TTC);

    /* Track worst local risk → this becomes the DSRC broadcast flag */
    if (level > FCW_LocalWorst)
    {
      FCW_LocalWorst = level;
    }

    /* Alert decision depends on direction */
    if (dir == DIR_OPPOSITE)
    {
      /*
       * Cooperative Confirmation (opposite direction):
       * Only trigger alert if BOTH vehicles detect danger.
       * My flag will be broadcast (local_worst), so the other
       * vehicle sees it next cycle. Here we check if the other
       * vehicle has also reported danger.
       */
      if (level > RISK_SAFE && n->fcw_flag > 0)
      {
        if (level > FCW_AlertLevel)
        {
          FCW_AlertLevel = level;
        }
      }
    }
    else /* DIR_SAME */
    {
      /*
       * Local detection only (same direction):
       * No cooperative confirmation needed.
       */
      if (level > FCW_AlertLevel)
      {
        FCW_AlertLevel = level;
      }
    }
  }
}

/**
 * @brief Local (US-only) forward-obstacle detection — no V2X needed.
 *
 * Closing speed is derived from the change in front distance, so it fires
 * whether WE drive toward an object or an object approaches a stopped car.
 * Feeds the confirmed alert (FCW_AlertLevel) → feedback. It deliberately does
 * NOT touch FCW_LocalWorst, so the cooperative DSRC broadcast stays clean.
 */
void FCW_voidProcessLocal(float front_distance, float dt)
{
  float prev = FCW_PrevFrontCm;
  FCW_PrevFrontCm = front_distance;       /* always update history */

  if (dt <= 0.0f) return;

  /* Need a real object both now and last cycle (skips sentinel transitions) */
  if (front_distance >= FCW_LOCAL_MAX_CM || prev >= FCW_LOCAL_MAX_CM) return;

  float delta = prev - front_distance;            /* > 0 → getting closer */
  if (delta <= FCW_LOCAL_DEADZONE_CM) return;     /* static / receding / noise */

  float closing = delta / dt;                     /* cm/s */
  float ttc     = SafetyEngine_CalcTTC(front_distance, closing);
  RiskLevel_t level = SafetyEngine_EvaluateRisk(ttc, FCW_WARNING_TTC, FCW_CRITICAL_TTC);

  /* Any locally-detected danger updates BOTH:
   * - AlertLevel → immediate local reaction (this vehicle)
   * - LocalWorst → broadcast via DSRC so an approaching V2X vehicle can
   *   use it for cooperative confirmation (it sees my flag + its own danger) */
  if (level > FCW_AlertLevel)
    FCW_AlertLevel = level;

  if (level > FCW_LocalWorst)
    FCW_LocalWorst = level;
}

/**
 * @brief End cycle — set FCW flag and activate/deactivate alerts
 */
void FCW_voidEndCycle(void)
{
  /*
   * CRITICAL: Always update the flag from local detection.
   * This flag is read by FCW_u8GetFlag() and sent in every DSRC message.
   * Without this, the cooperative confirmation would deadlock.
   */
  FCW_CurrentFlag = (uint8_t)FCW_LocalWorst;

  /*
   * NOTE: This module no longer drives hardware directly.
   * FCW_AlertLevel (the confirmed alert) is exposed via FCW_u8GetAlertLevel()
   * and consumed by the SafetyEngine aggregation → vTask_Feedback.
   */
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone FCW update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void FCW_voidUpdate(void)
{
  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();
  float front_dist = G_stHostVehicleState.FrontCenterUS;

  FCW_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(G_stHostVehicleState.Heading, table[i].heading);
    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
  }

  FCW_voidEndCycle();
}

/* ============ Public Getters ============ */

/**
 * @brief Local detection flag — broadcast over DSRC so the opposite
 *        vehicle can perform cooperative confirmation.
 */
uint8_t FCW_u8GetFlag(void)
{
  return FCW_CurrentFlag;
}

/**
 * @brief Confirmed alert level — drives the feedback (LED/Buzzer/Motor)
 *        via the SafetyEngine aggregation. For DIR_OPPOSITE this is only
 *        non-zero after both vehicles agree there is danger.
 */
uint8_t FCW_u8GetAlertLevel(void)
{
  return (uint8_t)FCW_AlertLevel;
}
