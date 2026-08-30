/**
 ******************************************************************************
 * @file    EEBL_program.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Implementation of the EEBL driver — Electronic Emergency Brake Light.
 * @ingroup app_eebl
 ******************************************************************************
 */

#include "../Inc/Application/EEBL/EEBL_interface.h"
#include "../Inc/Application/EEBL/EEBL_config.h"
#include "../Inc/Application/EEBL/EEBL_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
/** @brief The neighbor's speed last cycle [m/s]. Braking is detected as the *drop* from this to the current value. */
static float EEBL_PrevSpeed = 0.0f;
/** @brief Gate: did a neighbor ahead brake hard enough this cycle to exceed @ref EEBL_DECEL_THRESHOLD? */
static uint8_t EEBL_BrakingDetected = 0;
/** @brief Result: the worst risk seen across every neighbor this cycle. */
static RiskLevel_t EEBL_WorstLevel = RISK_SAFE;

/* ============ Init ============ */
void EEBL_voidInit(void)
{
  EEBL_PrevSpeed = 0.0f;
  EEBL_BrakingDetected = 0;
  EEBL_WorstLevel = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/*
 * @brief Start a new cycle: set the sudden-braking gate and reset the result.
 *        While not braking, ProcessNeighbor skips every neighbor.
 */
void EEBL_voidBeginCycle(void)
{
  /* Sudden braking = a drop in speed since the last cycle. */
  float decel = Host_Speed - EEBL_PrevSpeed;
  EEBL_BrakingDetected = (decel <= EEBL_DECEL_THRESHOLD) ? 1U : 0U;

  EEBL_PrevSpeed = Host_Speed;
  EEBL_WorstLevel = RISK_SAFE;
}

/*
 * @brief Evaluate one same-direction neighbor against the rear gap.
 *
 * Risk depends on the host's own speed (the cycle safe/critical gaps), not on
 * relative speed: a sudden brake with a vehicle close behind raises a warning.
 * Keeps the worst severity across the cycle.
 *
 * @param rear_distance Rear ultrasonic distance (cm)
 */
void EEBL_voidProcessNeighbor(float rear_distance)
{
  if (!EEBL_BrakingDetected)
  {
    return;
  }

  /* Nothing behind, or beyond the safe gap (which is RISK_SAFE anyway). */
  if (rear_distance <= 0.0f || rear_distance >= SafetyEngine_SafeDist)
  {
    return;
  }

  RiskLevel_t level = (rear_distance < SafetyEngine_CriticalDist) ? RISK_CRITICAL : RISK_WARNING;

  if (level > EEBL_WorstLevel)
  {
    EEBL_WorstLevel = level;
  }
}

/* ============ Public Getter ============ */

/*
 * @brief Get current EEBL risk level (LED/buzzer handled by the caller).
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t EEBL_u8GetFlag(void)
{
  return (uint8_t)EEBL_WorstLevel;
}
