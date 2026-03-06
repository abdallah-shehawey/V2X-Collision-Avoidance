#ifndef SDW_PRIVATE_H
#define SDW_PRIVATE_H

typedef enum
{
  SDW_SAFE = 0,
  SDW_WARNING,
  SDW_CRITICAL
} SDW_RiskLevel_t;

/* Internal */
static void SDW_voidCheckDirection(float distance, unsigned char direction);

static void SDW_voidActivateAlert(SDW_RiskLevel_t level, unsigned char direction);

static float SDW_f32CalculateSafeDistance(void);

/* Directions */
#define SDW_FRONT 0
#define SDW_REAR 1
#define SDW_LEFT 2
#define SDW_RIGHT 3

#endif
