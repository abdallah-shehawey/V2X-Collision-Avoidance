#ifndef BSW_CONFIG_H
#define BSW_CONFIG_H

/*
 * ====== Cooperative Blind-Spot Model ======
 * SENDER:   uses its FRONT-side ultrasonics (front-left / front-right) to
 *           detect a car alongside-ahead of it, and broadcasts bsw_flag
 *           (1=LEFT, 2=RIGHT) for the side it saw.
 * RECEIVER: for each neighbor whose bsw_flag is set, checks the OPPOSITE
 *           REAR side (sender LEFT -> my rear-right, sender RIGHT -> my
 *           rear-left). If something is there, this car is the one sitting
 *           in the sender's blind spot -> raise BSW warning on that side.
 */

/* Object present if a side ultrasonic reads < this distance (cm) */
#define BSW_SIDE_THRESHOLD (150.0f)

/* ====== Alerts ====== */
#define BSW_ENABLE_LED_ALERT 1
#define BSW_ENABLE_BUZZER    1

#endif
