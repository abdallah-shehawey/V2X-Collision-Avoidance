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

#include "../Inc/FCW/FCW_interface.h"
#include "../Inc/FCW/FCW_config.h"
#include "../Inc/FCW/FCW_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t    FCW_CurrentFlag = 0; /* 0=Safe, 1=Warning, 2=Critical */

/* Cycle accumulators (set during BeginCycle, used during ProcessNeighbor) */
static RiskLevel_t FCW_LocalWorst = RISK_SAFE;
static RiskLevel_t FCW_AlertLevel = RISK_SAFE;

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
    rel_speed = Host_Speed + n->speed;
  }
  else if (dir == DIR_SAME)
  {
    /* Following → closing speed = difference */
    rel_speed = Host_Speed - n->speed;
  }
  else
  {
    /* DIR_UNKNOWN → perpendicular or irrelevant, skip */
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

  /* Activate or deactivate alert based on confirmed level */
  if (FCW_AlertLevel > RISK_SAFE)
  {
    FCW_ActivateAlert(FCW_AlertLevel);
  }
  else
  {
    FCW_DeactivateAlert();
  }
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
  float front_dist = US_Distances[US_FRONT];

  FCW_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
  }

  FCW_voidEndCycle();
}

/* ============ Public Getter ============ */
uint8_t FCW_u8GetFlag(void)
{
  return FCW_CurrentFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Activate alerts (LED, Buzzer, ADAS) based on risk level
 */
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

/**
 * @brief Deactivate all alerts when risk returns to SAFE
 */
static void FCW_DeactivateAlert(void)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_OFF(); */
#endif

#if FCW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
