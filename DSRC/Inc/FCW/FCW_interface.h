#ifndef FCW_INTERFACE_H
#define FCW_INTERFACE_H

#include <stdint.h>

/**
 * @brief Initialize the FCW module
 */
void FCW_voidInit(void);

/**
 * @brief Main FCW update - call in main loop
 *        Reads DSRC neighbor table + ultrasonic data,
 *        evaluates all neighbors, sets fcw_flag
 */
void FCW_voidUpdate(void);

/**
 * @brief Get current FCW risk level (for DSRC flag broadcast)
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t FCW_u8GetFlag(void);

#endif
