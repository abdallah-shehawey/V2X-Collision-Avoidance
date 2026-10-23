/**
 ******************************************************************************
 * @file    IMA_config.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Compile-time configuration for the IMA driver — Intersection Movement Assist.
 * @ingroup app_ima
 ******************************************************************************
 */

#ifndef IMA_CONFIG_H
#define IMA_CONFIG_H

/* Priority rule only (higher speed = right of way); no distance/delay tuning.
 * Alerts (LED/buzzer) are handled outside this module — it only computes the
 * flag, exposed via IMA_u8GetFlag(). */

#endif
