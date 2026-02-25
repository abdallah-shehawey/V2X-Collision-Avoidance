/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SSD_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : SSD                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef SSD_INTERFACE_H_
#define SSD_INTERFACE_H_

/**
 * @brief Seven-Segment Display Type Enumeration
 * @details Defines the type of seven-segment display:
 *          - Common Cathode: Segments active HIGH
 *          - Common Anode: Segments active LOW
 */
typedef enum
{
  SSD_COMMON_CATHODE = 0, /**< Common cathode type (segments active HIGH) */
  SSD_COMMON_ANODE = 1    /**< Common anode type (segments active LOW) */
} SSD_Type_t;

/**
 * @brief Seven-Segment Display Configuration Structure
 * @details Contains all configuration parameters for a seven-segment display:
 *          - Display type (common cathode/anode)
 *          - Data port and pins
 *          - Enable port and pin
 */
typedef struct
{
  SSD_Type_t Type;        /**< SSD_COMMON_CATHODE or SSD_COMMON_ANODE */
  GPIO_Port_t DataPort;   /**< GPIO Port for segment pins (A-H) */
  GPIO_Port_t EnablePort; /**< GPIO Port for enable pin (A-H) */
  GPIO_Pin_t EnablePin;   /**< GPIO Pin for enable (0-15) */
  GPIO_Pin_t StartPin;    /**< Starting pin for 8-bit segment data */
} SSD_Config_t;

/**
 * @fn    SSD_enumInit
 * @brief Initialize a seven-segment display
 * @details This function:
 *          1. Configures data port pins as outputs
 *          2. Configures enable pin as output
 *          3. Sets initial pin states
 * @param Configuration: Pointer to SSD configuration structure
 * @return ErrorState_t:
 *         - OK: Initialization successful
 *         - NOK: Invalid configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumInit(const SSD_Config_t *Configuration);

/**
 * @fn    SSD_enumDisplayNumber
 * @brief Display a number on the seven-segment display
 * @details This function:
 *          1. Validates input number (0-9)
 *          2. Converts number to segment pattern
 *          3. Applies pattern considering display type
 * @param Configuration: Pointer to SSD configuration structure
 * @param Copy_u8Number: Number to display (0-9)
 * @return ErrorState_t:
 *         - OK: Number displayed successfully
 *         - NOK: Invalid number or configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumDisplayNumber(const SSD_Config_t *Configuration, uint8_t Copy_u8Number);

/**
 * @fn    SSD_enumEnable
 * @brief Enable the seven-segment display
 * @details Sets the enable pin according to display type:
 *          - Common Cathode: Enable pin LOW
 *          - Common Anode: Enable pin HIGH
 * @param Configuration: Pointer to SSD configuration structure
 * @return ErrorState_t:
 *         - OK: Display enabled successfully
 *         - NOK: Invalid configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumEnable(const SSD_Config_t *Configuration);

/**
 * @fn    SSD_enumDisable
 * @brief Disable the seven-segment display
 * @details Sets the enable pin according to display type:
 *          - Common Cathode: Enable pin HIGH
 *          - Common Anode: Enable pin LOW
 * @param Configuration: Pointer to SSD configuration structure
 * @return ErrorState_t:
 *         - OK: Display disabled successfully
 *         - NOK: Invalid configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumDisable(const SSD_Config_t *Configuration);

#endif /* SSD_INTERFACE_H_ */
