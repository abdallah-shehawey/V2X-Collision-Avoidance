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

/* ============ Module State (cycle accumulators) ============ */
static uint8_t     IMA_CrossingDetected = 0; /* At least one crossing-dir neighbor */
static uint8_t     IMA_IShouldWait      = 0; /* Other vehicle is faster → I yield   */
static RiskLevel_t IMA_WorstRisk        = RISK_SAFE;

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
 * @brief Begin a new IMA processing cycle — reset accumulators.
 *        IMA reads Host_Speed from the shared globals.
 */
void IMA_voidBeginCycle(void)
{
  IMA_CrossingDetected = 0;
  IMA_IShouldWait      = 0;
  IMA_WorstRisk        = RISK_SAFE;
}

/**
 * @brief Process one crossing-direction DSRC neighbor for IMA.
 *
 * Priority rule: higher speed = right of way. If the other vehicle is faster (or
 * equal) the host yields → WARNING. If the host is faster it passes first → no
 * alert. The SafetyEngine only dispatches crossing-direction neighbors here.
 *
 * @param n Pointer to neighbor data
 */
void IMA_voidProcessNeighbor(const Neighbor *n)
{
  /* A crossing vehicle is detected */
  IMA_CrossingDetected = 1;

  /* If the host is faster, it has priority and needs no warning. */
  if (Host_Speed > n->speed)
  {
    return;
  }

  /* Other is faster or equal → the host should yield. */
  IMA_IShouldWait = 1;

  if (RISK_WARNING > IMA_WorstRisk)
  {
    IMA_WorstRisk = RISK_WARNING;
  }
}

/* ============ Public Getter ============ */

/**
 * @brief Current IMA flag. Warns only when a crossing vehicle is near and the
 *        host must yield.
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
