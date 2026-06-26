
#ifndef SCB_INTERFACE_H_
#define SCB_INTERFACE_H_

#include "stdint.h"
#include "../../LIB/ErrTypes.h"

/**
 * @fn     SCB_vSetPriorityGrouping
 * @brief  Set the Priority Grouping in AIRCR register
 * @details Configures the split between Group Priority and Sub-Priority bits
 *          in the Interrupt Priority Register.
 * @param  Copy_u8PriorityGrouping: Priority grouping value (0-7)
 *         - 3: 4 bits for Group, 0 bits for Sub (16 Groups, 0 Sub)
 *         - 4: 3 bits for Group, 1 bit for Sub (8 Groups, 2 Sub)
 *         - 5: 2 bits for Group, 2 bits for Sub (4 Groups, 4 Sub)
 *         - 6: 1 bit for Group, 3 bits for Sub (2 Groups, 8 Sub)
 *         - 7: 0 bits for Group, 4 bits for Sub (1 Group, 16 Sub)
 * @return ErrorState_t: OK if valid, NOK if invalid grouping
 * @example SCB_vSetPriorityGrouping(0x05);
 */
ErrorState_t SCB_vSetPriorityGrouping(uint8_t Copy_u8PriorityGrouping);

/**
 * @fn     SystemInit
 * @brief  System initialization called from startup file.
 * @details Handles FPU enablement and potential critical system settings.
 */
void SystemInit(void);

#endif /* SCB_INTERFACE_H_ */
