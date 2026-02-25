/*
 *<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    KPD_config.h    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 *
 *  Author : Abdallah Abdelmoemen Shehawey
 *  Layer  : HAL
 *  SWC    : KPD
 *
 */

#ifndef KPD_CONFIG_H_
#define KPD_CONFIG_H_

#define KPD_ROW_INIT GPIO_PIN0
#define KPD_ROW_END GPIO_PIN3

#define KPD_COL_INIT GPIO_PIN4
#define KPD_COL_END GPIO_PIN7

/* C0   C1  C2  C3  */
uint8_t KPD_u8Buttons[4][4] = {{'7', '8', '9', '/'}, /* Row0 */
                               {'4', '5', '6', '*'}, /* Row1 */
                               {'1', '2', '3', '-'}, /* Row2 */
                               {'?', '0', '=', '+'} /* Row3 */};

/*
  * Options:
    1-GPIO_PORTA
    2-GPIO_PORTB
    3-GPIO_PORTC
    4-GPIO_PORTD
    5-GPIO_PORTE
    6-GPIO_PORTF
    7-GPIO_PORTG
    8-GPIO_PORTH
*/

#define KPD_PORT GPIO_PORTD

/* Row Pins */
#define KPD_R0 GPIO_PIN0
#define KPD_R1 GPIO_PIN1
#define KPD_R2 GPIO_PIN2
#define KPD_R3 GPIO_PIN3

/* Column Pins */
#define KPD_C0 GPIO_PIN4
#define KPD_C1 GPIO_PIN5
#define KPD_C2 GPIO_PIN6
#define KPD_C3 GPIO_PIN7

#endif /* KPD_CONFIG_H_ */
