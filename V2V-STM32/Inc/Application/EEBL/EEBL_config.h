/**
 ******************************************************************************
 * @file    EEBL_config.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Compile-time configuration for the EEBL driver — Electronic Emergency Brake Light.
 * @ingroup app_eebl
 ******************************************************************************
 */

#ifndef EEBL_CONFIG_H
#define EEBL_CONFIG_H

/*
 * EEBL detects a sudden brake, then rates the rear gap using the shared cycle
 * safe/critical distances (SafetyEngine_SafeDist / CriticalDist). The only
 * EEBL-specific tuning is the braking threshold below.
 *
 * Host_Speed is in m/s (the SafetyEngine converts G_stHostVehicleState.Speed
 * from cm/s to m/s once per cycle); distances are in cm. Numbers are tuned for
 * the prototype's 0..5 m/s range — re-tune on the real car.
 */

/**
 * @brief How hard a neighbor must brake before EEBL calls it an emergency
 *        [m/s of speed lost per cycle].
 *
 * Negative by definition — it is a *drop* in speed between two consecutive
 * cycles. At the prototype's ~5 m/s top speed, losing 0.20 m/s in one cycle is
 * unambiguous braking rather than noise.
 *
 * @note Moving it closer to zero makes the module more sensitive, at the cost of
 *       firing on ordinary speed fluctuation.
 */
#define EEBL_DECEL_THRESHOLD (-0.20f)

/* Alerts (LED/buzzer) are handled outside this module — it only computes the
 * risk level, exposed via EEBL_u8GetFlag(). */

#endif
