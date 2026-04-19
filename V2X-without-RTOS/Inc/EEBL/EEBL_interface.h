#ifndef EEBL_INTERFACE_H
#define EEBL_INTERFACE_H

#include <stdint.h>

/**
 * @brief Initialize the EEBL module
 */
void EEBL_voidInit(void);

/**
 * @brief Main EEBL update — call in main loop
 *        Detects sudden braking, checks rear sensor + DSRC neighbors,
 *        calculates TTC, and activates local alert if needed.
 *        (Local-only: no flag broadcast via DSRC)
 */
void EEBL_voidUpdate(void);

#endif
