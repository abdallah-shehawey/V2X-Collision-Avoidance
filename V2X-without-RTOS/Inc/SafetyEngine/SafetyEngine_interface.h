#ifndef SAFETYENGINE_INTERFACE_H
#define SAFETYENGINE_INTERFACE_H

/**
 * @brief Initialize all safety modules (FCW, EEBL, etc.)
 */
void SafetyEngine_voidInit(void);

/**
 * @brief Single-pass update over the DSRC neighbor table
 *        Processes FCW + EEBL (and future modules) in one loop.
 *        Call this in the main loop instead of calling each module separately.
 */
void SafetyEngine_voidUpdate(void);

#endif
