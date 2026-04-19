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

// ====== Forward Declaration ======
void USART_RXCMP(void);

// ====== Public API ======
void System_Init(void);

#endif // SYSTEM_H
