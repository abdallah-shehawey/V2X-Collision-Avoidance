/**
 ******************************************************************************
 * @file    BSW_program.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Implementation of the BSW driver — Blind Spot Warning.
 * @ingroup app_bsw
 ******************************************************************************
 */

#include "../Inc/Application/BSW/BSW_config.h"
#include "../Inc/Application/BSW/BSW_interface.h"
#include "../Inc/Application/BSW/BSW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/System/System.h"

/* ============ Module State ============ */

/* Sender flag broadcast over DSRC (this car's own side): bit0=LEFT, bit1=RIGHT */
/** @brief What *we* broadcast: which of our own front sides is occupied. See @ref Neighbor::bsw_flag. */
static uint8_t BSW_SenderFlag = BSW_FLAG_NONE;

/* Cycle distances (latched in BeginCycle) */
/** @brief Front-left distance latched at the top of this cycle [cm] — used in the *sender* role. */
static float BSW_FrontLeft  = 0.0f;
/** @brief Front-right distance latched at the top of this cycle [cm] — used in the *sender* role. */
static float BSW_FrontRight = 0.0f;
/** @brief Rear-left distance latched at the top of this cycle [cm] — used in the *receiver* role. */
static float BSW_RearLeft   = 0.0f;
/** @brief Rear-right distance latched at the top of this cycle [cm] — used in the *receiver* role. */
static float BSW_RearRight  = 0.0f;

/* Receiver-side alert severity per side (set during ProcessNeighbor):
 * 0 = safe, 1 = warning (< BSW_SIDE_THRESHOLD), 2 = critical (< BSW_SIDE_CRITICAL). */
/** @brief Result: severity of the blind-spot alert on our LEFT (0 safe, 1 warning, 2 critical). */
static uint8_t BSW_AlertLeft  = 0;
/** @brief Result: severity of the blind-spot alert on our RIGHT (0 safe, 1 warning, 2 critical). */
static uint8_t BSW_AlertRight = 0;

/* Map a measured rear-side distance to a BSW severity level. */
/**
 * @brief Grade one side's distance into a severity.
 * @param dist Distance read by that side's rear sensor [cm].
 * @retval 0 Safe — beyond @ref BSW_SIDE_THRESHOLD.
 * @retval 1 Warning — inside @ref BSW_SIDE_THRESHOLD.
 * @retval 2 Critical — inside @ref BSW_SIDE_CRITICAL.
 */
static uint8_t BSW_u8DistToSeverity(float dist)
{
  if (dist <= 0.0f || dist >= BSW_SIDE_THRESHOLD) return 0;  /* clear / out of band */
  if (dist < BSW_SIDE_CRITICAL)                   return 2;  /* critical            */
  return 1;                                                  /* warning             */
}

/* ============ Init ============ */
void BSW_voidInit(void)
{
  BSW_SenderFlag = BSW_FLAG_NONE;
  BSW_AlertLeft  = 0;
  BSW_AlertRight = 0;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/*
 * @brief Begin a new BSW processing cycle
 *
 *  - Latch the four side distances.
 *  - Sender role: build our bsw_flag from the front-side sensors. Each side is
 *    an independent bit, so a vehicle on both sides sets BSW_FLAG_BOTH.
 *  - Reset the receiver-side alert accumulators.
 */
void BSW_voidBeginCycle(float front_left, float front_right, float rear_left, float rear_right)
{
  BSW_FrontLeft  = front_left;
  BSW_FrontRight = front_right;
  BSW_RearLeft   = rear_left;
  BSW_RearRight  = rear_right;

  /* ---- Sender: front-side detection, one bit per side (no priority) ---- */
  BSW_SenderFlag = BSW_FLAG_NONE;

  if (front_left > 0.0f && front_left < BSW_SIDE_THRESHOLD)
  {
    BSW_SenderFlag |= BSW_FLAG_LEFT;
  }
  if (front_right > 0.0f && front_right < BSW_SIDE_THRESHOLD)
  {
    BSW_SenderFlag |= BSW_FLAG_RIGHT;
  }

  /* ---- Receiver: reset per-side alert accumulators ---- */
  BSW_AlertLeft  = 0;
  BSW_AlertRight = 0;
}

/*
 * @brief Process one DSRC neighbor for BSW (receiver role)
 *
 * A neighbor broadcasts the front side(s) on which it sees a car. That car sits
 * behind the neighbor on the mirrored side, so each side bit maps to our
 * opposite rear sensor:
 *   sender LEFT  (its front-left)  -> check our REAR-RIGHT
 *   sender RIGHT (its front-right) -> check our REAR-LEFT
 * The bits are tested independently, so BSW_FLAG_BOTH runs both checks.
 */
void BSW_voidProcessNeighbor(const Neighbor *n)
{
  /* Severity scales with how close the car behind us is on the mirrored side.
   * Keep the worst across neighbors that map to the same rear sensor. */
  if (n->bsw_flag & BSW_FLAG_LEFT)
  {
    uint8_t sev = BSW_u8DistToSeverity(BSW_RearRight);
    if (sev > BSW_AlertRight) BSW_AlertRight = sev;
  }
  if (n->bsw_flag & BSW_FLAG_RIGHT)
  {
    uint8_t sev = BSW_u8DistToSeverity(BSW_RearLeft);
    if (sev > BSW_AlertLeft) BSW_AlertLeft = sev;
  }
}

/* ============ Public Getters ============ */

/*
 * @brief Get the sender flag to broadcast over DSRC (my own front side(s)).
 * @return bit0 = LEFT, bit1 = RIGHT (0=none, 1=LEFT, 2=RIGHT, 3=both)
 */
uint8_t BSW_u8GetFlag(void)
{
  return BSW_SenderFlag;
}

/*
 * @brief Get the receiver-side blind-spot result for THIS car. The LED/buzzer
 *        is driven elsewhere by the caller — this module only computes which
 *        side(s) have a vehicle in the blind spot.
 * @return bit0 = LEFT, bit1 = RIGHT
 *         (0=none, 1=LEFT, 2=RIGHT, 3=both)
 */
uint8_t BSW_u8GetBlindSpot(void)
{
  return (uint8_t)((BSW_AlertLeft ? 0x01U : 0x00U) | (BSW_AlertRight ? 0x02U : 0x00U));
}

/*
 * @brief Get the receiver-side blind-spot SEVERITY for THIS car — the worst of
 *        the two sides. Distance-graded: closer car behind → higher severity.
 * @return 0 = safe, 1 = warning (< BSW_SIDE_THRESHOLD), 2 = critical (< BSW_SIDE_CRITICAL)
 */
uint8_t BSW_u8GetSeverity(void)
{
  return (BSW_AlertLeft > BSW_AlertRight) ? BSW_AlertLeft : BSW_AlertRight;
}

/*
 * @brief Get the receiver-side blind-spot severity PER SIDE, packed into one
 *        byte so the Raspberry Pi can distinguish LEFT from RIGHT (the worst-of
 *        getter above collapses both sides into a single number).
 * @return (BSW_AlertLeft & 0x3) | ((BSW_AlertRight & 0x3) << 2)
 *         bits 1:0 = LEFT severity (0/1/2), bits 3:2 = RIGHT severity (0/1/2)
 */
uint8_t BSW_u8GetSidesSeverity(void)
{
  return (uint8_t)((BSW_AlertLeft & 0x3U) | ((BSW_AlertRight & 0x3U) << 2));
}
