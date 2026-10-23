/**
 ******************************************************************************
 * @file    BUZ_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the BUZZ driver — the piezo buzzer.
 * @ingroup hal_buzz
 ******************************************************************************
 */

#ifndef BUZ_INTERFACE_H_
#define BUZ_INTERFACE_H_

#include "../../MCAL/GPIO/GPIO_interface.h"
#include "../../../Drivers/LIB/ErrTypes.h"

/**
 * @brief Which pin level makes the buzzer sound — i.e. which way round it is wired.
 */
typedef enum
{
  BUZ_ACTIVE_LOW = 0, /**< Sounds when the pin is driven **low**. */
  BUZ_ACTIVE_HIGH     /**< Sounds when the pin is driven **high**. */
} BUZ_ActiveState_t;

/**
 * @brief Configuration structure for the buzzer
 */
typedef struct
{
  GPIO_Port_t       Port;        /**< GPIO Port (PORTA to PORTH) */
  GPIO_Pin_t        Pin;         /**< GPIO Pin (PIN0 to PIN15) */
  BUZ_ActiveState_t ActiveState; /**< ACTIVE_HIGH or ACTIVE_LOW */
} BUZ_Config_t;

/**
 * @brief Initialize the buzzer pin
 * @param Config Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_Init(const BUZ_Config_t *Config);

/**
 * @brief Turn on the buzzer
 * @param Config Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_On(const BUZ_Config_t *Config);

/**
 * @brief Turn off the buzzer
 * @param Config Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_Off(const BUZ_Config_t *Config);

/**
 * @brief Toggle the buzzer state
 * @param Config Pointer to buzzer configuration structure
 * @return ErrorState_t: Error state
 */
ErrorState_t BUZ_Toggle(const BUZ_Config_t *Config);

#endif /* BUZ_INTERFACE_H_ */
