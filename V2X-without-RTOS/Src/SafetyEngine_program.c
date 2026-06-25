/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<    SafetyEngine_program.c   >>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : V2X Safety Engine (Single-Pass)                 **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/FCW/FCW_interface.h"
#include "../Inc/EEBL/EEBL_interface.h"
#include "../Inc/BSW/BSW_interface.h"
#include "../Inc/DNPW/DNPW_interface.h"
#include "../Inc/IMA/IMA_interface.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"

float US_Distances[US_SENSOR_COUNT];
float Host_Speed   = 0.0f;
float Host_Heading = 0.0f;
float Host_DistToIntersection = 0.0f;

/* ============ Init ============ */
void SafetyEngine_voidInit(void)
{
  FCW_voidInit();
  EEBL_voidInit();
  BSW_voidInit();
  DNPW_voidInit();
  IMA_voidInit();
}

/* ============ Single-Pass Update ============ */
/*
 * Flow:
 *   1. BeginCycle → each module resets its accumulators
 *   2. For each neighbor → call ProcessNeighbor on all modules
 *   3. EndCycle → each module finalizes (set flags, activate alerts)
 */
void SafetyEngine_voidUpdate(void)
{
  /* Host_Speed, Host_Heading, US_Distances → shared globals (System.h) */

  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();

  /* Read all US distances ONCE per cycle */
  float front_dist  = US_Distances[US_FRONT];
  float rear_dist   = US_Distances[US_REAR];
  float front_left  = US_Distances[US_FRONT_LEFT];
  float front_right = US_Distances[US_FRONT_RIGHT];
  float rear_left   = US_Distances[US_REAR_LEFT];
  float rear_right  = US_Distances[US_REAR_RIGHT];

  /* Read host vehicle data (from sensors/modules) */
  /* HostSpeed   = ...get from speedometer... */
  /* HostHeading = ...get from compass/IMU...  */


  /* 1. Begin cycle for all modules */
  FCW_voidBeginCycle(front_dist);
  EEBL_voidBeginCycle();
  BSW_voidBeginCycle(front_left, front_right, rear_left, rear_right);
  DNPW_voidBeginCycle(front_dist);
  IMA_voidBeginCycle();

  /* 2. Single pass over neighbor table */
  for (uint8_t i = 0; i < count; i++)
  {
    /* Compute direction ONCE per neighbor — shared by all modules */
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);

    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
    EEBL_voidProcessNeighbor(&table[i], rear_dist, dir);
    BSW_voidProcessNeighbor(&table[i], dir);
    DNPW_voidProcessNeighbor(&table[i], dir);
    IMA_voidProcessNeighbor(&table[i], dir);
  }

  /* 3. End cycle for all modules */
  FCW_voidEndCycle();
  EEBL_voidEndCycle();
  BSW_voidEndCycle();
  DNPW_voidEndCycle();
  IMA_voidEndCycle();
}

/* ============ Shared Direction Detection ============ */

/**
 * @brief Calculate absolute heading difference, normalized to [0, 180]
 */
static float CalcHeadingDiff(float h1, float h2)
{
  float diff = h1 - h2;

  if (diff > 180.0f)
  {
    diff -= 360.0f;
  }
  if (diff < -180.0f)
  {
    diff += 360.0f;
  }

  return (diff < 0.0f) ? -diff : diff;
}

Direction_t SafetyEngine_DetectDirection(float my_heading, float other_heading)
{
  float diff = CalcHeadingDiff(my_heading, other_heading);

  if (diff <= HEADING_SAME_THRESHOLD)
  {
    return DIR_SAME;
  }

  if (diff >= (180.0f - HEADING_OPPOSITE_THRESHOLD))
  {
    return DIR_OPPOSITE;
  }

  if (diff >= (90.0f - HEADING_CROSS_THRESHOLD) && diff <= (90.0f + HEADING_CROSS_THRESHOLD))
  {
    return DIR_CROSSING;
  }

  return DIR_UNKNOWN;
}

/* ============ Shared Threshold Risk Evaluation ============ */
/*
 * Generic "lower value = higher risk" evaluator.
 * Still used by IMA for time-gap/delay thresholds. FCW/EEBL/DNPW have
 * moved to the distance-based model below and no longer call this.
 */
RiskLevel_t SafetyEngine_EvaluateRisk(float value, float warning_thr, float critical_thr)
{
  if (value < 0.0f)
  {
    return RISK_SAFE;
  }

  if (value <= critical_thr)
  {
    return RISK_CRITICAL;
  }

  if (value <= warning_thr)
  {
    return RISK_WARNING;
  }

  return RISK_SAFE;
}

/* ============ Shared Distance-Based Risk Assessment ============ */

RiskLevel_t SafetyEngine_AssessDistanceRisk(float host_speed, float distance,
                                            float dist_per_ms, float min_dist,
                                            float crit_ratio)
{
  /* No valid distance reading → nothing in range → safe */
  if (distance <= 0.0f)
  {
    return RISK_SAFE;
  }

  /* Speed-dependent safe gap, floored so a near-stopped car still has a gap */
  float safe_dist = host_speed * dist_per_ms;
  if (safe_dist < min_dist)
  {
    safe_dist = min_dist;
  }

  float critical_dist = safe_dist * crit_ratio;

  if (distance < critical_dist)
  {
    return RISK_CRITICAL;
  }

  if (distance < safe_dist)
  {
    return RISK_WARNING;
  }

  return RISK_SAFE;
}
