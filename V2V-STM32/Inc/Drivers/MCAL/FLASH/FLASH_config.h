/**
 ******************************************************************************
 * @file    FLASH_config.h
 * @brief   Compile-time configuration for the FLASH driver.
 * @ingroup mcal_flash
 *
 * @details
 * Nothing to tune here today — the driver always programs in x32 (word)
 * parallelism (see `FLASH_PSIZE_X32` in `FLASH_private.h`), which is the only
 * mode valid at this board's 3.3 V supply. This file exists so the driver
 * follows the same four-file layout as every other MCAL driver, and as the
 * obvious place to add a build-time knob later (e.g. a different `PSIZE` for
 * a board revision running at a different voltage).
 ******************************************************************************
 */

#ifndef FLASH_CONFIG_H_
#define FLASH_CONFIG_H_

#endif /* FLASH_CONFIG_H_ */
