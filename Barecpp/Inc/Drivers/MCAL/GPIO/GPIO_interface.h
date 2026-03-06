/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    GPIO_interface.h   >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : GPIO                                            **
 **                                                                           **
 **===========================================================================**
 */

#ifndef GPIO_INTERFACE_H_
#define GPIO_INTERFACE_H_

#include "../../LIB/ErrTypes.h"
#include <stdint.h>


/************************** Port Definitions **************************/
/********************************
 * @port_t enum:
 * @brief: GPIO port selection
 * @param: PORTA, PORTB, PORTC, PORTD, PORTE, PORTF, PORTG, PORTH
 * @return: GPIO port selection
 */
typedef enum {
  GPIO_PORTA = 0, /* GPIO Port A */
  GPIO_PORTB,     /* GPIO Port B */
  GPIO_PORTC,     /* GPIO Port C */
  GPIO_PORTD,     /* GPIO Port D */
  GPIO_PORTE,     /* GPIO Port E */
  GPIO_PORTF,     /* GPIO Port F */
  GPIO_PORTG,     /* GPIO Port G */
  GPIO_PORTH      /* GPIO Port H */
} GPIO_Port_t;

/************************** Pin Definitions **************************/
/********************************
 * @pin_t enum:
 * @brief: GPIO pin selection
 * @param: PIN0, PIN1, PIN2, PIN3, PIN4, PIN5, PIN6, PIN7, PIN8, PIN9, PIN10,
 * PIN11, PIN12, PIN13, PIN14, PIN15
 * @return: GPIO pin selection
 */
typedef enum {
  GPIO_PIN0 = 0, /* GPIO Pin 0 */
  GPIO_PIN1,     /* GPIO Pin 1 */
  GPIO_PIN2,     /* GPIO Pin 2 */
  GPIO_PIN3,     /* GPIO Pin 3 */
  GPIO_PIN4,     /* GPIO Pin 4 */
  GPIO_PIN5,     /* GPIO Pin 5 */
  GPIO_PIN6,     /* GPIO Pin 6 */
  GPIO_PIN7,     /* GPIO Pin 7 */
  GPIO_PIN8,     /* GPIO Pin 8 */
  GPIO_PIN9,     /* GPIO Pin 9 */
  GPIO_PIN10,    /* GPIO Pin 10 */
  GPIO_PIN11,    /* GPIO Pin 11 */
  GPIO_PIN12,    /* GPIO Pin 12 */
  GPIO_PIN13,    /* GPIO Pin 13 */
  GPIO_PIN14,    /* GPIO Pin 14 */
  GPIO_PIN15     /* GPIO Pin 15 */
} GPIO_Pin_t;

/************************** Mode Definitions **************************/
/********************************
 * @Mode_t enum:
 * @brief: GPIO mode selection
 * @param: INPUT, OUTPUT, ALTFN, ANALOG
 * @return: GPIO mode selection
 */
typedef enum {
  GPIO_INPUT = 0, /* Input Mode */
  GPIO_OUTPUT,    /* Output Mode */
  GPIO_ALTFN,     /* Alternate Function Mode */
  GPIO_ANALOG     /* Analog Mode */
} GPIO_Mode_t;

/************************** Output Type Definitions **************************/
/********************************
 * @OutputType_t enum:
 * @brief: GPIO output type selection
 * @param: PUSH_PULL, OPEN_DRAIN
 * @return: GPIO output type selection
 */
typedef enum {
  GPIO_PUSH_PULL = 0, /* Push-Pull Output Type */
  GPIO_OPEN_DRAIN     /* Open-Drain Output Type */
} GPIO_OutputType_t;

/************************** Nibble Type Definitions **************************/
/********************************
 * @NibbleType_t enum:
 * @brief: GPIO nibble type selection
 * @param: LOW_NIBBLE, HIGH_NIBBLE
 * @return: GPIO nibble type selection
 */
typedef enum {
  GPIO_LOW_NIBBLE, /* Low Nibble */
  GPIO_HIGH_NIBBLE /* High Nibble */
} GPIO_NibbleType_t;

/************************** Output Speed Definitions **************************/
/********************************
 * @OutputSpeed_t enum:
 * @brief: GPIO output speed selection
 * @param: LOW_SPEED, MEDIUM_SPEED, HIGH_SPEED, VERY_HIGH_SPEED
 * @return: GPIO output speed selection
 */
typedef enum {
  GPIO_LOW_SPEED = 0,  /* Low Speed */
  GPIO_MEDIUM_SPEED,   /* Medium Speed */
  GPIO_HIGH_SPEED,     /* High Speed */
  GPIO_VERY_HIGH_SPEED /* Very High Speed */
} GPIO_OutputSpeed_t;

/************************** Pull-up/Pull-down Definitions
 * **************************/
/********************************
 * @PullUpDown_t enum:
 * @brief: GPIO pull-up/pull-down selection
 * @param: NO_PULL, PULL_UP, PULL_DOWN
 * @return: GPIO pull-up/pull-down selection
 */
typedef enum {
  GPIO_NO_PULL = 0, /* No Pull-up or Pull-down */
  GPIO_PULL_UP,     /* Pull-up */
  GPIO_PULL_DOWN    /* Pull-down */
} GPIO_PullUpDown_t;

/************************** Alternate Function Definitions
 * **************************/
/********************************
 * @AlternateFunction_t enum:
 * @brief: GPIO alternate function selection
 * @param: AF0, AF1, AF2, AF3, AF4, AF5, AF6, AF7, AF8, AF9, AF10, AF11, AF12,
 * AF13, AF14, AF15
 * @return: GPIO alternate function selection
 */
typedef enum {
  GPIO_AF0 = 0, /* Alternate Function 0 */
  GPIO_AF1,     /* Alternate Function 1 */
  GPIO_AF2,     /* Alternate Function 2 */
  GPIO_AF3,     /* Alternate Function 3 */
  GPIO_AF4,     /* Alternate Function 4 */
  GPIO_AF5,     /* Alternate Function 5 */
  GPIO_AF6,     /* Alternate Function 6 */
  GPIO_AF7,     /* Alternate Function 7 */
  GPIO_AF8,     /* Alternate Function 8 */
  GPIO_AF9,     /* Alternate Function 9 */
  GPIO_AF10,    /* Alternate Function 10 */
  GPIO_AF11,    /* Alternate Function 11 */
  GPIO_AF12,    /* Alternate Function 12 */
  GPIO_AF13,    /* Alternate Function 13 */
  GPIO_AF14,    /* Alternate Function 14 */
  GPIO_AF15     /* Alternate Function 15 */
} GPIO_AlternateFunction_t;

/************************** Pin Value Definitions **************************/
/********************************
 * @PinValue_t enum:
 * @brief: GPIO pin value selection
 * @param: PIN_LOW, PIN_HIGH
 * @return: GPIO pin value selection
 */
typedef enum {
  GPIO_PIN_LOW = 0, /* Pin Low Value */
  GPIO_PIN_HIGH     /* Pin High Value */
} GPIO_PinValue_t;

/************************** Pin Configuration Structure
 * **************************/
/********************************
 * @PinConfig_t struct:
 * @brief: GPIO pin configuration structure
 * @param: Port, PinNum, Mode, Otype, Speed, PullType, AlternateFunction
 * @return: GPIO pin configuration structure
 */
typedef struct {
  GPIO_Port_t Port;           /* GPIO Port Selection */
  GPIO_Pin_t PinNum;          /* GPIO Pin Number */
  GPIO_Mode_t Mode;           /* GPIO Mode */
  GPIO_OutputType_t Otype;    /* Output Type (if configured as output) */
  GPIO_OutputSpeed_t Speed;   /* Output Speed (if configured as output) */
  GPIO_PullUpDown_t PullType; /* Pull-up/Pull-down Configuration */
  GPIO_AlternateFunction_t
      AlternateFunction; /* Alternate Function (if in AF mode) */
} GPIO_PinConfig_t;

/************************** Port Half Definitions **************************/
/********************************
 * @PortHalf_t enum:
 * @brief: GPIO port half selection
 * @param: PORT_FIRST_HALF, PORT_SECOND_HALF
 * @return: GPIO port half selection
 */
typedef enum {
  PORT_FIRST_HALF = 0, /* Pins 0-7 of the GPIO Port */
  PORT_SECOND_HALF     /* Pins 8-15 of the GPIO Port */
} GPIO_PortHalf_t;

/************************** Port Half Configuration Structure
 * **************************/
/********************************
 * @GPIO_Port8BitsConfig_t struct:
 * @brief: GPIO port 8-bits configuration structure
 * @param: Port, StartPin, Mode, Otype, Speed, PullType
 * @return: GPIO port 8-bits configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* Starting Pin Number (PIN0 to PIN12) */
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT, etc.) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_8PinsConfig_t;

/************************** Port 4-Bits Configuration Structure
 * **************************/
/********************************
 * @GPIO_Port4BitsConfig_t struct:
 * @brief: GPIO port 4-bits configuration structure
 * @param: Port, StartPin, Mode, Otype, Speed, PullType
 * @return: GPIO port 4-bits configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* Starting Pin Number (PIN0 to PIN12) */
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_4PinsConfig_t;

/************************** Low Nibble Configuration Structure
 * **************************/
/********************************
 * @GPIO_LowNibbleConfig_t struct:
 * @brief: GPIO port low nibble configuration structure
 * @param: Port, Mode, Otype, Speed, PullType
 * @return: GPIO port low nibble configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* Make it Zero*/
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_LowNibbleConfig_t;

/************************** High Nibble Configuration Structure
 * **************************/
/********************************
 * @GPIO_HighNibbleConfig_t struct:
 * @brief: GPIO port high nibble configuration structure
 * @param: Port, Mode, Otype, Speed, PullType
 * @return: GPIO port high nibble configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* Make it 4*/
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_HighNibbleConfig_t;

/************************** Byte Configuration Structure
 * **************************/
/********************************
 * @GPIO_ByteConfig_t struct:
 * @brief: GPIO port byte configuration structure
 * @param: Port, Mode, Otype, Speed, PullType
 * @return: GPIO port byte configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* Make it Zero*/
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_ByteConfig_t;

/************************** Half Port Configuration Structure
 * **************************/
/********************************
 * @GPIO_PortConfig_t struct:
 * @brief: GPIO port configuration structure
 * @param: Port, Mode, Otype, Speed, PullType
 * @return: GPIO port configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* Make it Zero*/
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_HalfPortConfig_t;

/************************** Port Configuration Structure
 * **************************/
/********************************
 * @GPIO_PortConfig_t struct:
 * @brief: GPIO port configuration structure
 * @param: Port, Mode, Otype, Speed, PullType
 * @return: GPIO port configuration structure
 */
typedef struct {
  GPIO_Port_t Port;    /* GPIO Port Selection (PORTA to PORTH) */
  GPIO_Pin_t StartPin; /* NOTE : Don't Change it */
  GPIO_Mode_t Mode;    /* GPIO Mode Selection (INPUT, OUTPUT) */
  GPIO_OutputType_t
      Otype; /* GPIO Output Type Selection (PUSH_PULL or OPEN_DRAIN) */
  GPIO_OutputSpeed_t
      Speed; /* GPIO Output Speed Selection (LOW_SPEED to VERY_HIGH_SPEED) */
  GPIO_PullUpDown_t
      PullType; /* GPIO Pull Configuration (NO_PULL, PULL_UP or PULL_DOWN) */
} GPIO_PortConfig_t;

/************************** Function Prototypes **************************/
/*
 * @fn     GPIO_enumPinInit
 * @brief : Initializes GPIO pin configuration
 * @param : PinConfig[in]: Pointer to pin configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 * @example
 * GPIO_PinConfig_t PinCfg = {GPIO_PORTA, GPIO_PIN5, GPIO_OUTPUT, GPIO_PUSH_PULL, GPIO_MEDIUM_SPEED, GPIO_NO_PULL};
 * GPIO_enumPinInit(&PinCfg);
 */
ErrorState_t GPIO_enumPinInit(const GPIO_PinConfig_t *PinConfig);

/*
 * @fn     GPIO_enumLowNibbleInit
 * @brief : Initializes GPIO pin configuration
 * @param : PinConfig[in]: Pointer to pin configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 */
ErrorState_t GPIO_enumLowNibbleInit(GPIO_LowNibbleConfig_t *LowNibbleConfig);

/*
 * @fn     GPIO_enumHighNibbleInit
 * @brief : Initializes GPIO pin configuration
 * @param : PinConfig[in]: Pointer to pin configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 */
ErrorState_t GPIO_enumHighNibbleInit(GPIO_HighNibbleConfig_t *HighNibbleConfig);

/*
 * @fn     GPIO_enumByteInit
 * @brief : Initializes GPIO pin configuration
 * @param : PinConfig[in]: Pointer to pin configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 */
ErrorState_t GPIO_enumByteInit(GPIO_ByteConfig_t *ByteConfig);

/*
 * @fn     GPIO_enumHalfPortInit
 * @brief : Initializes GPIO pin configuration
 * @param : PinConfig[in]: Pointer to pin configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 */
ErrorState_t GPIO_enumHalfPortInit(GPIO_HalfPortConfig_t *HalfPortConfig);

/*
 * @fn     GPIO_enumPortInit
 * @brief : Initializes GPIO pin configuration
 * @param : PinConfig[in]: Pointer to pin configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 */
ErrorState_t GPIO_enumPortInit(GPIO_PortConfig_t *PortConfig);

/**
 * @fn     GPIO_enumPortHalfInit
 * @brief : Initializes GPIO port configuration
 * @param : GPIO_PortHalfConfig_t[in]: Pointer to port configuration structure
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 */
ErrorState_t GPIO_enumPort8PinsInit(GPIO_8PinsConfig_t *GPIO_8PinsConfig);

/**
 * @fn     GPIO_enumWrite8BitsVal
 * @brief : Write 8 bits directly to ODR register starting from specified pin
 * @param : Port: GPIO port index (PORTA to PORTH)
 * @param : StartPin: Starting pin number (PIN0 to PIN8)
 * @param : Value: 8-bit value to write (0x00 to 0xFF)
 * @retval ErrorState_t: OK if write successful, NOK if invalid parameters
 * @example GPIO_enumWrite8PinsVal(GPIO_PORTA, GPIO_PIN0, 0xFF);
 */
ErrorState_t GPIO_enumWrite8PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                    uint8_t Value);

/**
 * @fn     GPIO_enumWrite4BitsVal
 * @brief : Write 4 bits directly to ODR register starting from specified pin
 * @param : Port: GPIO port index (PORTA to PORTH)
 * @param : StartPin: Starting pin number (PIN0 to PIN12)
 * @param : Value: 4-bit value to write (0x0 to 0xF)
 * @retval ErrorState_t: OK if write successful, NOK if invalid parameters
 */
ErrorState_t GPIO_enumWrite4PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                    uint8_t Value);

/**
 * @fn     GPIO_enumWritePinVal
 * @brief : Write a value to a GPIO pin
 * @param : Port: GPIO port (PORTA to PORTH)
 * @param : PinNum: Pin number (PIN0 to PIN15)
 * @param : PinVal: Value to write (PIN_LOW or PIN_HIGH)
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 * @example GPIO_enumWritePinVal(GPIO_PORTA, GPIO_PIN5, GPIO_PIN_HIGH);
 */
ErrorState_t GPIO_enumWritePinVal(GPIO_Port_t Port, GPIO_Pin_t PinNum,
                                  GPIO_PinValue_t PinVal);

/**
 * @fn     GPIO_enumReadPinVal
 * @brief : Read the current value of a GPIO pin
 * @param : Port: GPIO port (PORTA to PORTH)
 * @param : PinNum: Pin number (PIN0 to PIN15)
 * @param : PinVal: Pointer to store the read value
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 * @example
 * GPIO_PinValue_t val;
 * GPIO_enumReadPinVal(GPIO_PORTA, GPIO_PIN5, &val);
 */
ErrorState_t GPIO_enumReadPinVal(GPIO_Port_t Port, GPIO_Pin_t PinNum,
                                 GPIO_PinValue_t *PinVal);

/**
 * @fn     GPIO_enumTogPinVal
 * @brief : Toggle the current value of a GPIO pin
 * @param : Port: GPIO port (PORTA to PORTH)
 * @param : PinNum: Pin number (PIN0 to PIN15)
 * @retval GPIO_ErrorState: GPIO_OK if successful, GPIO_NOK if error
 * @example GPIO_enumTogPinVal(GPIO_PORTA, GPIO_PIN5);
 */
ErrorState_t GPIO_enumTogPinVal(GPIO_Port_t Port, GPIO_Pin_t PinNum);

/**
 * @fn     GPIO_enumPort4BitsInit
 * @brief : Initializes GPIO port 4-bit configuration
 * @param : GPIO_4BinsConfig[in]: Pointer to port 4-bit configuration structure
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumPort4PinsInit(GPIO_4PinsConfig_t *GPIO_4PinsConfig);

/**
 * @fn     GPIO_enumRead4PinsVal
 * @brief : Read the current value of a GPIO port 4-pins
 * @param : Port: GPIO port (PORTA to PORTH)
 * @param : StartPin: Starting pin number (PIN0 to PIN12)
 * @param : PinsValue: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumRead4PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                   uint8_t *PinsValue);

/*=================================================================================================================*/
/**
 * @fn     GPIO_enumRead8PinsVal
 * @brief : Read the current value of a GPIO port 8-pins
 * @param : Port: GPIO port (PORTA to PORTH)
 * @param : StartPin: Starting pin number (PIN0 to PIN15)
 * @param : PinsValue: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumRead8PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                   uint8_t *PinsValue);

/*=================================================================================================================*/
/**
 * @fn     GPIO_enumWriteLowNibble
 * @brief : Write the current value of a GPIO port low nibble
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : Copy_u8Val: Value to write
 * @param : PinsVal: Value to write
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 * @example GPIO_enumWriteLowNibbleVal(GPIO_PORTA, 0x0F, GPIO_PIN_HIGH);
 */
ErrorState_t GPIO_enumWriteLowNibbleVal(GPIO_Port_t port, uint8_t Copy_u8Val,
                                     GPIO_PinValue_t PinsVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumWriteHighNibble
 * @brief : Write the current value of a GPIO port high nibble
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : Copy_u8Val: Value to write
 * @param : PinsVal: Value to write
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 * @example GPIO_enumWriteHighNibbleVal(GPIO_PORTA, 0xF0, GPIO_PIN_HIGH);
 */
ErrorState_t GPIO_enumWriteHighNibbleVal(GPIO_Port_t port, uint8_t Copy_u8Val,
                                      GPIO_PinValue_t PinsVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumWriteByte
 * @brief : Write the current value of a GPIO port byte
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : Copy_u8Val: Value to write
 * @param : PinsVal: Value to write
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 * @example GPIO_enumWriteByteVal(GPIO_PORTA, 0xFF, GPIO_PIN_HIGH);
 */
ErrorState_t GPIO_enumWriteByteVal(GPIO_Port_t port, uint8_t Copy_u8Val,
                                GPIO_PinValue_t PinsVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumWriteHalfPort
 * @brief : Write the current value of a GPIO port half word
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : Copy_u8Val: Value to write
 * @param : PinsVal: Value to write
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 * @example GPIO_enumWriteHalfWordVal(GPIO_PORTA, 0xFFFF, GPIO_PIN_HIGH);
 */
ErrorState_t GPIO_enumWriteHalfWordVal(GPIO_Port_t port, uint16_t Copy_u16Val,
                                    GPIO_PinValue_t PinsVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumWritePort
 * @brief : Write the current value of a GPIO port word
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : Copy_u8Val: Value to write
 * @param : PinsVal: Value to write
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 * @example GPIO_enumWritePortVal(GPIO_PORTA, 0xFFFF, GPIO_PIN_HIGH);
 */
ErrorState_t GPIO_enumWritePortVal(GPIO_Port_t port, uint16_t Copy_u16Val,
                                GPIO_PinValue_t PinsVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumReadLowNibbleVal
 * @brief : Read the current value of a GPIO port low nibble
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : LowNibbleVal: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumReadLowNibbleVal(GPIO_Port_t port, uint8_t *LowNibbleVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumReadHighNibbleVal
 * @brief : Read the current value of a GPIO port high nibble
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : HighNibbleVal: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumReadHighNibbleVal(GPIO_Port_t port,
                                        uint8_t *HighNibbleVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumReadByteVal
 * @brief : Read the current value of a GPIO port byte
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : ByteVal: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumReadByteVal(GPIO_Port_t port, uint8_t *ByteVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumReadHalfPortVal
 * @brief : Read the current value of a GPIO port half word
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : HalfPortVal: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumReadHalfPortVal(GPIO_Port_t port, uint16_t *HalfPortVal);
/*=================================================================================================================*/
/**
 * @fn     GPIO_enumReadPortVal
 * @brief : Read the current value of a GPIO port word
 * @param : port: GPIO port (PORTA to PORTH)
 * @param : PortVal: Pointer to store the read value
 * @retval ErrorState_t: OK if configuration successful, NOK if invalid
 * parameters, NULL_POINTER if invalid pointer
 */
ErrorState_t GPIO_enumReadPortVal(GPIO_Port_t port, uint32_t *PortVal);
/*=================================================================================================================*/

#endif /* GPIO_INTERFACE_H_ */
