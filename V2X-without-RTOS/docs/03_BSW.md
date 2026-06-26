# BSW — Blind Spot Warning

> **Files:** [`Src/BSW_program.c`](../Src/BSW_program.c) · [`Inc/BSW/BSW_interface.h`](../Inc/BSW/BSW_interface.h) · [`Inc/BSW/BSW_config.h`](../Inc/BSW/BSW_config.h) · [`Inc/BSW/BSW_private.h`](../Inc/BSW/BSW_private.h)

---

## 1. What does this module do? (the idea, simply)

The **blind spot** is the area beside/behind your car that your mirrors don't cover. If a car is sitting there and you change lanes, you crash. **BSW**'s job is to warn you when a car is in your blind spot.

The clever part here is that BSW is **cooperative and role-based**. It splits the job between two cars:

- **Sender role:** a car that *sees another car alongside it (ahead)* using its **front-side** sensors broadcasts a `bsw_flag` telling which side it saw.
- **Receiver role:** a car that *receives* such a flag checks its **rear-side** sensor on the *mirrored* side to figure out whether it is the one sitting in the sender's blind spot.

> Every car plays **both roles every cycle** — it may be a sender for one neighbor and a receiver for another.

---

## 2. The mirrored-side idea (the heart of BSW)

This is the trickiest concept. Picture two cars side by side:

- The **sender** sees a car on its **front-left** → so it is the one *ahead*, and the other car is *behind it on the sender's left*.
- From the **other car's** (receiver's) point of view, that sender is *ahead of it on its right*. So the receiver should look at its **rear-right** to find itself in the sender's blind spot.

That's why the sides are mirrored:

```text
sender broadcasts LEFT  (saw front-left)  → receiver checks its REAR-RIGHT
sender broadcasts RIGHT (saw front-right) → receiver checks its REAR-LEFT
```

If the receiver finds something on that mirrored rear side → it *is* the car in the sender's blind spot → raise a BSW warning on that side.

---

## 3. Inputs and Outputs

### Inputs

| Input | Source | Role |
|-------|--------|------|
| `front_left`, `front_right` | front-side ultrasonics | sender (detect car alongside-ahead) |
| `rear_left`, `rear_right` | rear-side ultrasonics | receiver (am I in the blind spot?) |
| `n->bsw_flag` | neighbor's broadcast flag | receiver |

### Outputs

BSW has **two separate outputs**, each with its own getter:

| Output | Getter | Description |
|--------|--------|-------------|
| Sender flag | `BSW_u8GetFlag()` | broadcast over DSRC: bit0=LEFT, bit1=RIGHT (0=none, 1, 2, 3=both) — the side(s) *I* saw |
| Blind-spot result | `BSW_u8GetBlindSpot()` | the receiver-side result for *this* car: bit0=LEFT, bit1=RIGHT (0=none, 1=LEFT, 2=RIGHT, 3=both) |

> **This module only computes results** — it does **not** drive any LED/buzzer. The caller reads `BSW_u8GetBlindSpot()` to know which side(s) to alert, and drives the LED/buzzer itself.
>
> **Why two getters?** The *sender flag* is what I broadcast (I saw a car alongside me). The *blind-spot result* is the receiver decision shown on my own dashboard (a car is sitting in **my** blind spot). They are different things, so each has its own getter.

### Flag values (BSW_private.h)

`LEFT` and `RIGHT` are independent **bits**, so a car can flag both sides at once:

```c
#define BSW_FLAG_NONE  0x00
#define BSW_FLAG_LEFT  0x01   // I (sender) saw a car on my front-left
#define BSW_FLAG_RIGHT 0x02   // I (sender) saw a car on my front-right
#define BSW_FLAG_BOTH  (BSW_FLAG_LEFT | BSW_FLAG_RIGHT)
```

---

## 4. Configuration (BSW_config.h)

```c
#define BSW_SIDE_THRESHOLD (80.0f)  // an object is "present" if a side ultrasonic reads < this (cm)
```

A single threshold drives both roles: any side reading below it counts as a car next to me.

---

## 5. Lifecycle

### Stage 1: `BSW_voidBeginCycle(front_left, front_right, rear_left, rear_right)`

Two things happen:

**(a) Sender decision — from the FRONT-side sensors (one bit per side, no priority):**

```c
BSW_SenderFlag = BSW_FLAG_NONE;
if (front_left  > 0.0f && front_left  < BSW_SIDE_THRESHOLD)
  BSW_SenderFlag |= BSW_FLAG_LEFT;     // car alongside-ahead on my left
if (front_right > 0.0f && front_right < BSW_SIDE_THRESHOLD)
  BSW_SenderFlag |= BSW_FLAG_RIGHT;    // ... on my right
```

> Each side sets its own bit, so a car on **both** sides broadcasts `BSW_FLAG_BOTH`.
> There is no left/right priority anymore.

**(b) Reset the receiver-side alert accumulators:**

```c
BSW_AlertLeft  = 0;
BSW_AlertRight = 0;
```

### Stage 2: `BSW_voidProcessNeighbor(n)` — receiver role

For each **side bit** the neighbor broadcasts, we check the *mirrored* rear side.
The bits are tested independently (`&`), so a both-sides flag runs both checks:

```c
if (n->bsw_flag & BSW_FLAG_LEFT) {
  // sender saw us on its left → we sit on its rear → check OUR rear-right
  if (BSW_RearRight > 0.0f && BSW_RearRight < BSW_SIDE_THRESHOLD)
    BSW_AlertRight = 1;
}
if (n->bsw_flag & BSW_FLAG_RIGHT) {
  // sender saw us on its right → check OUR rear-left
  if (BSW_RearLeft > 0.0f && BSW_RearLeft < BSW_SIDE_THRESHOLD)
    BSW_AlertLeft = 1;
}
```

> The direction is **not** used. Blind-spot pairing is resolved purely by the
> mirrored-side check, not by heading.

There is **no EndCycle**: the per-side flags are accumulated here and exposed
directly through the getters.

> The **sender flag** is set in BeginCycle and read by `BSW_u8GetFlag()` for the
> DSRC broadcast. The **blind-spot result for this car** (`BSW_u8GetBlindSpot()`)
> is the receiver decision. Driving the LED/buzzer from those two getters is done
> outside the module.

---

## 6. Full Scenario (worked example)

Two cars in adjacent lanes. **Car A** is slightly ahead, **Car B** is slightly behind and to A's right-rear (i.e. B is in A's right blind spot... but let's drive it from the flags).

1. **Car A (sender):** its **front-right** sensor reads 120 cm (sees B alongside-ahead on its right) → `BSW_SenderFlag = BSW_FLAG_RIGHT` → broadcasts `bsw_flag = 2`.
2. **Car B (receiver):** receives A's `bsw_flag = 2` (RIGHT). Mirrored rule → B checks its **rear-left**. B's rear-left reads 100 cm (< 150) → `BSW_AlertLeft = 1`.
3. **Result on Car B:** `BSW_u8GetBlindSpot()` returns `1` (LEFT bit set). The caller reads that and lights B's **left** blind-spot warning — telling B's driver "there's a vehicle (A) on your left; don't move left."

---

## 7. Quick Summary

| Point | Takeaway |
|-------|----------|
| Goal | Warn of a car in the blind spot |
| Sensors | 4 side ultrasonics (front-L/R, rear-L/R) + DSRC |
| Sender role | front-side sensors → broadcast which side I saw |
| Receiver role | mirrored rear-side check → am I in the sender's blind spot? |
| Mirror rule | sender LEFT → my rear-right; sender RIGHT → my rear-left |
| Direction used? | No — pairing is by mirrored side, not heading |
| Sender flag | `BSW_u8GetFlag()` → `bsw_flag`: bit0=LEFT, bit1=RIGHT (0/1/2/3=both) |
| Blind-spot result | `BSW_u8GetBlindSpot()` → bit0=LEFT, bit1=RIGHT (LED driven externally) |
