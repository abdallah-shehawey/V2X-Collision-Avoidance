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


#include "System/System.h"       /* For DSRC/V2X message structure if defined there */
#include "../Inc/Drivers/HAL/BUZZ/BUZ_interface.h"
#include "../Inc/Drivers/HAL/LED/LED_interface.h"

extern volatile float G_fSpeed;                 /* Host vehicle speed from MPU9250 */
extern volatile uint16_t G_u16DistCenter;       /* Front Center distance from Ultrasonic */
extern V2X_Message_t G_stIncomingV2XMsg;        /* Incoming message from DSRC for Front Vehicle Speed */

extern BUZ_Config_t V2X_Buzzer;
extern LED_Config_t TaskLed; /* Example LED mapping; could be FrontRightLed, etc. */


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
      FCW_voidSendWarning(FCW_CRITICAL);
    }
    else if (ttc <= FCW_WARNING_TTC)
    {
      FCW_voidActivateAlert(FCW_WARNING);
      FCW_voidSendWarning(FCW_WARNING);
    }
  }
}

/* Calculate Time To Collision (TTC) using Real Data */
static float FCW_f32CalculateTTC(void)
{
  /* Fetch real speeds: Host Speed vs Target/Front vehicle speed */
  /* Speed from MPU9250 is typically in m/s */
  float host_speed = G_fSpeed;
  
  /* Assuming the incoming V2X message is from the vehicle directly ahead */
  float front_speed = G_stIncomingV2XMsg.Speed_ms;  
  
  float relativeSpeed = host_speed - front_speed;

  /* No collision risk if we are slower or at the same speed as the front vehicle */
  if (relativeSpeed <= 0.0f)
  {
    return -1.0f; 
  }

  /* Fetch real distance from Ultrasonic */
  /* G_u16DistCenter is likely in cm. Convert to meters for TTC formula */
  float distance_meters = (float)G_u16DistCenter / 100.0f;

  return (distance_meters / relativeSpeed);
}

/* HardWare Alerts based on Risk Level */
static void FCW_voidActivateAlert(FCW_RiskLevel_t level)
{
#if FCW_ENABLE_LED_ALERT
  if (level == FCW_CRITICAL || level == FCW_WARNING)
  {
     /* Activate Front LEDs (e.g. PC0 and PC1 mapped in System.h) */
     /* For demonstration: Toggle or Turn On an initialized LED */
<<<<<<< HEAD
	  LED_TurnOn(&TaskLed);
=======
     LED_TurnOn(&TaskLed);
>>>>>>> b3d88525aeefcab7ecbf8e9d2ee9390d185b4581
  }
#endif

#if FCW_ENABLE_BUZZER
  if (level == FCW_CRITICAL)
  {
     BUZ_On(&V2X_Buzzer);
  }
#endif

#if FCW_ENABLE_ADAS_REQUEST
  if (level == FCW_CRITICAL)
  {
    /*
      ADAS_RequestBrake(); // AEB hook
    */
  }
#endif
}

/* Broadcast Warning over DSRC */
static void FCW_voidSendWarning(FCW_RiskLevel_t level)
{
  /*
    Construct a new DSRC warning packet:
    WarningPacket.Sender_ID = OUR_ID;
    WarningPacket.Target_ID = BROADCAST;
    WarningPacket.Event     = FCW_EVENT;
    WarningPacket.Severity  = level;
    
    UART_Transmit(&DSRC_UART, WarningPacket);
  */
}
