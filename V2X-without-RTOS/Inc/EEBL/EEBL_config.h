#ifndef EEBL_CONFIG_H
#define EEBL_CONFIG_H

/*
 * EEBL detects a sudden brake, then rates the rear gap using the shared cycle
 * safe/critical distances (SafetyEngine_SafeDist / CriticalDist). The only
 * EEBL-specific tuning is the braking threshold below.
 *
 * Host_Speed is in m/s (from the MPU driver); distances are in cm. Numbers are
 * tuned for the prototype's 0..5 m/s range — re-tune on the real car.
 */

/*
 * Sudden-braking threshold (m/s per cycle, negative = braking): the drop in
 * speed between two consecutive cycles. At ~5 m/s top speed, a -0.20 m/s step
 * is a clear braking event. Smaller magnitude = more sensitive.
 */
#define EEBL_DECEL_THRESHOLD (-0.20f)

/* Alerts (LED/buzzer) are handled outside this module — it only computes the
 * risk level, exposed via EEBL_u8GetFlag(). */

#endif
