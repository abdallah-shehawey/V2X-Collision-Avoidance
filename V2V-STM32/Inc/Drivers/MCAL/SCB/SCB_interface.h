/**
 ******************************************************************************
 * @file    SCB_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the SCB driver — the system control block.
 * @ingroup mcal_scb
 ******************************************************************************
 */


#ifndef SCB_INTERFACE_H_
#define SCB_INTERFACE_H_

#include "stdint.h"
#include "../../LIB/ErrTypes.h"

/**
 * @brief  Set the Priority Grouping in AIRCR register
 * @details Configures the split between Group Priority and Sub-Priority bits
 *          in the Interrupt Priority Register.
 * @param  Copy_u8PriorityGrouping Priority grouping value (0-7)
 *         - 3: 4 bits for Group, 0 bits for Sub (16 Groups, 0 Sub)
 *         - 4: 3 bits for Group, 1 bit for Sub (8 Groups, 2 Sub)
 *         - 5: 2 bits for Group, 2 bits for Sub (4 Groups, 4 Sub)
 *         - 6: 1 bit for Group, 3 bits for Sub (2 Groups, 8 Sub)
 *         - 7: 0 bits for Group, 4 bits for Sub (1 Group, 16 Sub)
 * @return ErrorState_t: OK if valid, NOK if invalid grouping
 * @par Example:
 * SCB_vSetPriorityGrouping(0x05);
 */
ErrorState_t SCB_vSetPriorityGrouping(uint8_t Copy_u8PriorityGrouping);

/**
 * @brief  System initialization called from startup file.
 * @details Handles FPU enablement and potential critical system settings.
 */
void SystemInit(void);

/**
 * @brief  Relocate the vector table (write `SCB->VTOR`).
 * @details
 * Used once, by the bootloader, right before it hands control to an
 * application slot: the core normally starts fetching exception/interrupt
 * vectors from address 0, but a bootloader + A/B application scheme puts the
 * *running* application's own vector table at the base of whichever slot is
 * active — not at 0 — so the core has to be told where to find it. See
 * `V2V-STM32/docs/FOTA.md` for the full bootloader-to-application handoff
 * sequence this is one step of.
 * @param  Copy_u32VectorTableAddr Base address of the target vector table.
 *         Must be 512-byte aligned (the low 9 bits of `VTOR` are reserved on
 *         this part) — in practice this is always a flash sector boundary,
 *         which is far more aligned than that already.
 * @return ErrorState_t: OK if the address was validly aligned and applied,
 *         NOK if it was not.
 * @par Example:
 * SCB_vSetVectorTable(0x0800C000); // jump target: application Slot A
 */
ErrorState_t SCB_vSetVectorTable(uint32_t Copy_u32VectorTableAddr);

/**
 * @brief  Trigger a full Cortex-M core + peripheral reset (`AIRCR.SYSRESETREQ`).
 * @details
 * Used by FOTA (`FOTA_Metadata_interface.h`, `FOTA_RequestUpdate()`) to
 * reboot into the bootloader after writing the "update requested" flag to
 * the metadata sector: the bootloader always runs first on any reset and
 * checks that flag, so requesting an update is "write the flag, then reset."
 * @note   Never returns — the reset takes effect within a few clock cycles.
 */
void SCB_vSystemReset(void);

#endif /* SCB_INTERFACE_H_ */
