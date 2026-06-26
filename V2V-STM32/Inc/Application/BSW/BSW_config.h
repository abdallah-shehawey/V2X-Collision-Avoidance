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

/* Object present if a side ultrasonic reads < this distance (cm) */
#define BSW_SIDE_THRESHOLD (80.0f)

/* Alerts (LED/buzzer) are handled outside this module — it only computes the
 * sender flag and the receiver-side blind-spot result, exposed via
 * BSW_u8GetFlag() and BSW_u8GetBlindSpot(). */

#endif
