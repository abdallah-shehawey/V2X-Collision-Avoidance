/**
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

#include "../Inc/DNPW/DNPW_interface.h"
#include "../Inc/DNPW/DNPW_config.h"
#include "../Inc/DNPW/DNPW_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t DNPW_CurrentFlag = 0; /* 0=Safe, 1=Warning, 2=Critical */

/* Cycle accumulators (reset in BeginCycle, used in ProcessNeighbor/EndCycle) */
static uint8_t    DNPW_OppositeDetected = 0; /* DSRC: at least one opposite-dir neighbor */
static uint8_t    DNPW_FrontVehicle     = 0; /* US: vehicle ahead (considering overtaking) */
static RiskLevel_t DNPW_WorstRisk       = RISK_SAFE; /* Worst TTC risk from opposite neighbors */
static float      DNPW_FrontDist        = 0.0f; /* Front US distance (saved from BeginCycle) */

/* ============ Init ============ */
void DNPW_voidInit(void)
{
  DNPW_CurrentFlag     = 0;
  DNPW_OppositeDetected = 0;
  DNPW_FrontVehicle    = 0;
  DNPW_WorstRisk       = RISK_SAFE;
  DNPW_FrontDist       = 0.0f;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new DNPW processing cycle
 *
 *  Step 1: Check if there is a vehicle ahead (front US gate)
 *  Step 2: Reset DSRC accumulators
 *
 * @param front_dist Front ultrasonic distance (cm)
 */
void DNPW_voidBeginCycle(float front_dist)
{
  /* Save front distance for TTC calculation in ProcessNeighbor */
  DNPW_FrontDist = front_dist;

  /* Gate: is there a vehicle ahead? (driver is behind someone, considering overtaking) */
  if (front_dist > 0.0f && front_dist < DNPW_FRONT_THRESHOLD)
  {
    DNPW_FrontVehicle = 1;
  }
  else
  {
    DNPW_FrontVehicle = 0;
  }

  /* Reset DSRC accumulators */
  DNPW_OppositeDetected = 0;
  DNPW_WorstRisk        = RISK_SAFE;
}

/**
 * @brief Process one DSRC neighbor for DNPW
 *
 * Only opposite-direction neighbors matter for DNPW:
 *   - If heading is opposite → oncoming traffic
 *   - Compute relative_speed = my_speed + other_speed (closing speeds add)
 *   - Compute TTC using front distance (space needed to pass)
 *   - Track worst risk level across all opposite neighbors
 *
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 */
void DNPW_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  /* Only opposite-direction vehicles are an overtaking threat */
  if (dir != DIR_OPPOSITE)
  {
    return;
  }

  /* Mark that at least one opposite vehicle exists */
  DNPW_OppositeDetected = 1;

  /* Calculate closing speed (opposite = speeds add up) */
  float rel_speed = Host_Speed + n->speed;

  if (rel_speed > 0.0f && DNPW_FrontDist > 0.0f)
  {
    float ttc = SafetyEngine_CalcTTC(DNPW_FrontDist, rel_speed);
    RiskLevel_t level = SafetyEngine_EvaluateRisk(ttc, DNPW_WARNING_TTC, DNPW_CRITICAL_TTC);

    /* Track worst risk across all opposite neighbors (conservative) */
    if (level > DNPW_WorstRisk)
    {
      DNPW_WorstRisk = level;
    }
  }
}

/**
 * @brief End cycle — apply 3-gate decision logic
 *
 * DNPW alert triggers ONLY when ALL three conditions are true:
 *   1. Front vehicle exists  (you're behind someone, considering overtaking)
 *   2. Opposite vehicle detected  (oncoming traffic via DSRC)
 *   3. TTC risk > SAFE  (not enough time/distance to safely pass)
 *
 * This conservative approach prevents false alarms:
 *   - No car ahead → no reason to overtake → no warning
 *   - No oncoming car → safe to pass → no warning
 *   - Plenty of time → safe to pass → no warning
 */
void DNPW_voidEndCycle(void)
{
  /* 3-Gate Decision */
  if (DNPW_FrontVehicle && DNPW_OppositeDetected && (DNPW_WorstRisk > RISK_SAFE))
  {
    /* Update flag for DSRC broadcast */
    DNPW_CurrentFlag = (uint8_t)DNPW_WorstRisk;

    /* Activate alert */
    DNPW_ActivateAlert(DNPW_WorstRisk);
  }
  else
  {
    /* All clear — safe to pass or no overtaking scenario */
    DNPW_CurrentFlag = 0;
    DNPW_DeactivateAlert();
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone DNPW update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void DNPW_voidUpdate(void)
{
  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();
  float front_dist = US_Distances[US_FRONT];

  DNPW_voidBeginCycle(front_dist);

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
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

/**
 * @brief Activate DNPW alert — warn driver NOT to overtake
 */
static void DNPW_ActivateAlert(RiskLevel_t level)
{
#if DNPW_ENABLE_LED_ALERT
  /* LED_DNPW_ON(); */
  /* LCD_Print("! DO NOT PASS - Oncoming vehicle"); */
#endif

#if DNPW_ENABLE_BUZZER
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
 * @brief Deactivate DNPW alert — safe to pass
 */
static void DNPW_DeactivateAlert(void)
{
#if DNPW_ENABLE_LED_ALERT
  /* LED_DNPW_OFF(); */
#endif

#if DNPW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
