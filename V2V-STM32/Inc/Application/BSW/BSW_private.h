/**
 ******************************************************************************
 * @file    BSW_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the BSW driver — not a public header.
 * @ingroup app_bsw
 ******************************************************************************
 */

#ifndef BSW_PRIVATE_H
#define BSW_PRIVATE_H
#include <stdint.h>

/* bsw_flag bitmask carried in the DSRC message (sender's own side).
 * LEFT and RIGHT are independent bits, so both can be set at once. */
#define BSW_FLAG_NONE  0x00        /**< Neither side occupied. */
#define BSW_FLAG_LEFT  0x01        /**< Left side occupied (bit 0). */
#define BSW_FLAG_RIGHT 0x02        /**< Right side occupied (bit 1). */
#define BSW_FLAG_BOTH  (BSW_FLAG_LEFT | BSW_FLAG_RIGHT) /**< Both sides occupied — the two bits together. */
/* No module-private functions: the LED/buzzer is driven outside this module. */

#endif
