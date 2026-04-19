#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
/* Number of ultrasonic sensors */
#define US_SENSOR_COUNT 6

/* Ultrasonic sensor indices */
#define US_FRONT       0
#define US_FRONT_RIGHT 1
#define US_FRONT_LEFT  2
#define US_REAR        3
#define US_REAR_RIGHT  4
#define US_REAR_LEFT   5

/* ====== Shared Host Vehicle Data ====== */
/* Defined in System.c — updated by sensor drivers, read by all safety modules */
extern float US_Distances[US_SENSOR_COUNT];
extern float Host_Speed;    /* Current speed (m/s)        */
extern float Host_Heading;  /* Current heading (0-360°)   */

/* ====== Shared Direction Detection ====== */
/* Heading threshold for direction classification (degrees) */
#define HEADING_SAME_THRESHOLD     (30.0f)
#define HEADING_OPPOSITE_THRESHOLD (30.0f)

typedef enum
{
  DIR_SAME,
  DIR_OPPOSITE,
  DIR_UNKNOWN
} Direction_t;

/* ====== Shared TTC & Risk Evaluation ====== */
typedef enum
{
  RISK_SAFE = 0,
  RISK_WARNING,
  RISK_CRITICAL
} RiskLevel_t;

// ====== Forward Declaration ======
void USART_RXCMP(void);

// ====== Public API ======
void System_Init(void);

/**
 * @brief Detect direction relationship between two vehicles
 * @param my_heading     Host vehicle heading (0-360)
 * @param other_heading  Neighbor vehicle heading (0-360)
 * @return DIR_SAME, DIR_OPPOSITE, or DIR_UNKNOWN
 */
Direction_t System_DetectDirection(float my_heading, float other_heading);

/**
 * @brief Calculate Time To Collision
 * @param distance        Distance to obstacle (cm)
 * @param relative_speed  Relative closing speed
 * @return TTC in seconds, or -1.0 if no collision risk
 */
float System_CalcTTC(float distance, float relative_speed);

/**
 * @brief Evaluate risk level based on TTC value
 * @param ttc          Time to collision (seconds)
 * @param warning_ttc  TTC threshold for warning level
 * @param critical_ttc TTC threshold for critical level
 * @return RISK_SAFE, RISK_WARNING, or RISK_CRITICAL
 */
RiskLevel_t System_EvaluateRisk(float ttc, float warning_ttc, float critical_ttc);

#endif // SYSTEM_H
