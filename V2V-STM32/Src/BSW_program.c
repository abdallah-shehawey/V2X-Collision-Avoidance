/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    BSW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : BSW (Blind Spot Warning)                        **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/BSW/BSW_config.h"
#include "../Inc/Application/BSW/BSW_interface.h"
#include "../Inc/Application/BSW/BSW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/System/System.h"

/* ============ Module State ============ */

/* Flags for DSRC broadcast */
static uint8_t BSW_LeftFlag = 0;
static uint8_t BSW_RightFlag = 0;

/* Cycle accumulators */
static uint8_t BSW_SameDirectionExists = 0; /* DSRC: at least one same-dir neighbor */
static float BSW_LeftDist = 0.0f;           /* Side US distances (from SafetyEngine) */
static float BSW_RightDist = 0.0f;

/* ============ Init ============ */
void BSW_voidInit(void)
{
  BSW_LeftFlag = 0;
  BSW_RightFlag = 0;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new BSW processing cycle
 *
 *  Save side US distances and reset DSRC accumulator.
 *
 * @param left_dist   Min of front-left and rear-left US distance (cm)
 * @param right_dist  Min of front-right and rear-right US distance (cm)
 */
void BSW_voidBeginCycle(float left_dist, float right_dist)
{
  /* Save distances for EndCycle */
  BSW_LeftDist = left_dist;
  BSW_RightDist = right_dist;

  /* Reset DSRC accumulator */
  BSW_SameDirectionExists = 0;
}

/**
 * @brief Process one DSRC neighbor for BSW
 *
 * Step 1 (DSRC gate): Is there a vehicle going in the same direction?
 *        If yes → set flag. BSW will check US in EndCycle.
 *        If not same direction → ignore (not a blind-spot threat).
 */
void BSW_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  (void)n; /* vehicle_id not used currently */

  /* Only same-direction vehicles are blind-spot candidates */
  if (dir == DIR_SAME)
  {
    BSW_SameDirectionExists = 1;
  }
}

/**
 * @brief End cycle — combine DSRC + US and decide alerts
 *
 * Logic:
 *   1. DSRC says there's a same-direction vehicle nearby? (gate)
 *   2. If yes → check US sensors:
 *      - Left US  < threshold → vehicle on left  → alert left
 *      - Right US < threshold → vehicle on right → alert right
 *   3. If no same-direction vehicle → no blind-spot risk → clear alerts
 */
void BSW_voidEndCycle(void)
{
  /* Reset flags */
  BSW_LeftFlag = 0;
  BSW_RightFlag = 0;

  /* Gate: no same-direction vehicle from DSRC → no blind-spot risk */
  if (!BSW_SameDirectionExists)
  {
    BSW_DeactivateAlert(BSW_LEFT);
    BSW_DeactivateAlert(BSW_RIGHT);
    return;
  }

  /*
   * DSRC confirmed a same-direction vehicle exists.
   * Now check US sensors to determine WHICH side.
   */

  /* -------- LEFT SIDE -------- */
  if (BSW_LeftDist > 0.0f && BSW_LeftDist < BSW_SIDE_THRESHOLD)
  {
    BSW_LeftFlag = 1;
    BSW_ActivateAlert(BSW_LEFT);
  }
  else
  {
    BSW_DeactivateAlert(BSW_LEFT);
  }

  /* -------- RIGHT SIDE -------- */
  if (BSW_RightDist > 0.0f && BSW_RightDist < BSW_SIDE_THRESHOLD)
  {
    BSW_RightFlag = 1;
    BSW_ActivateAlert(BSW_RIGHT);
  }
  else
  {
    BSW_DeactivateAlert(BSW_RIGHT);
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone BSW update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void BSW_voidUpdate(void)
{
  Neighbor *table = DSRC_GetTable();
  uint8_t count = DSRC_GetCount();

  /* Compute min side distances (same logic as SafetyEngine) */
  float left_dist = G_stHostVehicleState.FrontLeftUS;
  float right_dist = G_stHostVehicleState.FrontRightUS;
  float left_rear = G_stHostVehicleState.BackLeftUS;
  float right_rear = G_stHostVehicleState.BackRightUS;
  if (left_rear < left_dist)
  {
    left_dist = left_rear;
  }
  if (right_rear < right_dist)
  {
    right_dist = right_rear;
  }

  BSW_voidBeginCycle(left_dist, right_dist);

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(G_stHostVehicleState.Heading, table[i].heading);
    BSW_voidProcessNeighbor(&table[i], dir);
  }

  BSW_voidEndCycle();
}

/* ============ Public Getters ============ */
uint8_t BSW_u8GetLeftFlag(void)
{
  return BSW_LeftFlag;
}

uint8_t BSW_u8GetRightFlag(void)
{
  return BSW_RightFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Activate blind-spot alert on specified side
 */
static void BSW_ActivateAlert(uint8_t side)
{
#if BSW_ENABLE_LED_ALERT
  if (side == BSW_LEFT)
  {
    /* LED_LEFT_ON(); */
    /* LCD_Print("! Vehicle on LEFT - Don't change lane"); */
  }
  else if (side == BSW_RIGHT)
  {
    /* LED_RIGHT_ON(); */
    /* LCD_Print("! Vehicle on RIGHT - Don't change lane"); */
  }
  else
  {
  }
#endif

#if BSW_ENABLE_BUZZER
  /* BUZZER_SHORT_BEEP(); */
#endif
}

/**
 * @brief Deactivate blind-spot alert on specified side
 */
static void BSW_DeactivateAlert(uint8_t side)
{
#if BSW_ENABLE_LED_ALERT
  if (side == BSW_LEFT)
  {
    /* LED_LEFT_OFF(); */
  }
  else
  {
    /* LED_RIGHT_OFF(); */
  }
#endif

#if BSW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
