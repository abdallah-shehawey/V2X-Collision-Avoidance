/**
 ******************************************************************************
 * @file    ErrTypes.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Project-wide return codes and boolean-style constants.
 * @ingroup lib
 *
 * Every driver in the firmware that can fail returns an @ref ErrorState_t
 * rather than a bare integer, so a caller can always tell *why* a call failed
 * (bad pointer, peripheral busy, timed out) instead of only *that* it failed.
 * Drivers that cannot fail return `void`.
 ******************************************************************************
 */
#ifndef ERRTYPES_H_
#define ERRTYPES_H_

/**
 * @addtogroup lib
 * @{
 */

#ifndef NULL
/** @brief Null pointer constant, defined here so the drivers never need a libc header. */
#define NULL 0u
#endif

/** @brief Generic "switch this on" value for the `ENABLE`/`DISABLE` driver arguments. */
#define ENABLE 1u
/** @brief Generic "switch this off" value for the `ENABLE`/`DISABLE` driver arguments. */
#define DISABLE 0u

/** @brief Peripheral state: no transfer in progress, a new one may be started. */
#define IDLE 0u
/** @brief Peripheral state: a transfer is already in progress. */
#define BUSY 1u

/**
 * @brief Return code shared by every fallible driver call in the firmware.
 */
typedef enum
{
  OK = 0,       /**< The call completed successfully. */
  NOK,          /**< The call failed, or an argument was out of range. */
  NULL_POINTER, /**< A required pointer argument was NULL. */
  BUSY_STATE,   /**< The peripheral was still busy with a previous transfer. */
  TIMEOUT_STATE /**< The peripheral did not respond within the allowed time. */
} ErrorState_t;

/** @} */ /* end of lib */

#endif /* ERRTYPES_H_ */
