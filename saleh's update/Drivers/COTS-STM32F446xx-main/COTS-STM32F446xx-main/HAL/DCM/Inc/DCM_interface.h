/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    DCM_interface.h         >>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : DC_MOTOR                                        **
 **                                                                           **
 **===========================================================================**
 */
#ifndef DCMOTOR_INTERFACE_H_
#define DCMOTOR_INTERFACE_H_

#define DCMOTOR_CW   0
#define DCMOTOR_CCW  1
#define DCMOTOR_STOP 2

typedef struct
{
	GPIO_Port_t Port; /* GPIO Port for both pins */
	GPIO_Pin_t PinA;	/* Motor control pin A */
	GPIO_Pin_t PinB;	/* Motor control pin B */
} DCMOTOR_Config_t;

/* Function to initialize DC Motor pins */
ErrorState_t DCMOTOR_Init(const DCMOTOR_Config_t *Config);

/* Function to control DC Motor direction */
ErrorState_t DCMOTOR_Control(const DCMOTOR_Config_t *Config, uint8_t State);

#endif /* DCMOTOR_INTERFACE_H_ */
