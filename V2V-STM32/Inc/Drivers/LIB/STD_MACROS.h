/**
 ******************************************************************************
 * @file    STD_MACROS.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Bit-manipulation macros used by every register access in the MCAL.
 * @ingroup lib
 *
 * These are the only primitives the drivers use to touch a peripheral register:
 * an MCAL driver never writes a raw hex mask, it names the bit and uses one of
 * the macros below.
 *
 * @warning The arguments are not parenthesised internally. Pass a plain
 *          variable and a plain bit index — an expression such as
 *          `READ_BIT(reg, i + 1)` will not parse the way you expect, and
 *          `READ_BIT(...)` used inside a larger expression needs its own
 *          parentheses because `&` binds looser than `==`.
 ******************************************************************************
 */
#ifndef STD_MACROS_h_
#define STD_MACROS_h_

/**
 * @addtogroup lib
 * @{
 */

/**
 * @brief Register width in bits assumed by @ref ROR and @ref ROL.
 * @note  This is 8, so the two rotate macros are only correct for 8-bit
 *        values. They are not used on the 32-bit peripheral registers.
 */
#define REGISTER_SIZE 8

/**
 * @brief Set bit @p bit of @p reg to 1.
 * @param reg Register or variable to modify in place.
 * @param bit Bit index, 0 = least-significant bit.
 */
#define SET_BIT(reg, bit) reg |= (1 << bit)

/**
 * @brief Clear bit @p bit of @p reg to 0.
 * @param reg Register or variable to modify in place.
 * @param bit Bit index, 0 = least-significant bit.
 */
#define CLR_BIT(reg, bit) reg &= (~(1 << bit))

/**
 * @brief Toggle bit @p bit of @p reg.
 * @param reg Register or variable to modify in place.
 * @param bit Bit index, 0 = least-significant bit.
 */
#define TOG_BIT(reg, bit) reg ^= (1 << bit)

/**
 * @brief Read bit @p bit of @p reg, shifted down to bit 0.
 * @param reg Register or variable to read.
 * @param bit Bit index, 0 = least-significant bit.
 * @return 0 or 1.
 */
#define READ_BIT(reg, bit) (reg & (1 << bit)) >> bit

/**
 * @brief Extract byte @p byte of @p reg, shifted down to bit 0.
 * @param reg  Register or variable to read.
 * @param byte Byte index, 0 = least-significant byte.
 * @return The selected byte, in the range 0..255.
 */
#define READ_BYTE(reg, byte) (reg & (0XFF << (byte * 8))) >> (byte * 8)

/**
 * @brief Test whether bit @p bit of @p reg is set.
 * @param reg Register or variable to read.
 * @param bit Bit index, 0 = least-significant bit.
 * @return 1 if the bit is set, 0 otherwise.
 */
#define IS_BIT_SET(reg, bit) (reg & (1 << bit)) >> bit

/**
 * @brief Test whether bit @p bit of @p reg is clear.
 * @param reg Register or variable to read.
 * @param bit Bit index, 0 = least-significant bit.
 * @return 1 if the bit is clear, 0 otherwise.
 */
#define IS_BIT_CLR(reg, bit) !((reg & (1 << bit)) >> bit)

/**
 * @brief Rotate @p reg right by @p num bits, in place.
 * @param reg Variable to rotate in place.
 * @param num Number of bit positions to rotate by, 1..7.
 * @note  Assumes an 8-bit value — see @ref REGISTER_SIZE.
 */
#define ROR(reg, num) reg = (reg << (REGISTER_SIZE - num)) | (reg >> (num))

/**
 * @brief Rotate @p reg left by @p num bits, in place.
 * @param reg Variable to rotate in place.
 * @param num Number of bit positions to rotate by, 1..7.
 * @note  Assumes an 8-bit value — see @ref REGISTER_SIZE.
 */
#define ROL(reg, num) reg = (reg >> (REGISTER_SIZE - num)) | (reg << (num))

/** @} */ /* end of lib */

#endif /* STD_MACROS_H_ */
