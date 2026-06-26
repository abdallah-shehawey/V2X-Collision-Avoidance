# SafetyEngine — The Central Engine & the Single-Pass Pattern

> **Files:** [`Src/SafetyEngine_program.c`](../Src/SafetyEngine_program.c) · [`Inc/SafetyEngine/SafetyEngine_interface.h`](../Inc/SafetyEngine/SafetyEngine_interface.h)

The SafetyEngine is the **conductor**. It owns the shared host data, runs all 5 safety modules over the neighbor table in **one pass**, and provides the shared helper functions (direction detection + risk assessment) that every module reuses.

---

## 1. Why does the SafetyEngine exist?

Without it, each module would iterate the neighbor table itself:

```text
FCW:  for each neighbor { ... }   ← loop 1
EEBL: for each neighbor { ... }   ← loop 2
BSW:  for each neighbor { ... }   ← loop 3
DNPW: for each neighbor { ... }   ← loop 4
IMA:  for each neighbor { ... }   ← loop 5
```

That's 5 passes over the same data, and each one recomputes the neighbor's **direction** (`DetectDirection`) independently. The SafetyEngine collapses this into **one pass** and computes the direction **once per neighbor**, sharing it with all modules.

---

## 2. Shared Host Data

The engine **defines** the global host state that all modules read:

```c
float US_Distances[US_SENSOR_COUNT];   // 6 ultrasonic readings (cm)
float Host_Speed   = 0.0f;             // m/s (from the MPU)
float Host_Heading = 0.0f;             // 0..360 degrees
float Host_DistToIntersection = 0.0f;  // cm
```

These are declared `extern` in [`System.h`](../Inc/System.h) and used everywhere. The six ultrasonic indices (front, front-left, front-right, rear, rear-left, rear-right) are also defined there.

> **Current state:** these stay at 0 until real sensor drivers update them. That's why, as shipped, the modules read SAFE — there's no speed and no distance yet. Wiring the MPU + ultrasonics is what brings them to life.

---

## 3. The Single-Pass Update — `SafetyEngine_voidUpdate()`

This is the function `main()` calls every loop. Each neighbor is processed once and
dispatched only to the modules its direction can affect; results are read later via
the getters (there is no EndCycle).

```c
void SafetyEngine_voidUpdate(void) {
  Neighbor *table = DSRC_GetTable();
  uint8_t   count = DSRC_GetCount();

  // Read all 6 ultrasonic distances once per cycle
  float front_dist  = US_Distances[US_FRONT];
  float rear_dist   = US_Distances[US_REAR];
  float front_left  = US_Distances[US_FRONT_LEFT];
  float front_right = US_Distances[US_FRONT_RIGHT];
  float rear_left   = US_Distances[US_REAR_LEFT];
  float rear_right  = US_Distances[US_REAR_RIGHT];

  // Speed-dependent safe/critical gaps for this cycle (shared by FCW + EEBL)
  SafetyEngine_SafeDist = Host_Speed * SAFE_DIST_PER_MS;
  if (SafetyEngine_SafeDist < MIN_SAFE_DISTANCE) SafetyEngine_SafeDist = MIN_SAFE_DISTANCE;
  SafetyEngine_CriticalDist = SafetyEngine_SafeDist * CRITICAL_RATIO;

  // 1) BEGIN — each module resets its per-cycle state
  FCW_DNPW_voidBeginCycle(front_dist);
  EEBL_voidBeginCycle();
  BSW_voidBeginCycle(front_left, front_right, rear_left, rear_right);
  IMA_voidBeginCycle();

  // 2) PROCESS — dispatch each neighbor by its direction
  for (uint8_t i = 0; i < count; i++) {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading); // once per neighbor

    if (dir == DIR_SAME) {
      FCW_DNPW_voidProcessSameDirection();
      EEBL_voidProcessNeighbor(rear_dist);
      BSW_voidProcessNeighbor(&table[i]);
    } else if (dir == DIR_OPPOSITE) {
      FCW_DNPW_voidProcessOppositeDirection(&table[i]);
    } else if (dir == DIR_CROSSING) {
      IMA_voidProcessNeighbor(&table[i]);
    }
  }
  // Results are read on demand via each module's getter — no EndCycle.
}
```

Notice the "compute once" points: the 6 ultrasonic distances and the speed-dependent
gaps are latched once per cycle, and `dir` is computed once per neighbor. Dispatching
by direction also means a module never wastes cycles rejecting a direction it doesn't
care about.

---

## 4. Shared Helper #1 — Direction Detection

Every module needs to know how a neighbor is oriented relative to me. This is computed once and shared.

```c
Direction_t SafetyEngine_DetectDirection(float my_heading, float other_heading);
```

It returns one of:

| Result | Meaning | Used by |
|--------|---------|---------|
| `DIR_SAME` | roughly same heading (within 30°) | EEBL |
| `DIR_OPPOSITE` | roughly opposite (within 30° of 180°) | FCW, DNPW |
| `DIR_CROSSING` | roughly perpendicular (within 30° of 90°) | IMA |
| `DIR_UNKNOWN` | none of the above | — |

The logic (via the `CalcHeadingDiff` helper that normalizes the angle difference to `[0, 180]`):

```c
float diff = CalcHeadingDiff(my_heading, other_heading);
if (diff <= 30)                 return DIR_SAME;        // HEADING_SAME_THRESHOLD
if (diff >= 180 - 30)           return DIR_OPPOSITE;    // HEADING_OPPOSITE_THRESHOLD
if (diff >= 90-30 && diff <= 90+30) return DIR_CROSSING; // HEADING_CROSS_THRESHOLD
return DIR_UNKNOWN;
```

The 30° thresholds are defined in [`SafetyEngine_interface.h`](../Inc/SafetyEngine/SafetyEngine_interface.h).

---

## 5. Shared Helper #2 — Distance-Based Risk

The model used by FCW, EEBL, and DNPW:

```c
RiskLevel_t SafetyEngine_AssessDistanceRisk(float host_speed, float distance,
                                            float dist_per_ms, float min_dist,
                                            float crit_ratio);
```

```c
if (distance <= 0.0f) return RISK_SAFE;          // no valid reading → nothing in range

float safe_dist = host_speed * dist_per_ms;      // speed-dependent safe gap
if (safe_dist < min_dist) safe_dist = min_dist;  // floor

float critical_dist = safe_dist * crit_ratio;
if (distance < critical_dist) return RISK_CRITICAL;
if (distance < safe_dist)     return RISK_WARNING;
return RISK_SAFE;
```

Each module passes its **own tuned constants** (`dist_per_ms`, `min_dist`, `crit_ratio` from its `config.h`), so they share one formula but keep independent calibration.

---

## 6. Shared Helper #3 — Threshold Risk (for IMA)

A generic "lower value = higher risk" evaluator, used by IMA for **time-gap / delay** thresholds (seconds), where smaller delay = more urgent:

```c
RiskLevel_t SafetyEngine_EvaluateRisk(float value, float warning_thr, float critical_thr) {
  if (value < 0.0f)          return RISK_SAFE;
  if (value <= critical_thr) return RISK_CRITICAL;
  if (value <= warning_thr)  return RISK_WARNING;
  return RISK_SAFE;
}
```

> Only IMA uses this evaluator (for its time-to-intersection thresholds). FCW and
> EEBL use the distance-based gaps instead.

---

## 7. Initialization — `SafetyEngine_voidInit()`

Called once from `main()` before the loop; just initializes all modules:

```c
void SafetyEngine_voidInit(void) {
  FCW_DNPW_voidInit();
  EEBL_voidInit();
  BSW_voidInit();
  IMA_voidInit();
}
```

---

## 8. How it all ties together (end-to-end)

```text
main()
 ├─ System_Init()              (clocks, GPIO, USART, DSRC)
 ├─ SafetyEngine_voidInit()    (init all modules)
 └─ while(1):
     ├─ DSRC_Update()              ← drain RX queue → neighbor table
     ├─ SafetyEngine_voidUpdate()  ← SINGLE PASS over the table:
     │     Begin → (Process × neighbors, dispatched by direction)
     │     results read on demand via getters
     ├─ read local flags (LED/buzzer), build self Neighbor, attach cooperative flags
     ├─ DSRC_SendNeighbor(&self)   ← broadcast my state
     └─ SYSTIC_vDelayMs(500)
```

---

## 9. Quick Summary

| Point | Takeaway |
|-------|----------|
| Role | Conductor: owns host data, runs all modules in one pass |
| Pattern | BeginCycle → ProcessNeighbor (×neighbors); results read via getters |
| Key optimization | direction computed once per neighbor, then dispatched by direction |
| Shared host data | `Host_Speed`, `Host_Heading`, `US_Distances[6]`, `Host_DistToIntersection` |
| Shared cycle gaps | `SafetyEngine_SafeDist` / `CriticalDist` (FCW + EEBL) |
| Helper 1 | `DetectDirection` (same/opposite/crossing/unknown) |
| Helper 2 | `AssessDistanceRisk` (distance model) |
| Helper 3 | `EvaluateRisk` (time/delay model — IMA) |
