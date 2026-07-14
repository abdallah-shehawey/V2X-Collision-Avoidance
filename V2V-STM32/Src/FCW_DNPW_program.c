/**
 ******************************************************************************
 * @file    FCW_DNPW_program.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Implementation of the FCW/DNPW driver — Forward Collision Warning and Do-Not-Pass Warning.
 * @ingroup app_fcw_dnpw
 ******************************************************************************
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
/** @brief Is something inside the wider FCW front gate, @ref FCW_FRONT_THRESHOLD? (Local, ultrasonic-only.) */
static uint8_t FCW_DNPW_FcwObject = 0;
/** @brief Is something inside the tighter DNPW / head-on front gate, @ref DNPW_FRONT_THRESHOLD? */
static uint8_t FCW_DNPW_FrontObject = 0;
/** @brief Is a car alongside us on the overtaking (left) side? Escalates DNPW to critical — see @ref DNPW_FRONT_LEFT_CRITICAL. */
static uint8_t FCW_DNPW_FrontLeftNear = 0;
/** @brief Severity for a same-direction vehicle ahead, latched for this cycle. */
static RiskLevel_t FCW_DNPW_FrontSeverity = RISK_SAFE;
/**
 * @brief Severity for a head-on encounter.
 * @note The safe gaps are **doubled** here. Two cars approaching each other close
 *       at the sum of their speeds, so the same physical distance buys only half
 *       the time it would against a stationary obstacle.
 */
static RiskLevel_t FCW_DNPW_HeadonSeverity = RISK_SAFE;

/** @brief Result: forward-collision risk in our own lane. */
static RiskLevel_t FCW_DNPW_FrontFlag = RISK_SAFE;
/** @brief Result: head-on candidate, broadcast to the other car as @ref Neighbor::fcw_headon_flag so it can confirm from its own side. */
static uint8_t FCW_DNPW_HeadonFlag = 0;
/** @brief Result: a head-on that **both** cars independently flagged — our own geometry agrees with the neighbor's broadcast @ref Neighbor::fcw_headon_flag. */
static uint8_t FCW_DNPW_HeadonConfirmed = 0;
/** @brief Result: do-not-pass severity — an oncoming car in another lane makes overtaking unsafe. */
static RiskLevel_t FCW_DNPW_DnpwFlag = RISK_SAFE;

/* ============ Init ============ */
void FCW_DNPW_voidInit(void)
{
  FCW_DNPW_FcwObject = 0;
  FCW_DNPW_FrontObject = 0;
  FCW_DNPW_FrontLeftNear = 0;
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

/*
 * @brief Start a new cycle: latch the front distances and reset the signals.
 * @param front_distance      Front-center ultrasonic distance (cm)
 * @param front_left_distance Front-left ultrasonic distance (cm)
 */
void FCW_DNPW_voidBeginCycle(float front_distance, float front_left_distance)
{
  /* Two front gates (not yet known to be a vehicle — a neighbor confirms later):
   *   - FCW gate (wider, 40 cm) feeds the same-lane forward collision: warn earlier.
   *   - DNPW/head-on gate (nearer, 20 cm) feeds the oncoming/overtaking case. */
  FCW_DNPW_FcwObject = (front_distance > 0.0f && front_distance < FCW_FRONT_THRESHOLD) ? 1U : 0U;
  FCW_DNPW_FrontObject = (front_distance > 0.0f && front_distance < DNPW_FRONT_THRESHOLD) ? 1U : 0U;

  /* The oncoming car alongside on the LEFT (overtaking side) escalates a DNPW to
   * CRITICAL: to pass we pull out left, so the car we must not pass is on our left. */
  FCW_DNPW_FrontLeftNear = (front_left_distance > 0.0f && front_left_distance < DNPW_FRONT_LEFT_CRITICAL) ? 1U : 0U;

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

/*
 * @brief A same-direction neighbor confirms the object ahead is a vehicle, so the
 *        latched front-distance severity becomes a real front-collision flag.
 *        The severity is the same for every same-direction neighbor (it depends
 *        only on the front distance), so a plain assignment is enough.
 */
void FCW_DNPW_voidProcessSameDirection(void)
{
  FCW_DNPW_FrontFlag = FCW_DNPW_FrontSeverity;
}

/*
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
     * when the front-left sensor reads a near object (the oncoming car alongside
     * on the overtaking side). */
    FCW_DNPW_DnpwFlag = FCW_DNPW_FrontLeftNear ? RISK_CRITICAL : RISK_WARNING;
  }
}

/* ============ Public Getters (return the ready results) ============ */

/** @brief Forward collision, same lane. @return 0=Safe, 1=Warning, 2=Critical */
uint8_t FCW_GetFrontFlag(void) { return (uint8_t)FCW_DNPW_FrontFlag; }

/** @brief Head-on candidate, broadcast over DSRC. @return 0/1 */
uint8_t FCW_GetHeadonFlag(void) { return FCW_DNPW_HeadonFlag; }

/** @brief Confirmed head-on collision. @return 0=Safe, else severity (1/2) */
uint8_t FCW_GetHeadonConfirmed(void) { return FCW_DNPW_HeadonConfirmed; }

/** @brief Do-not-pass (oncoming in another lane). @return 0=Safe, 1=Warning, 2=Critical */
uint8_t DNPW_GetFlag(void) { return (uint8_t)FCW_DNPW_DnpwFlag; }
