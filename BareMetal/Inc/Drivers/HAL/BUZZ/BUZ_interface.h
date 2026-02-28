/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    BUZ_interface.h   >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                            **
 **                  SWC    : BUZZER                                         **
 **                                                                           **
 **===========================================================================**
 */

#ifndef BUZ_INTERFACE_H_
#define BUZ_INTERFACE_H_

#include "GPIO_interface.h"
#include "ErrTypes.h"


typedef enum
{
  ACTIVE_LOW = 0,
  ACTIVE_HIGH
} BUZ_ActiveState_t;

/**
 * @brief Configuration structure for the buzzer
 */
typedef struct
{
  GPIO_Port_t Port; /* GPIO Port (PORTA to PORTH) */
  GPIO_Pin_t Pin;   /* GPIO Pin (PIN0 to PIN15) */
  BUZ_ActiveState_t ActiveState; /* ACTIVE_HIGH or ACTIVE_LOW */
} BUZ_Config_t;

/**
 * @brief Initialize the buzzer pin
 * @param Config: Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_Init(const BUZ_Config_t *Config);

/**
 * @brief Turn on the buzzer
 * @param Config: Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_On(const BUZ_Config_t *Config);

/**
 * @brief Turn off the buzzer
 * @param Config: Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_Off(const BUZ_Config_t *Config);

/**
 * @brief Toggle the buzzer state
 * @param Config: Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_Toggle(const BUZ_Config_t *Config);

#endif /* BUZ_INTERFACE_H_ */
