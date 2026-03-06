#ifndef BSW_INTERFACE_H
#define BSW_INTERFACE_H

/* Initialize BSW module */
void BSW_voidInit(void);

/* Periodic runnable */
void BSW_voidUpdate(void);

/* Simulation inputs */
void BSW_voidSetSimulatedData(float leftDist, float rightDist, unsigned char leftSignal, unsigned char rightSignal);

#endif
