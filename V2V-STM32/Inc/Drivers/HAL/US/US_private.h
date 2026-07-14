/**
 ******************************************************************************
 * @file    US_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the US driver — not a public header.
 * @ingroup hal_us
 ******************************************************************************
 */

#ifndef US_PRIVATE_H_
#define US_PRIVATE_H_

/**
 * @brief Fire the trigger pulse that starts one sensor's measurement.
 *
 * Holds TRIG low for @ref US_TRIG_SETTLE_US, then high for @ref US_TRIG_PULSE_US.
 *
 * @param[in] pxSensor The sensor to trigger.
 */
static void US_vSendTrigger(const US_Config_t *pxSensor);

#endif /* US_PRIVATE_H_ */
