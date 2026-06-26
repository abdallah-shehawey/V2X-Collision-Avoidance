#ifndef BSW_PRIVATE_H
#define BSW_PRIVATE_H
#include <stdint.h>

/* bsw_flag bitmask carried in the DSRC message (sender's own side).
 * LEFT and RIGHT are independent bits, so both can be set at once. */
#define BSW_FLAG_NONE  0x00
#define BSW_FLAG_LEFT  0x01
#define BSW_FLAG_RIGHT 0x02
#define BSW_FLAG_BOTH  (BSW_FLAG_LEFT | BSW_FLAG_RIGHT)

/* No module-private functions: the LED/buzzer is driven outside this module. */

#endif
