/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    FCW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : FCW                                             **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/FCW/FCW_config.h"
#include "../Inc/Application/FCW/FCW_private.h"
#include "../Inc/Application/DSRC/DSRC.h"

#include "System/System.h"       /* For DSRC/V2X message structure if defined there */
#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"

#include "../Inc/Drivers/HAL/MPU9250/MPU9250_interface.h"


#include "../Inc/Drivers/HAL/L298N/L298N_interface.h"

extern volatile float G_fSpeed;                 /* Host vehicle speed from MPU9250 */
extern volatile uint16_t G_u16DistCenter;       /* Front Center distance from Ultrasonic */
extern volatile float G_fHeading;               /* Host vehicle heading from MPU9250 */

extern volatile uint16_t G_u16DistLeft;         /* Front Left Ultrasonic */
extern volatile uint16_t G_u16DistRight;        /* Front Right Ultrasonic */
extern volatile uint16_t G_u16DistBackCenter;   /* Back Center Ultrasonic */
extern volatile uint16_t G_u16DistBackLeft;     /* Back Left Ultrasonic */
extern volatile uint16_t G_u16DistBackRight;    /* Back Right Ultrasonic */



void FCW_voidInit(void)
{
	/* Any necessary Initialization before the Scheduler runs */
	/* Real data starts feeding in automatically from Sensor tasks */
}

void FCW_voidUpdate(void)
{
	/* Fetch latest data passively and evaluate collision */
	FCW_voidCheckCollision();
}

/* ================= Core Logic ================= */

static void FCW_voidCheckCollision(void)
{
	float ttc = FCW_f32CalculateTTC();

	if (ttc > 0) /* Valid TTC calculated */
	{
		if (ttc <= FCW_CRITICAL_TTC)
		{
			FCW_voidActivateAlert(FCW_CRITICAL);
		}
		else if (ttc <= FCW_WARNING_TTC)
		{
			FCW_voidActivateAlert(FCW_WARNING);
		}
		else
		{
			FCW_voidActivateAlert(FCW_SAFE);
		}
	}
	else
	{
		FCW_voidActivateAlert(FCW_SAFE);
}
/* Calculate Time To Collision (TTC) using real sensor + V2X data */
static float FCW_f32CalculateTTC(void)
{
	/* Our speed from MPU9250 */
	float host_speed = G_fSpeed;

	/* Read front vehicle speed directly from DSRC Neighbor table (V2X) */
	Neighbor *table = DSRC_GetTable();
	uint8_t   count = DSRC_GetCount();

	if (count == 0)
	{
		/* No V2X neighbors → no front vehicle data → assume safe */
		return -1.0f;
	}

	/* Find the most threatening neighbor: highest positive closing speed */
	float front_speed  = table[0].speed;
	float max_closing  = host_speed - front_speed;
	for (uint8_t i = 1; i < count; i++)
	{
		float closing = host_speed - table[i].speed;
		if (closing > max_closing)
		{
			max_closing  = closing;
			front_speed  = table[i].speed;
		}
	}

	/* Relative speed: positive means we are closing in on the front vehicle */
	float relativeSpeed = host_speed - front_speed;

	/* No collision risk if we are slower or same speed as the front vehicle */
	if (relativeSpeed <= 0.0f)
	{
		return -1.0f;
	}

	/* Distance from Ultrasonic (cm → m) */
	float distance_meters = (float)G_u16DistCenter / 100.0f;

	return (distance_meters / relativeSpeed);
}

/* HardWare Alerts based on Risk Level */
static void FCW_voidActivateAlert(FCW_RiskLevel_t level)
{
	if (level == FCW_SAFE )
	{
		G_u8SystemRiskLevel = 0; /* Safe */
		G_eMotorGlobalCommand = CMD_MOVE_FORWARD;
	}
	else if (level == FCW_WARNING)
	{
		G_u8SystemRiskLevel = 1; /* Warning */
		G_eMotorGlobalCommand = CMD_MOVE_FORWARD;
	}
	else if (level == FCW_CRITICAL)
	{
		G_u8SystemRiskLevel = 2; /* Critical */

		if (G_u16DistBackCenter > 100)
		{
			/* No car immediately behind us. Safe to hard brake. */
			G_eMotorGlobalCommand = CMD_STOP;
		}
		else
		{
			/* There is a car right behind us! Braking might cause rear-end collision. Try evasive maneuver. */

			if (G_u16DistRight > 100 && G_u16DistBackRight > 100)
			{
				/* Right lane is totally clear front & back -> Steer Right */
				G_eMotorGlobalCommand = CMD_STEER_RIGHT;
			}
			else if (G_u16DistLeft > 100 && G_u16DistBackLeft > 100)
			{
				/* Right is blocked, but Left lane is totally clear -> Steer Left */
				G_eMotorGlobalCommand = CMD_STEER_LEFT;
			}
			else
			{
				/* We are boxed in (Left and Right are blocked, Rear is blocked)! Hard brake is the only option left. */
				G_eMotorGlobalCommand = CMD_STOP;
			}
		}
	}
	else
	{

	}
}

