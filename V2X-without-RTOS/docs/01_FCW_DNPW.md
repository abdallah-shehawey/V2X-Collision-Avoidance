# FCW + DNPW — Forward Collision & Do-Not-Pass Warning

> **Files:** [`Src/FCW_DNPW_program.c`](../Src/FCW_DNPW_program.c) · [`Inc/FCW_DNPW/FCW_DNPW_interface.h`](../Inc/FCW_DNPW/FCW_DNPW_interface.h) · [`Inc/FCW_DNPW/FCW_DNPW_config.h`](../Inc/FCW_DNPW/FCW_DNPW_config.h) · [`Inc/FCW_DNPW/FCW_DNPW_private.h`](../Inc/FCW_DNPW/FCW_DNPW_private.h)

---

## 1. Why one module? (the idea, simply)

**FCW** (Forward Collision Warning) and **DNPW** (Do-Not-Pass Warning) live in the
same module because they are decided from the **same per-cycle signals**. Telling
them apart is just a matter of *who else sees the obstacle*:

- **FCW** — there's a vehicle ahead and you might hit it. Either it's in your lane
  (local), or it's an oncoming car you're about to meet head-on (cooperative).
- **DNPW** — you're tempted to overtake the car ahead, but there's an oncoming car
  in the other lane. Passing now would be dangerous → "Do Not Pass".

The trick: an oncoming car that is **itself** looking at an obstacle ahead is on a
real head-on course with you. An oncoming car that sees **nothing** ahead is just
in another lane — that's an overtaking situation, not a head-on one.

---

## 2. The five per-cycle signals

The module accumulates these during one neighbor pass, then derives the flags on
demand in the getters:

| Signal | Meaning | Set in |
|--------|---------|--------|
| `FrontDist` | front ultrasonic distance (cm) | BeginCycle |
| `FrontObject` | ultrasonic sees something within the front gate | BeginCycle |
| `FrontFlag` | front-collision severity (Safe/Warning/Critical) | ProcessSameDirection |
| `Oncoming` | an opposite-direction neighbor exists | ProcessOppositeDirection |
| `OncomingHeadon` | that oncoming neighbor raised its own head-on flag | ProcessOppositeDirection |

> **`FrontObject` vs `FrontFlag`:** `FrontObject` just means the ultrasonic saw
> *something* (a car, a wall, a curb). `FrontFlag` is only set when a
> **same-direction DSRC neighbor** confirms that object is actually a vehicle.

---

## 3. Configuration (FCW_DNPW_config.h)

```c
#define FCW_DNPW_FRONT_THRESHOLD (300.0f)  // object within 300 cm counts as "ahead"
```

Severity is **not** tuned here — it comes from the shared cycle gaps the
SafetyEngine computes once per cycle (`SafetyEngine_SafeDist` /
`SafetyEngine_CriticalDist`), the same model EEBL uses. The rule:

```text
distance >= safe_dist                 -> SAFE
critical_dist <= distance < safe_dist -> WARNING
distance < critical_dist              -> CRITICAL
```

---

## 4. Lifecycle — BeginCycle → ProcessNeighbor → getters

There is **no EndCycle**. The module accumulates signals during the pass and the
getters derive the results when read.

### `FCW_DNPW_voidBeginCycle(front_distance)`

Latches the front distance, decides the boolean `FrontObject` gate, and resets the
accumulators.

### `FCW_DNPW_voidProcessSameDirection()`

Called for each **same-direction** neighbor. It classifies the front distance into
a severity and keeps the worst across the cycle, so `FrontFlag` is ready to read:

```c
RiskLevel_t sev = FCW_DNPW_FrontSeverity();
if (sev > FCW_DNPW_FrontFlag) FCW_DNPW_FrontFlag = sev;
```

### `FCW_DNPW_voidProcessOppositeDirection(n)`

Called for each **oncoming** neighbor. Records that one exists and whether it
raised its own head-on flag:

```c
FCW_DNPW_Oncoming = 1;
if (n->fcw_headon_flag > 0) FCW_DNPW_OncomingHeadon = 1;
```

> The oncoming car's flag is only ever inspected here — i.e. only when an oncoming
> car actually exists.

---

## 5. The four getters (the decision)

| Getter | Returns | When it fires |
|--------|---------|---------------|
| `FCW_GetFrontFlag()` | 0/1/2 (severity) | a same-direction vehicle is ahead |
| `FCW_GetHeadonFlag()` | 0/1 (candidate) | a vehicle ahead **and** an oncoming car |
| `FCW_GetHeadonConfirmed()` | 0/1/2 (severity) | candidate **and** the oncoming car's head-on flag |
| `DNPW_GetFlag()` | 0/1 (presence) | candidate **and** the oncoming car has **no** head-on flag |

```c
uint8_t FCW_GetHeadonFlag(void) {
  return (FCW_DNPW_FrontObject && FCW_DNPW_Oncoming) ? 1U : 0U;
}

uint8_t FCW_GetHeadonConfirmed(void) {        // genuine head-on
  if (FCW_GetHeadonFlag() && FCW_DNPW_OncomingHeadon)
    return (uint8_t)FCW_DNPW_FrontSeverity();
  return 0;
}

uint8_t DNPW_GetFlag(void) {                  // overtaking risk
  return (FCW_GetHeadonFlag() && !FCW_DNPW_OncomingHeadon) ? 1U : 0U;
}
```

> **Why DNPW has no severity:** the oncoming car is in another lane, so the front
> distance measures whatever is in *our* lane, not the car we'd be passing. There
> is no meaningful distance to that car, so DNPW is a presence signal (0/1) only.

---

## 6. The three scenarios

**Scenario 1 — Local FCW.** A car directly ahead in my lane.

```
V1 ---> ---> V2     (V2 ahead of V1, same lane)
```

`FCW_GetFrontFlag()` returns the severity. No cooperation needed.

**Scenario 2 — Cooperative head-on FCW.** A car coming straight at me.

```
V1 ----->   <----- V2
```

Both cars see an object ahead **and** an oncoming car, so both raise
`fcw_headon_flag` and broadcast it. Each confirms the other →
`FCW_GetHeadonConfirmed()` returns the severity.

**Scenario 3 — DNPW.** An oncoming car, but in another lane.

```
V1 ----->
V2 ----->          (V2 ahead of V1)
        <----- V3  (other lane, nothing ahead of it)
```

V1 sees a car ahead and an oncoming car (V3) → raises its head-on candidate. But
V3 sees nothing ahead, so it never raises a head-on flag. When V1 reads V3's flag
as 0 → it's an overtaking situation → `DNPW_GetFlag()` returns 1.

> The deciding bit between scenario 2 and 3 is **only** `OncomingHeadon`: if the
> oncoming car also faces an obstacle, it's a real head-on; if not, it's a pass.

---

## 7. What is broadcast vs read locally

| Flag | Broadcast over DSRC? | Read by |
|------|----------------------|---------|
| `fcw_headon_flag` (`FCW_GetHeadonFlag`) | **yes** — the cooperative signal | other cars, to confirm |
| `FCW_GetFrontFlag` | no | this car's driver (LED/buzzer) |
| `FCW_GetHeadonConfirmed` | no | this car's driver |
| `DNPW_GetFlag` | no | this car's driver |

---

## 8. Quick Summary

| Point | Takeaway |
|-------|----------|
| Goal | Prevent frontal/head-on collisions and unsafe overtaking |
| Sensor | Front ultrasonic + DSRC heading + neighbor head-on flag |
| Model | distance-based severity (shared with EEBL), no TTC / relative speed |
| Cooperation | only `fcw_headon_flag` is exchanged; only inspected for oncoming cars |
| FCW vs DNPW | same vs other lane, decided by the oncoming car's own head-on flag |
| Alerts | LED/buzzer/ADAS driven externally — module only computes the flags |
