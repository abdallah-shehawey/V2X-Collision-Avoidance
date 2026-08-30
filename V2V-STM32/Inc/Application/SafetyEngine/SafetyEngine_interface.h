/**
 ******************************************************************************
 * @file    SafetyEngine_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the SafetyEngine driver — the shared risk model and per-cycle ADAS orchestration.
 * @ingroup app_safetyengine
 ******************************************************************************
 */

#ifndef SAFETYENGINE_INTERFACE_H
#define SAFETYENGINE_INTERFACE_H

/**
 * @addtogroup app_safetyengine
 * @{
 */

/**
 * @name Direction classification thresholds
 *
 * How far two vehicles' headings may differ and still count as travelling the
 * same way, opposite ways, or across each other [degrees].
 *
 * @note These were widened from 30° to 40° deliberately. Each car reports the
 *       heading *its own* magnetometer measured, and two magnetometers rarely
 *       agree to better than a few tens of degrees. A tight threshold therefore
 *       kept classifying genuinely same-direction pairs as @ref DIR_UNKNOWN,
 *       which silently switched every cooperative module off.
 * @{
 */
#define HEADING_SAME_THRESHOLD     (40.0f) /**< Headings within this of each other → @ref DIR_SAME. */
#define HEADING_OPPOSITE_THRESHOLD (40.0f) /**< Headings within this of 180° apart → @ref DIR_OPPOSITE. */
#define HEADING_CROSS_THRESHOLD    (40.0f) /**< Headings within this of 90° apart → @ref DIR_CROSSING. */
/** @} */

/**
 * @brief How a neighbor is moving relative to us — the first question every
 *        cooperative module asks.
 *
 * It decides which module even applies: EEBL only makes sense for a car ahead in
 * @ref DIR_SAME, Do-Not-Pass only for @ref DIR_OPPOSITE, IMA only for
 * @ref DIR_CROSSING.
 */
typedef enum
{
  DIR_SAME,     /**< Travelling the same way we are — a car ahead or behind in our lane. */
  DIR_OPPOSITE, /**< Coming towards us — oncoming traffic. */
  DIR_CROSSING, /**< Crossing our path at roughly a right angle — the intersection case. */
  DIR_UNKNOWN   /**< The headings do not fit any of the above within the thresholds. Modules skip the neighbor rather than guess. */
} Direction_t;

/**
 * @brief The graded verdict every ADAS module returns.
 *
 * These map straight onto the 2-bit fields of @ref G_u16SystemFlags — @ref SYS_SAFE,
 * @ref SYS_WARNING and @ref SYS_CRITICAL have the same numeric values.
 */
typedef enum
{
  RISK_SAFE = 0, /**< No hazard. */
  RISK_WARNING,  /**< Getting close — the driver should slow down. */
  RISK_CRITICAL  /**< Too close — the driver should stop. */
} RiskLevel_t;

/**
 * @name Shared safe-distance model
 *
 * The one place the firmware decides how much room is "enough". Both distance-based
 * modules — local FCW and EEBL — use it, so a change here moves both, and the two
 * can never drift out of agreement.
 *
 * The gap scales with speed, because stopping distance does:
 *
 * @verbatim
 *   safe_cm     = max(Host_Speed[m/s] * SAFE_DIST_PER_MS, MIN_SAFE_DISTANCE)
 *   critical_cm = safe_cm * CRITICAL_RATIO
 * @endverbatim
 *
 * @note The constants are sized for the **prototype**, not a real car: a small
 *       vehicle whose top speed is around 0.5 m/s, operating in corridors where
 *       obstacles sit tens of centimetres away. At that scale the speed term is
 *       small and @ref MIN_SAFE_DISTANCE is what actually governs — it is the
 *       dominant floor, not a rarely-hit corner case.
 * @{
 */
#define SAFE_DIST_PER_MS  (40.0f)  /**< Centimetres of safe gap per 1 m/s of speed. */
#define MIN_SAFE_DISTANCE (30.0f)  /**< Floor on the safe gap [cm], whatever the speed. */
#define CRITICAL_RATIO    (0.667f) /**< Fraction of the safe gap below which the risk is critical. */
/** @} */

/**
 * @name Host vehicle state for the current cycle
 *
 * @ref SafetyEngine_voidUpdate latches these from @ref G_stHostVehicleState once
 * at the top of each 50 ms cycle, and the modules then read them as they walk the
 * neighbor table. Latching once is what guarantees every module in a given cycle
 * reasons about the *same* vehicle state, even though the sensors keep updating.
 * @{
 */
extern float Host_Speed;   /**< This vehicle's speed for this cycle [m/s]. */
extern float Host_Heading; /**< This vehicle's heading for this cycle [degrees], 0..360. */

extern float SafetyEngine_SafeDist;     /**< The safe gap for this cycle [cm], from the model above. Read-only to the modules. */
extern float SafetyEngine_CriticalDist; /**< The critical gap for this cycle [cm]. Read-only to the modules. */
/** @} */

/* ====== Public API ====== */

/**
 * @brief Initialise all five ADAS modules (FCW/DNPW, EEBL, BSW, IMA).
 *
 * Called from `main()` before the scheduler starts, so no task can ever observe a
 * module in a half-initialised state.
 */
void SafetyEngine_voidInit(void);

/**
 * @brief Run one full ADAS cycle: every module, over every neighbor, in one pass.
 *
 * This is the ADAS brain, and it is the only place a hazard decision is made.
 * Each 50 ms cycle it:
 *
 * 1. latches @ref Host_Speed and @ref Host_Heading, and computes this cycle's
 *    @ref SafetyEngine_SafeDist and @ref SafetyEngine_CriticalDist;
 * 2. walks the DSRC neighbor table **once**, offering each neighbor to every
 *    module that its @ref Direction_t makes relevant;
 * 3. aggregates the modules' @ref RiskLevel_t results into @ref G_u16SystemFlags.
 *
 * The single pass matters: the modules share the neighbor table and the latched
 * host state, so walking it once means they all reason about one coherent snapshot
 * of the world rather than five slightly different ones.
 *
 * @note Makes no actuator decision whatsoever. It publishes a status word;
 *       `vTask_Feedback` is what turns that into light and sound.
 * @warning The caller must hold **both** mutexes, taken in the order
 *          NeighborTable → Data.
 */
void SafetyEngine_voidUpdate(void);

/**
 * @brief Detect direction relationship between two vehicles
 * @param my_heading     Host vehicle heading (0-360)
 * @param other_heading  Neighbor vehicle heading (0-360)
 * @return DIR_SAME, DIR_OPPOSITE, DIR_CROSSING, or DIR_UNKNOWN
 */
Direction_t SafetyEngine_DetectDirection(float my_heading, float other_heading);

/**
 * @brief Evaluate risk from a "lower value = higher risk" metric.
 *        Used by IMA for time-gap/delay thresholds.
 * @param value        Metric to evaluate (e.g. delay/time-gap in seconds)
 * @param warning_thr  Threshold for warning level
 * @param critical_thr Threshold for critical level
 * @return RISK_SAFE, RISK_WARNING, or RISK_CRITICAL
 */
RiskLevel_t SafetyEngine_EvaluateRisk(float value, float warning_thr, float critical_thr);

/**
 * @brief Assess collision risk from host speed and a measured distance.
 *
 *   safe_dist_cm  = host_speed(m/s) * dist_per_ms   (floored at min_dist)
 *   distance >= safe_dist               -> RISK_SAFE
 *   crit*safe <= distance < safe_dist   -> RISK_WARNING
 *   distance < crit*safe                -> RISK_CRITICAL
 *
 * @param host_speed   Host vehicle speed (m/s, from MPU)
 * @param distance     Measured distance to the object (cm, from ultrasonic)
 * @param dist_per_ms  cm of safe gap per 1 m/s of host speed (module-tuned)
 * @param min_dist     Minimum safe-distance floor (cm)
 * @param crit_ratio   Fraction of safe distance below which risk is CRITICAL
 * @return RISK_SAFE, RISK_WARNING, or RISK_CRITICAL
 */
RiskLevel_t SafetyEngine_AssessDistanceRisk(float host_speed, float distance,
                                            float dist_per_ms, float min_dist,
                                            float crit_ratio);

/** @} */ /* end of app_safetyengine */

#endif
