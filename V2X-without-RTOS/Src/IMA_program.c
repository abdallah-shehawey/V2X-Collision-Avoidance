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

#include "../Inc/IMA/IMA_interface.h"
#include "../Inc/IMA/IMA_config.h"
#include "../Inc/IMA/IMA_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State (cycle accumulators) ============ */
static uint8_t     IMA_CrossingDetected = 0; /* At least one crossing-dir neighbor near intersection */
static uint8_t     IMA_IShouldWait      = 0; /* Host vehicle should yield (other is faster) */
static RiskLevel_t IMA_WorstRisk        = RISK_SAFE; /* Worst risk across all crossing neighbors */

/* ============ Init ============ */
void IMA_voidInit(void)
{
  IMA_CrossingDetected = 0;
  IMA_IShouldWait      = 0;
  IMA_WorstRisk        = RISK_SAFE;
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
 * The faster vehicle has priority and passes first; if the host is the slower
 * one it must yield, and its time-to-intersection grades the risk. Only
 * crossing neighbors with both vehicles near the intersection are considered.
 *
 * @param n Pointer to neighbor data
 */
void IMA_voidProcessNeighbor(const Neighbor *n)
{
  /* Both vehicles must be near the intersection. */
  if (Host_DistToIntersection <= 0.0f || Host_DistToIntersection > IMA_INTERSECTION_RANGE)
  {
    return;
  }
  if (n->distance_to_intersection <= 0.0f || n->distance_to_intersection > IMA_INTERSECTION_RANGE)
  {
    return;
  }

  IMA_CrossingDetected = 1;

  /* Faster vehicle passes first: if the host is faster, it has priority and
   * needs no warning. */
  if (Host_Speed > n->speed)
  {
    return;
  }

  /* Host must yield. A stopped host can't collide; otherwise grade the risk by
   * its time-to-intersection and keep the worst across crossing neighbors. */
  IMA_IShouldWait = 1;

  if (Host_Speed <= 0.0f)
  {
    return;
  }

  float delay = Host_DistToIntersection / Host_Speed;
  RiskLevel_t level = SafetyEngine_EvaluateRisk(delay, IMA_WARNING_DELAY, IMA_CRITICAL_DELAY);

  if (level > IMA_WorstRisk)
  {
    IMA_WorstRisk = level;
  }
}

/* ============ Public Getter ============ */

/**
 * @brief Current IMA risk level. Warns only when a crossing vehicle is near,
 *        the host must yield, and the approach is risky.
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t IMA_u8GetFlag(void)
{
  if (IMA_CrossingDetected && IMA_IShouldWait && IMA_WorstRisk > RISK_SAFE)
  {
    return (uint8_t)IMA_WorstRisk;
  }
  return 0;
}