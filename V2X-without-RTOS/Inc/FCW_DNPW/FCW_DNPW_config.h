#ifndef FCW_DNPW_CONFIG_H
#define FCW_DNPW_CONFIG_H

/*
 * ====== Cooperative FCW + DNPW Model ======
 *
 * FCW and DNPW share the same cooperative inputs, so they live in one pipeline:
 *
 *   - FrontObject    : front ultrasonic sees something ahead
 *   - Oncoming       : a neighbor is heading opposite to us (DSRC heading)
 *   - OncomingHeadon : that oncoming neighbor's broadcast fcw_headon_flag
 *
 * Decision (the oncoming car's head-on flag is read only when one exists):
 *   FrontObject, no oncoming                 -> local FCW
 *   FrontObject + oncoming + its headon      -> cooperative head-on FCW
 *   FrontObject + oncoming + no its headon   -> DNPW (overtaking)
 *
 * Severity comes from the shared cycle gaps (SafetyEngine_SafeDist /
 * CriticalDist) against the front distance.
 */

/* Front-vehicle presence gate (cm): closer than this counts as "vehicle
 * ahead", the boolean that distinguishes the scenarios above. */
#define FCW_DNPW_FRONT_THRESHOLD   (300.0f)

#endif
