/**
 ******************************************************************************
 * @file    SCB_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the SCB driver — not a public header.
 * @ingroup mcal_scb
 ******************************************************************************
 */

#ifndef SCB_PRIVATE_H_
#define SCB_PRIVATE_H_

/**
 * @brief The key that must be written into the top half of `AIRCR` for the write to take effect.
 * @note A write to `AIRCR` without this key is silently ignored. It is a guard
 *       against a stray pointer accidentally triggering a system reset.
 */
#define AIRCR_VECTKEY 0xFA05 << 16

#define SCB_MAX_PRIORITY_GROUPING 7 /**< Largest valid priority-group value; higher means fewer preemption levels. */


#endif /* SCB_PRIVATE_H_ */
