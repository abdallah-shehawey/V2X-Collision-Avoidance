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

/* Front-vehicle presence gates (cm): closer than this counts as "vehicle
 * ahead". FCW warns earlier (farther) than the oncoming/overtaking case, so it
 * has the wider gate; DNPW + head-on share the nearer gate.
 * Prototype scale: small car in a corridor. */
#define FCW_FRONT_THRESHOLD        (40.0f)  /* FCW front gate: warn earlier      */
#define DNPW_FRONT_THRESHOLD       (20.0f)  /* DNPW + head-on front gate: nearer */

/* DNPW escalation gate (cm): when DNPW fires, a near front-right reading (a car
 * alongside on the overtaking side) raises the DNPW severity to CRITICAL. */
#define DNPW_FRONT_RIGHT_CRITICAL  (20.0f)

#endif
