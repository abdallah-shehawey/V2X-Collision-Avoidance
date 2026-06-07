#ifndef BSW_CONFIG_H
#define BSW_CONFIG_H

/* ====== Distance Threshold ====== */
/* Object in blind spot if side US < this value (cm).
 * 50cm ≈ adjacent lane for a 20cm-wide car; rejects the arena side walls
 * (≥~85cm when driving center) and the 400cm no-echo sentinel.
 * NOTE: keep the car roughly centered during BSW tests so a near wall
 * doesn't read as a side vehicle. */
#define BSW_SIDE_THRESHOLD (50.0f)

/* ====== Alerts ====== */
#define BSW_ENABLE_LED_ALERT 1
#define BSW_ENABLE_BUZZER    1

#endif
