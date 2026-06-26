/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<   FCW_DNPW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : Cooperative FCW + DNPW                          **
 **                                                                           **
 **===========================================================================**
 */

/*
 * Cooperative Forward Collision Warning (FCW) and Do-Not-Pass Warning (DNPW).
 * Both are derived from the same per-cycle signals, so they share one module.
 *
 * The SafetyEngine feeds the module by direction during the neighbor pass; the
 * three flags are then derived on demand by the getters:
 *
 *   fcw_front_flag  : forward collision with a vehicle ahead in the same lane.
 *                     Local, severity WARNING/CRITICAL from the front distance.
 *
 *   fcw_headon_flag : head-on candidate — a vehicle ahead and an oncoming
 *                     vehicle both present. Broadcast over DSRC so the oncoming
 *                     car can confirm. Boolean.
 *
 *   dnpw_flag       : the oncoming car has no head-on candidate of its own, so
 *                     it is in another lane — an overtaking risk, not head-on.
 *
 * Severity comes from the front distance against the cycle safe/critical gaps
 * (SafetyEngine_SafeDist / SafetyEngine_CriticalDist).
 */

#include "../Inc/FCW_DNPW/FCW_DNPW_interface.h"
#include "../Inc/FCW_DNPW/FCW_DNPW_config.h"
#include "../Inc/FCW_DNPW/FCW_DNPW_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State (per-cycle signals) ============ */
static float FCW_DNPW_FrontDist = 0.0f;            /* front ultrasonic distance (cm)                 */
static uint8_t FCW_DNPW_FrontObject = 0;           /* ultrasonic sees an object within the front gate */
static RiskLevel_t FCW_DNPW_FrontFlag = RISK_SAFE; /* front-collision severity (set per same-dir car) */
static uint8_t FCW_DNPW_Oncoming = 0;              /* an opposite-direction (oncoming) neighbor exists */
static uint8_t FCW_DNPW_OncomingHeadon = 0;        /* that oncoming neighbor raised its head-on flag   */

/* Classify the front distance against the cycle safe/critical gaps. */
static RiskLevel_t FCW_DNPW_FrontSeverity(void)
{
  float d = FCW_DNPW_FrontDist;

  if (d <= 0.0f)
  {
    return RISK_SAFE; /* no valid reading */
  }
  if (d < SafetyEngine_CriticalDist)
  {
    return RISK_CRITICAL;
  }
  if (d < SafetyEngine_SafeDist)
  {
    return RISK_WARNING;
  }
  return RISK_SAFE;
}

/* ============ Init ============ */
void FCW_DNPW_voidInit(void)
{
  FCW_DNPW_FrontDist = 0.0f;
  FCW_DNPW_FrontObject = 0;
  FCW_DNPW_FrontFlag = RISK_SAFE;
  FCW_DNPW_Oncoming = 0;
  FCW_DNPW_OncomingHeadon = 0;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Start a new cycle: latch the front distance and reset the signals.
 * @param front_distance Front ultrasonic distance (cm)
 */
void FCW_DNPW_voidBeginCycle(float front_distance)
{
  FCW_DNPW_FrontDist = front_distance;

  /* Ultrasonic sees an object within the fixed front gate (not yet known to be
   * a vehicle — a same-direction neighbor confirms that later). */
  FCW_DNPW_FrontObject = (front_distance > 0.0f && front_distance < FCW_DNPW_FRONT_THRESHOLD) ? 1U : 0U;

  FCW_DNPW_FrontFlag = RISK_SAFE;
  FCW_DNPW_Oncoming = 0;
  FCW_DNPW_OncomingHeadon = 0;
}

/**
 * @brief A same-direction neighbor confirms a vehicle is ahead. Classify the
 *        front distance now and keep the worst severity across the cycle, so
 *        the front flag is ready to read directly.
 */
void FCW_DNPW_voidProcessSameDirection(void)
{
  RiskLevel_t sev = FCW_DNPW_FrontSeverity();
  if (sev > FCW_DNPW_FrontFlag)
  {
    FCW_DNPW_FrontFlag = sev;
  }
}

/**
 * @brief Record an oncoming neighbor and whether it reports its own head-on
 *        candidate. A matching candidate means both face the same obstacle
 *        (genuine head-on); otherwise the oncoming car is in another lane.
 * @param n Pointer to neighbor data
 */
void FCW_DNPW_voidProcessOppositeDirection(const Neighbor *n)
{
  FCW_DNPW_Oncoming = 1;

  if (n->fcw_headon_flag > 0)
  {
    FCW_DNPW_OncomingHeadon = 1;
  }
}

/* ============ Public Getters (derive results on demand) ============ */

/**
 * @brief Forward-collision flag — front-distance severity for a vehicle ahead
 *        in the same lane. Already computed in ProcessSameDirection, so this
 *        just returns the ready value (RISK_SAFE if no same-direction vehicle).
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t FCW_GetFrontFlag(void)
{
  return (uint8_t)FCW_DNPW_FrontFlag;
}

/**
 * @brief Head-on candidate — a vehicle ahead and an oncoming vehicle present.
 *        Broadcast over DSRC for the oncoming car to confirm.
 * @return 0=no candidate, 1=candidate
 */
uint8_t FCW_GetHeadonFlag(void)
{
  return (FCW_DNPW_FrontObject && FCW_DNPW_Oncoming) ? 1U : 0U;
}

/**
 * @brief Confirmed head-on collision — our candidate and the oncoming car's
 *        candidate both hold.
 * @return 0=Safe, else front-distance severity (1=Warning, 2=Critical)
 */
uint8_t FCW_GetHeadonConfirmed(void)
{
  if (FCW_GetHeadonFlag() && FCW_DNPW_OncomingHeadon)
  {
    return (uint8_t)FCW_DNPW_FrontSeverity();
  }
  return 0;
}

/**
 * @brief Do-Not-Pass flag — a head-on candidate while the oncoming car has none
 *        of its own (it is in another lane: an overtaking risk). This is a
 *        presence signal only: the front distance measures whatever is ahead in
 *        our lane, not the oncoming car, so there is no meaningful severity.
 * @return 0=no warning, 1=do not pass
 */
uint8_t DNPW_GetFlag(void)
{
  return (FCW_GetHeadonFlag() && !FCW_DNPW_OncomingHeadon) ? 1U : 0U;
}
