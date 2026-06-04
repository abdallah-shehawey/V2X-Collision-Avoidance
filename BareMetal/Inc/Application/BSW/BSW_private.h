#ifndef BSW_PRIVATE_H
#define BSW_PRIVATE_H
#include <stdint.h>

/* Side definition */
#define BSW_LEFT  0
#define BSW_RIGHT 1

/* Internal functions */
static void BSW_ActivateAlert(uint8_t side);
static void BSW_DeactivateAlert(uint8_t side);

#endif
