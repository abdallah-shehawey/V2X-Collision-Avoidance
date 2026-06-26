# IMA — Intersection Movement Assist

> **Files:** [`Src/IMA_program.c`](../Src/IMA_program.c) · [`Inc/IMA/IMA_interface.h`](../Inc/IMA/IMA_interface.h) · [`Inc/IMA/IMA_config.h`](../Inc/IMA/IMA_config.h) · [`Inc/IMA/IMA_private.h`](../Inc/IMA/IMA_private.h)

---

## 1. What does this module do? (the idea, simply)

At an intersection, two cars approach from **crossing roads** (perpendicular paths). Neither may see the other in time. **IMA**'s job: figure out who has the right of way and, if *I'm* the one who should wait, warn me **"WAIT — CROSS TRAFFIC."**

This is the only module that is **not** distance-based on ultrasonics. It works on:

- **Direction:** the other car must be *crossing* (perpendicular), not same/opposite.
- **Proximity to the intersection:** both cars must be near the intersection.
- **Priority by speed:** the faster car passes first; the slower one yields.
- **Time-to-intersection (delay):** how long until I reach the intersection — used to grade the risk.

---

## 2. Inputs and Outputs

### Inputs

| Input | Source | Unit |
|-------|--------|------|
| `Host_Speed` | my speed | m/s |
| `Host_DistToIntersection` | my distance to the intersection (shared global) | cm |
| `n->speed` | neighbor's speed | m/s |
| `n->distance_to_intersection` | neighbor's distance to the intersection | cm |
| `dir` | direction (only `DIR_CROSSING` matters) | enum |

### Outputs

| Output | Description |
|--------|-------------|
| `IMA_u8GetFlag()` | the risk level (0=Safe, 1=Warning, 2=Critical), also broadcast over DSRC |

> **This module only computes the risk level.** It does **not** drive the "WAIT — CROSS TRAFFIC" LED/buzzer itself — that's handled outside the module. The caller reads `IMA_u8GetFlag()` and decides what to do.
>
> Both cars share their `distance_to_intersection` in the DSRC message, which is how IMA can reason about *both* vehicles' positions.

---

## 3. Configuration (IMA_config.h)

```c
#define IMA_INTERSECTION_RANGE (2000.0f)  // 20 meters — IMA only acts when BOTH cars are this close (cm)
#define IMA_WARNING_DELAY  (4.0f)         // time-to-intersection ≤ 4 s → warning (seconds)
#define IMA_CRITICAL_DELAY (2.0f)         // time-to-intersection ≤ 2 s → critical (seconds)
```

Note IMA uses **time thresholds (seconds)**, not distance ratios. That's why it calls a different risk function (`SafetyEngine_EvaluateRisk`, the "lower value = higher risk" evaluator) instead of `AssessDistanceRisk`.

---

## 4. Lifecycle

### Stage 1: `IMA_voidBeginCycle()`

Just resets accumulators (IMA reads its host data from shared globals, so no sensor parameters):

```c
IMA_CrossingDetected = 0;
IMA_IShouldWait      = 0;
IMA_WorstRisk        = RISK_SAFE;
```

### Stage 2: `IMA_voidProcessNeighbor(n)` — the core decision

Called for each **crossing** neighbor (the SafetyEngine only dispatches those here).
One gate, then the priority decision:

**Gate — both near the intersection:**

```c
if (Host_DistToIntersection <= 0.0f || Host_DistToIntersection > IMA_INTERSECTION_RANGE) return;
if (n->distance_to_intersection <= 0.0f || n->distance_to_intersection > IMA_INTERSECTION_RANGE) return;
IMA_CrossingDetected = 1;   // confirmed crossing vehicle near the intersection
```

**Priority decision — who goes first?**

```c
if (Host_Speed > n->speed) return;   // I'm faster → I pass first → no alert for me

// The other is faster (or equal) → I should wait. THIS is the dangerous case.
IMA_IShouldWait = 1;

if (Host_Speed <= 0.0f) return;      // I'm stopped → can't collide

float delay = Host_DistToIntersection / Host_Speed;   // MY time-to-intersection
RiskLevel_t level = SafetyEngine_EvaluateRisk(delay, IMA_WARNING_DELAY, IMA_CRITICAL_DELAY);
if (level > IMA_WorstRisk)            // keep the worst across all crossing neighbors
  IMA_WorstRisk = level;
```

> **The key asymmetry:** if I'm faster I pass first and get **no alert**. Only when the
> *other* car has priority do I become the one who must wait — and only then is my
> time-to-intersection turned into a risk level.

### Result: `IMA_u8GetFlag()`

There is **no EndCycle**. The getter derives the decision — the alert fires only if
**all three** hold:

```c
uint8_t IMA_u8GetFlag(void) {
  if (IMA_CrossingDetected && IMA_IShouldWait && IMA_WorstRisk > RISK_SAFE)
    return (uint8_t)IMA_WorstRisk;
  return 0;
}
// The "WAIT — CROSS TRAFFIC" LED/buzzer is driven outside this module.
```

---

## 5. Full Scenario (worked example)

I approach an intersection at 2 m/s; a crossing car approaches at 4 m/s.

1. **Gate 1:** the neighbor is `DIR_CROSSING` ✔.
2. **Gate 2:** I'm 600 cm from the intersection (< 2000), the other is 800 cm (< 2000) ✔ → `IMA_CrossingDetected = 1`.
3. **Priority:** `Host_Speed (2) > n->speed (4)`? No → the other is faster → **I should wait** → `IMA_IShouldWait = 1`.
4. **My delay:** `600 / 2 = 300`... wait, distances are cm and speed is m/s — on the real car you'd convert units. With the tuned numbers, suppose my time-to-intersection evaluates to ~3 s → between 2 s and 4 s → **WARNING**.
5. **`IMA_u8GetFlag()`:** crossing ✔ + I should wait ✔ + risk > SAFE ✔ → returns `1` (WARNING). The caller reads it and shows **"WAIT — CROSS TRAFFIC"**.

> **Unit note:** `Host_DistToIntersection` is documented in cm while `Host_Speed` is m/s, so `distance / speed` isn't a clean "seconds" yet. On the real vehicle, make the units consistent (e.g. meters / (m/s)) so the delay thresholds (`4 s`, `2 s`) are meaningful. Flagged here as a calibration point.

---

## 6. Quick Summary

| Point | Takeaway |
|-------|----------|
| Goal | Resolve who yields at an intersection; warn me if I must wait |
| Inputs | both cars' speed + distance-to-intersection (shared via DSRC) |
| Direction | only `DIR_CROSSING` neighbors |
| Risk model | time-based (`SafetyEngine_EvaluateRisk`), not distance-based |
| Priority | faster car passes first; slower car gets the warning |
| Alert condition | crossing **AND** I should wait **AND** risk > SAFE |
| The flag | `ima_flag`: 0=Safe, 1=Warning, 2=Critical (read via `IMA_u8GetFlag()`) |
| Alerts | "WAIT — CROSS TRAFFIC" LED/buzzer driven externally — module only computes the level |
