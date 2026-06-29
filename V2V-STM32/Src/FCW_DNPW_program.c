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

#include "../Inc/Application/FCW_DNPW/FCW_DNPW_interface.h"
#include "../Inc/Application/FCW_DNPW/FCW_DNPW_config.h"
#include "../Inc/Application/FCW_DNPW/FCW_DNPW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

/*
 * Module state — the four results, each ready to read straight from a getter.
 * Inputs (front object + severity) are latched in BeginCycle; the result flags
 * are kept up to date as neighbors arrive, so no getter recomputes anything.
 */
static uint8_t     FCW_DNPW_FcwObject = 0;         /* object inside the FCW front gate (40 cm, wider)    */
static uint8_t     FCW_DNPW_FrontObject = 0;       /* object inside the DNPW/head-on front gate (20 cm)  */
static uint8_t     FCW_DNPW_FrontRightNear = 0;    /* car alongside on the right (front-right gate)      */
static RiskLevel_t FCW_DNPW_FrontSeverity = RISK_SAFE;  /* same-direction severity, latched per cycle     */
static RiskLevel_t FCW_DNPW_HeadonSeverity = RISK_SAFE; /* head-on severity: gaps doubled for closing speed */

static RiskLevel_t FCW_DNPW_FrontFlag = RISK_SAFE; /* [result] forward collision, same lane (0/1/2)    */
static uint8_t     FCW_DNPW_HeadonFlag = 0;        /* [result] head-on candidate, broadcast (0/1)       */
static uint8_t     FCW_DNPW_HeadonConfirmed = 0;   /* [result] confirmed head-on severity (0/1/2)       */
static RiskLevel_t FCW_DNPW_DnpwFlag = RISK_SAFE;  /* [result] do-not-pass severity (0/1/2)             */

/* ============ Init ============ */
void FCW_DNPW_voidInit(void)
{
  FCW_DNPW_FcwObject = 0;
  FCW_DNPW_FrontObject = 0;
  FCW_DNPW_FrontRightNear = 0;
  FCW_DNPW_FrontSeverity = RISK_SAFE;
  FCW_DNPW_HeadonSeverity = RISK_SAFE;
  FCW_DNPW_FrontFlag = RISK_SAFE;
  FCW_DNPW_HeadonFlag = 0;
  FCW_DNPW_HeadonConfirmed = 0;
  FCW_DNPW_DnpwFlag = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Start a new cycle: latch the front distances and reset the signals.
 * @param front_distance       Front-center ultrasonic distance (cm)
 * @param front_right_distance Front-right ultrasonic distance (cm)
 */
void FCW_DNPW_voidBeginCycle(float front_distance, float front_right_distance)
{
  /* Two front gates (not yet known to be a vehicle — a neighbor confirms later):
   *   - FCW gate (wider, 40 cm) feeds the same-lane forward collision: warn earlier.
   *   - DNPW/head-on gate (nearer, 20 cm) feeds the oncoming/overtaking case. */
  FCW_DNPW_FcwObject   = (front_distance > 0.0f && front_distance < FCW_FRONT_THRESHOLD)  ? 1U : 0U;
  FCW_DNPW_FrontObject = (front_distance > 0.0f && front_distance < DNPW_FRONT_THRESHOLD) ? 1U : 0U;

  /* A car alongside on the right (overtaking side) escalates a DNPW to CRITICAL. */
  FCW_DNPW_FrontRightNear =
      (front_right_distance > 0.0f && front_right_distance < DNPW_FRONT_RIGHT_CRITICAL) ? 1U : 0U;

  /* Latch the distance-based severity once. Two severities: a same-direction
   * collision closes at the speed difference, but a head-on closes at the sum of
   * both speeds (~double), so the head-on gaps are doubled to warn at twice the
   * distance. The same-direction severity uses the wider FCW gate; the head-on
   * severity uses the nearer DNPW gate. */
  FCW_DNPW_FrontSeverity = RISK_SAFE;
  FCW_DNPW_HeadonSeverity = RISK_SAFE;

  /* Same-direction (FCW gate): closing at the speed difference, plain gaps. */
  if (FCW_DNPW_FcwObject && front_distance < SafetyEngine_SafeDist)
  {
    FCW_DNPW_FrontSeverity = (front_distance < SafetyEngine_CriticalDist) ? RISK_CRITICAL : RISK_WARNING;
  }

  /* Head-on (DNPW gate): closing at the sum of both speeds (~double), doubled gaps. */
  if (FCW_DNPW_FrontObject && front_distance < SafetyEngine_SafeDist * 2.0f)
  {
    FCW_DNPW_HeadonSeverity = (front_distance < SafetyEngine_CriticalDist * 2.0f) ? RISK_CRITICAL : RISK_WARNING;
  }

  /* Reset the results for the new cycle. */
  FCW_DNPW_FrontFlag = RISK_SAFE;
  FCW_DNPW_HeadonFlag = 0;
  FCW_DNPW_HeadonConfirmed = 0;
  FCW_DNPW_DnpwFlag = RISK_SAFE;
}

/**
 * @brief A same-direction neighbor confirms the object ahead is a vehicle, so the
 *        latched front-distance severity becomes a real front-collision flag.
 *        The severity is the same for every same-direction neighbor (it depends
 *        only on the front distance), so a plain assignment is enough.
 */
void FCW_DNPW_voidProcessSameDirection(void)
{
  FCW_DNPW_FrontFlag = FCW_DNPW_FrontSeverity;
}

/**
 * @brief An oncoming neighbor exists. With an object ahead, that makes a head-on
 *        candidate (broadcast for confirmation). Whether the oncoming car raised
 *        its own head-on flag splits the case:
 *          - it did  → both face the same obstacle: confirmed head-on (severity).
 *          - it did not → it is in another lane: a do-not-pass / overtaking risk.
 *        All three results are settled here, ready for the getters to return.
 * @param n Pointer to neighbor data
 */
void FCW_DNPW_voidProcessOppositeDirection(const Neighbor *n)
{
  if (!FCW_DNPW_FrontObject)
  {
    return; /* no object ahead — oncoming car alone is not our hazard */
  }

  FCW_DNPW_HeadonFlag = 1; /* candidate: object ahead + oncoming present */

  if (n->fcw_headon_flag > 0)
  {
    /* Same obstacle: a real head-on overrides any do-not-pass seen this cycle.
     * Use the head-on severity (doubled gaps) for the higher closing speed. */
    FCW_DNPW_HeadonConfirmed = (uint8_t)FCW_DNPW_HeadonSeverity;
    FCW_DNPW_DnpwFlag = RISK_SAFE;
  }
  else if (!FCW_DNPW_HeadonConfirmed)
  {
    /* Oncoming is in another lane: overtaking risk. WARNING by default, CRITICAL
     * when the front-right sensor reads a near object (a car alongside). */
    FCW_DNPW_DnpwFlag = FCW_DNPW_FrontRightNear ? RISK_CRITICAL : RISK_WARNING;
  }
}

/* ============ Public Getters (return the ready results) ============ */

/** @brief Forward collision, same lane. @return 0=Safe, 1=Warning, 2=Critical */
uint8_t FCW_GetFrontFlag(void)      { return (uint8_t)FCW_DNPW_FrontFlag; }

/** @brief Head-on candidate, broadcast over DSRC. @return 0/1 */
uint8_t FCW_GetHeadonFlag(void)     { return FCW_DNPW_HeadonFlag; }

/** @brief Confirmed head-on collision. @return 0=Safe, else severity (1/2) */
uint8_t FCW_GetHeadonConfirmed(void) { return FCW_DNPW_HeadonConfirmed; }

/** @brief Do-not-pass (oncoming in another lane). @return 0=Safe, 1=Warning, 2=Critical */
uint8_t DNPW_GetFlag(void)          { return (uint8_t)FCW_DNPW_DnpwFlag; }
