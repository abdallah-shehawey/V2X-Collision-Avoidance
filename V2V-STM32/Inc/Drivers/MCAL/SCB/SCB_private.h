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
 * @warning This constant is unused today — nothing in the firmware currently
 *          calls @ref SCB_vSetPriorityGrouping (the FreeRTOS port's own
 *          `vInitPrioGroupValue()` sets it instead) — but if it ever is
 *          called, note that per the ARMv7-M architecture reference manual
 *          the value an `AIRCR` *write* must supply is `0x5FA` in bits
 *          [31:16]; `0xFA05` is what `VECTKEYSTAT` reads back *after* a
 *          successful write, not what a write should send. Left as-is rather
 *          than changed as a drive-by fix in an unrelated change — see
 *          @ref SCB_AIRCR_RESET_VECTKEY below, defined separately and
 *          correctly, for the one place in the firmware that actually writes
 *          `AIRCR` (the FOTA system-reset call).
 */
#define AIRCR_VECTKEY 0xFA05 << 16

#define SCB_MAX_PRIORITY_GROUPING 7 /**< Largest valid priority-group value; higher means fewer preemption levels. */

/**
 * @name AIRCR reset fields
 * Used by @ref SCB_vSystemReset only. Defined independently of
 * @ref AIRCR_VECTKEY (see the warning above) so this one is correct
 * regardless of that one.
 * @{
 */
#define SCB_AIRCR_RESET_VECTKEY (0x5FAUL << 16) /**< The write key AIRCR actually requires, per the ARMv7-M ARM. */
#define SCB_AIRCR_SYSRESETREQ   (1UL << 2)      /**< Request a system reset. */
#define SCB_AIRCR_PRIGROUP_MASK (0x7UL << 8)    /**< PRIGROUP field — preserved across the reset write so it isn't clobbered. */
/** @} */

#endif /* SCB_PRIVATE_H_ */
