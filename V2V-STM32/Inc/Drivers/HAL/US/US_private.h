/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<     US_private.h      >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Saleh                                  **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : US (Ultrasonic Distance Sensor)                 **
 **                                                                           **
 **===========================================================================**
 */

#ifndef US_PRIVATE_H_
#define US_PRIVATE_H_

/* Private helper: send the HC-SR04 trigger pulse on the TRIG pin
 * (US_TRIG_SETTLE_US low, then US_TRIG_PULSE_US high). */
static void US_vSendTrigger(const US_Config_t *pxSensor);

#endif /* US_PRIVATE_H_ */
