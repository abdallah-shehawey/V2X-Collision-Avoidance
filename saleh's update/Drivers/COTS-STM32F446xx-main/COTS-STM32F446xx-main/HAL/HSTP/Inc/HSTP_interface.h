/*
 * HSTP_interface.h
 *
 *  Created on: Aug 3, 2025
 *      Author: abdallah-shehawey
 */

#ifndef HSTP_INTERFACE_H_
#define HSTP_INTERFACE_H_


ErrorState_t HSTP_enumInit(void);
void HSTP_vSendData(uint8_t Copy_u8Data);
void HSTP_vShiftData(uint8_t Copy_u8Data);

#endif /* HSTP_INTERFACE_H_ */
