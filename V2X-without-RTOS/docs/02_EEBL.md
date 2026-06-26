# EEBL — Electronic Emergency Brake Lights

> **Files:** [`Src/EEBL_program.c`](../Src/EEBL_program.c) · [`Inc/EEBL/EEBL_interface.h`](../Inc/EEBL/EEBL_interface.h) · [`Inc/EEBL/EEBL_config.h`](../Inc/EEBL/EEBL_config.h) · [`Inc/EEBL/EEBL_private.h`](../Inc/EEBL/EEBL_private.h)

---

## 1. What does this module do? (the idea, simply)

Imagine you're driving and suddenly you have to **brake hard**. The car behind you
may not notice fast enough. **EEBL**'s job: when I brake in an emergency and there's
a car close behind, raise a warning so the alert (LED/buzzer) can react in time.

> **Difference from FCW:** FCW protects me from what's ahead. EEBL is about whoever
> is behind me. The two complement each other.
>
> **This module only computes a risk level** (`EEBL_u8GetFlag()` → 0/1/2). Driving
> the actual alert is done by the caller.

---

## 2. Inputs and Outputs

### Inputs

| Input | Source | Unit |
|-------|--------|------|
| `Host_Speed` | my current speed | m/s |
| `rear_distance` | rear ultrasonic sensor (US_REAR) | cm |
| `SafetyEngine_SafeDist` / `CriticalDist` | shared cycle gaps | cm |

### Output

| Output | Description |
|--------|-------------|
| `EEBL_u8GetFlag()` | the risk level: 0=Safe, 1=Warning, 2=Critical |

> **EEBL is entirely local** — it does not broadcast its own flag over DSRC (there's
> no `eebl_flag` in the Neighbor message). It decides from its own sensors and
> exposes the result through the getter.

---

## 3. Configuration (EEBL_config.h)

```c
#define EEBL_DECEL_THRESHOLD (-0.20f)  // speed drop per cycle counted as braking (negative)
```

The safe/critical distances are **not** tuned here — they come from the shared
cycle gaps the SafetyEngine computes once per cycle (`SafetyEngine_SafeDist` /
`SafetyEngine_CriticalDist`), the same model FCW uses.

- **`EEBL_DECEL_THRESHOLD = -0.20`:** the speed change between two consecutive
  cycles treated as "sudden braking". If my speed dropped by 0.20 m/s or more in
  one cycle → braking. Smaller magnitude = more sensitive.

---

## 4. Lifecycle — BeginCycle → ProcessNeighbor → getter

There is **no EndCycle**: the worst severity is accumulated during the pass and the
getter returns it.

### `EEBL_voidBeginCycle()` — the braking gate

```c
float decel = Host_Speed - EEBL_PrevSpeed;
EEBL_BrakingDetected = (decel <= EEBL_DECEL_THRESHOLD) ? 1U : 0U;

EEBL_PrevSpeed  = Host_Speed;     // save for next cycle
EEBL_WorstLevel = RISK_SAFE;      // reset so an old WARNING doesn't stick
```

`EEBL_BrakingDetected = 1` only on the cycle where the speed actually dropped.

### `EEBL_voidProcessNeighbor(rear_distance)`

Called for each **same-direction** neighbor (the SafetyEngine only dispatches those
here):

```c
if (!EEBL_BrakingDetected) return;                                  // 1. must be braking

if (rear_distance <= 0.0f || rear_distance >= SafetyEngine_SafeDist) return;  // 2. car behind, in range

// distance < safe_dist → WARNING or CRITICAL. Keep the WORST across the cycle.
RiskLevel_t level = (rear_distance < SafetyEngine_CriticalDist) ? RISK_CRITICAL : RISK_WARNING;
if (level > EEBL_WorstLevel) EEBL_WorstLevel = level;
```

Two subtle points:

- **The neighbor's speed is not used.** The gap is judged from **my own speed**
  only (I'm the one who braked) via the shared cycle gaps.
- **`level > EEBL_WorstLevel`** (not `!=`) keeps the worst risk regardless of
  neighbor order, so CRITICAL is never downgraded to WARNING.

---

## 5. Full Scenario (worked example)

I'm moving at 3 m/s with a car 60 cm behind me:

1. **Cycle 1:** my speed is 3 m/s, `EEBL_PrevSpeed = 3`.
2. **Cycle 2:** I brake suddenly → speed becomes 2.5 m/s. `decel = -0.5 ≤ -0.20` →
   `BrakingDetected = 1`.
3. SafetyEngine cycle gaps at 2.5 m/s: safe = 2.5 × 35 = 87.5 cm,
   critical = 87.5 × 0.6 = 52.5 cm.
4. Same-direction neighbor ✔ + braking ✔ + rear 60 cm (< 87.5, > 52.5) → **WARNING**.
5. `EEBL_u8GetFlag()` returns 1 → the caller lights its rear warning.

> **When does it go back to SAFE?** On a cycle with no sudden brake, or once the car
> behind pulls beyond the safe distance — `EEBL_WorstLevel` resets to SAFE every
> BeginCycle and is only raised again if braking + a close car both hold.

---

## 6. Quick Summary

| Point | Takeaway |
|-------|----------|
| Goal | Warn when I brake suddenly with a car close behind |
| Sensor | Rear ultrasonic + my speed |
| Trigger | sudden-braking detection (decel ≤ -0.20) |
| Model | distance-based on *my* speed (shared cycle gaps, same as FCW) |
| Direction | only same-direction cars (DIR_SAME) |
| Output | `EEBL_u8GetFlag()` → 0/1/2 (LED/buzzer driven externally) |
| DSRC flag | **none** — fully local module |
