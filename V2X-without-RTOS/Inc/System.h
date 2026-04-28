#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
/* Number of ultrasonic sensors */
#define US_SENSOR_COUNT 6

/* Ultrasonic sensor indices */
#define US_FRONT       0
#define US_FRONT_RIGHT 1
#define US_FRONT_LEFT  2
#define US_REAR        3
#define US_REAR_RIGHT  4
#define US_REAR_LEFT   5

/* ====== Shared Host Vehicle Data ====== */
/* Defined in System.c — updated by sensor drivers, read by all safety modules */
extern float US_Distances[US_SENSOR_COUNT];
extern float Host_Speed;    /* Current speed (m/s)        */
extern float Host_Heading;  /* Current heading (0-360°)   */
extern float Host_DistToIntersection; /* Distance to nearest intersection (cm) */

// ====== Forward Declaration ======
void USART_RXCMP(void);

// ====== Public API ======
void System_Init(void);

#endif // SYSTEM_H
