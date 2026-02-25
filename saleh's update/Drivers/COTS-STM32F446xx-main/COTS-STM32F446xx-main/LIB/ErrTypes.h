/********************************************************
 * @file ErrTypes.h
 * @name Abdallah AbdelMomen
 * @brief Return Error State types
 */
#ifndef ERRTYPES_H_
#define ERRTYPES_H_

#define NULL    ((void *)0)

#define ENABLE  1u
#define DISABLE 0u

/* Func States  */
#define IDLE 0u
#define BUSY 1u


typedef enum
{
  OK = 0,
  NOK,
  NULL_POINTER,
  BUSY_STATE,
  TIMEOUT_STATE
} ErrorState_t;

#endif /* ERRTYPES_H_ */
