/**
 ******************************************************************************
 * @file    BSW_config.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Compile-time configuration for the BSW driver — Blind Spot Warning.
 * @ingroup app_bsw
 ******************************************************************************
 */

#ifndef BSW_CONFIG_H
#define BSW_CONFIG_H

/*
 * ====== Cooperative Blind-Spot Model ======
 * SENDER:   uses its FRONT-side ultrasonics (front-left / front-right) to
 *           detect a car alongside-ahead of it, and broadcasts bsw_flag as a
 *           bitmask (bit0=LEFT, bit1=RIGHT) of the side(s) it saw.
 * RECEIVER: for each side bit set, checks the OPPOSITE REAR sensor (sender
 *           LEFT -> my rear-right, sender RIGHT -> my rear-left). If something
 *           is there, this car sits in the sender's blind spot -> raise BSW on
 *           that side. Both bits are handled independently.
 */

/**
 * @brief A side ultrasonic reading below this means a vehicle is there [cm].
 *
 * This is the **warning** band: a neighbor inside this distance raises a BSW
 * warning. Tuned for the prototype — a small car in a tight corridor.
 */
#define BSW_SIDE_THRESHOLD (60.0f)

/**
 * @brief Closer than this escalates the blind-spot alert to critical [cm].
 *
 * @warning Must be **strictly less than** @ref BSW_SIDE_THRESHOLD. The two carve
 *          the range into `[CRITICAL, THRESHOLD)` = warning and `[0, CRITICAL)` =
 *          critical; making this the larger of the two collapses the warning band
 *          to nothing and the module would only ever report critical or safe.
 */
#define BSW_SIDE_CRITICAL  (40.0f)

/* Alerts (LED/buzzer) are handled outside this module — it only computes the
 * sender flag and the receiver-side blind-spot result, exposed via
 * BSW_u8GetFlag(), BSW_u8GetBlindSpot() (side bitmask) and BSW_u8GetSeverity()
 * (0=safe/1=warning/2=critical). */

#endif
