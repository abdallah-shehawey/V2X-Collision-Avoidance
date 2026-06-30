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
static RiskLevel_t IMA_WorstRisk        = RISK_SAFE;

/* ============ Init ============ */
void IMA_voidInit(void)
{
  IMA_CrossingDetected = 0;
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
  IMA_WorstRisk        = RISK_SAFE;
}

/**
 * @brief Process one crossing-direction DSRC neighbor for IMA.
 *
 * Any crossing vehicle raises an alert so the driver knows an IMA case is in
 * play; the priority rule (higher speed = right of way) only sets its severity:
 *   - host faster or equal → it passes first → WARNING.
 *   - host slower          → it must yield   → CRITICAL.
 * The SafetyEngine only dispatches crossing-direction neighbors here.
 *
 * @param n Pointer to neighbor data
 */
void IMA_voidProcessNeighbor(const Neighbor *n)
{
  /* A crossing vehicle is detected */
  IMA_CrossingDetected = 1;

  /* Severity from the priority rule: the slower vehicle yields (CRITICAL), the
   * faster/equal one has right of way and passes first (WARNING). Either way an
   * alert fires so both drivers know an IMA case is in play. */
  RiskLevel_t risk = (Host_Speed < n->speed) ? RISK_CRITICAL  /* host slower → yield */
                                             : RISK_WARNING;  /* host faster/equal → right of way */

  if (risk > IMA_WorstRisk)
  {
    IMA_WorstRisk = risk;
  }
}

/* ============ Public Getter ============ */

/**
 * @brief Current IMA flag. Any crossing vehicle raises an alert; the severity is
 *        WARNING when the host has right of way, CRITICAL when it must yield.
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t IMA_u8GetFlag(void)
{
  if (IMA_CrossingDetected && IMA_WorstRisk > RISK_SAFE)
  {
    return (uint8_t)IMA_WorstRisk;
  }
  return 0;
}
