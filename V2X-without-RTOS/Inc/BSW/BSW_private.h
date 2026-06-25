#ifndef BSW_PRIVATE_H
#define BSW_PRIVATE_H
#include <stdint.h>

/* Side definition (also used as the receiver-side alert index) */
#define BSW_LEFT  0
#define BSW_RIGHT 1

/* bsw_flag values carried in the DSRC message (sender's own side) */
#define BSW_FLAG_NONE  0
#define BSW_FLAG_LEFT  1
#define BSW_FLAG_RIGHT 2

/* Internal functions */
static void BSW_ActivateAlert(uint8_t side);
static void BSW_DeactivateAlert(uint8_t side);

#endif
