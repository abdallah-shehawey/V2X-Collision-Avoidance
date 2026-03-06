#ifndef EEBL_INTERFACE_H
#define EEBL_INTERFACE_H

/* Initialize EEBL module */
void EEBL_voidInit(void);

/* Main runnable function (call periodically) */
void EEBL_voidUpdate(void);

/* For simulation only */
void EEBL_voidSetSimulatedData(float speed, float rearDistance);

#endif
