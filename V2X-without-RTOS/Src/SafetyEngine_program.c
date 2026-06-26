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
#include "../Inc/FCW_DNPW/FCW_DNPW_interface.h"
#include "../Inc/EEBL/EEBL_interface.h"
#include "../Inc/BSW/BSW_interface.h"
#include "../Inc/IMA/IMA_interface.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"

float US_Distances[US_SENSOR_COUNT];
float Host_Speed   = 0.0f;
float Host_Heading = 0.0f;

/* Speed-dependent safe/critical gaps (cm) for the current cycle, shared by the
 * distance-based modules (Local FCW, EEBL). See SafetyEngine_interface.h. */
float SafetyEngine_SafeDist     = 0.0f;
float SafetyEngine_CriticalDist = 0.0f;

/* ============ Init ============ */
void SafetyEngine_voidInit(void)
{
  FCW_DNPW_voidInit();
  EEBL_voidInit();
  BSW_voidInit();
  IMA_voidInit();
}

/* ============ Single-Pass Update ============ */
/*
 * One pass over the neighbor table:
 *   1. BeginCycle → each module resets its per-cycle state
 *   2. For each neighbor → dispatch to the modules its direction affects
 * Results are then read on demand via each module's getters.
 */
void SafetyEngine_voidUpdate(void)
{
  Neighbor *table = DSRC_GetTable();
  uint8_t count   = DSRC_GetCount();

  /* Host_Speed, Host_Heading and US_Distances are shared globals (System.h). */
  float front_dist  = US_Distances[US_FRONT];
  float rear_dist   = US_Distances[US_REAR];
  float front_left  = US_Distances[US_FRONT_LEFT];
  float front_right = US_Distances[US_FRONT_RIGHT];
  float rear_left   = US_Distances[US_REAR_LEFT];
  float rear_right  = US_Distances[US_REAR_RIGHT];

  /* Speed-dependent safe/critical gaps for this cycle, floored at the minimum. */
  SafetyEngine_SafeDist = Host_Speed * SAFE_DIST_PER_MS;
  if (SafetyEngine_SafeDist < MIN_SAFE_DISTANCE)
  {
    SafetyEngine_SafeDist = MIN_SAFE_DISTANCE;
  }
  SafetyEngine_CriticalDist = SafetyEngine_SafeDist * CRITICAL_RATIO;

  /* 1. Begin cycle */
  FCW_DNPW_voidBeginCycle(front_dist);
  EEBL_voidBeginCycle();
  BSW_voidBeginCycle(front_left, front_right, rear_left, rear_right);
  IMA_voidBeginCycle();

  /* 2. Dispatch each neighbor only to the modules its direction can affect. */
  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);

    if (dir == DIR_SAME)
    {
      /* Same lane ahead/behind: forward collision, rear brake, blind spot. */
      FCW_DNPW_voidProcessSameDirection();
      EEBL_voidProcessNeighbor(rear_dist);
      BSW_voidProcessNeighbor(&table[i]);
    }
    else if (dir == DIR_OPPOSITE)
    {
      /* Oncoming traffic: head-on FCW vs do-not-pass. */
      FCW_DNPW_voidProcessOppositeDirection(&table[i]);
    }
    else if (dir == DIR_CROSSING)
    {
      /* Crossing traffic at an intersection. */
      IMA_voidProcessNeighbor(&table[i]);
    }
  }

  /* All modules derive their results on demand via getters — no end-cycle step. */
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
 * Generic "lower value = higher risk" evaluator, used by IMA for time-gap and
 * delay thresholds.
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
