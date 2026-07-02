# V2X Graduation Project — Full Code Review Report

**Date:** 2026-07-02
**Scope:** Everything except `V2V-STM32/` — i.e. `Traffic_Light/`, `RPI/` (hub, V2N, V2P, DashBoard, Control, run_all.sh), and `esp32/`.

Severity legend:
- 🔴 **CRITICAL** — crashes, deadlocks, or features that can never work.
- 🟠 **HIGH** — wrong behavior in real scenarios.
- 🟡 **MEDIUM** — performance problems, resource leaks, reliability gaps.
- 🔵 **LOW** — code quality, duplication, style.

---

## Issue Index

| # | Severity | File | Problem |
|---|----------|------|---------|
| 1 | 🔴 | Traffic_Light/Intelligent_Gateway.py | `BROKER/PORT/USERNAME/PASSWORD` never defined → instant `NameError` crash |
| 2 | 🔴 | Traffic_Light/Intelligent_Gateway.py | ABBA deadlock between `state_lock` and `registry_lock` |
| 3 | 🔴 | RPI/V2P/V2P.py | Motorcycle collision alert can **never** fire (dead code) |
| 4 | 🔴 | Traffic_Light (all) | `AMBULANCE_ID` mismatch: Gateway=`"REX"`, cameras=`"T4RR"` → emergency latch bug |
| 5 | 🟠 | Traffic_Light/Intelligent_Gateway.py | `camera_ambulance` stays stuck ON while other cars are visible |
| 6 | 🟠 | Traffic_Light/Intelligent_Gateway.py | Legacy camera check triggers on "**no** ambulance" messages too |
| 7 | 🟠 | Traffic_Light/Intelligent_Gateway.py | Emergency warning reports the distance of the *wrong* vehicle |
| 8 | 🟠 | Traffic_Light/Traffic_light_GUI.py | Tkinter widgets touched from the MQTT network thread (not thread-safe) |
| 9 | 🟠 | Traffic_Light/Traffic_light_GUI.py | Double publish at every phase start + ~1.7 s drift per phase |
| 10 | 🟠 | Traffic_Light/Distance.py | Infinite video loop + `VideoWriter` → output file grows until the disk is full |
| 11 | 🟠 | Traffic_Light/Distance.py | Real OCR confidence is thrown away and replaced by a fake formula |
| 12 | 🟠 | RPI/hub/ipc_node.py | Concurrent `publish()` from two threads corrupts the socket stream |
| 13 | 🟠 | RPI/hub/ipc_node.py | `_recv_one()` throws away trailing bytes → can silently drop a delivered frame |
| 14 | 🟠 | RPI/hub/hub.py | Two publishers writing to the same subscriber socket can interleave frames |
| 15 | 🟠 | RPI/V2P/V2P.py | `CentroidTracker.history` never cleaned on deregister → memory leak |
| 16 | 🟡 | RPI (system-wide) | Publish cascade floods the hub + writes data.json ~30–40×/sec → SD-card wear |
| 17 | 🟡 | RPI/V2P/V2P.py | `skip_frames = 1` ("optimized") actually makes the Pi inference-bound |
| 18 | 🟡 | RPI/V2N/Car_client.py | `os.system('clear')` spawns a shell on every MQTT message |
| 19 | 🟡 | esp32/master+slave | Verbose `printf` inside RX callbacks — the very cause of the UART overflow that was patched around |
| 20 | 🟡 | RPI/hub/ipc_node.py | No reconnect: if the hub restarts, every node stays dead silently |
| 21 | 🟡 | Traffic_Light/Distance.py + dis&AI_camera.py | MQTT auth failures reported as "✅ connected" |
| 22 | 🟡 | RPI/V2P/model2.onnx | Model file in git is **0 bytes** — V2P will crash on load after a fresh clone |
| 23 | 🔵 | esp32/master.ino + slave.ino | 99% duplicated files — only `VEHICLE_ID` differs |
| 24 | 🔵 | Traffic_Light | Distance.py and dis&AI_camera.py duplicate the whole pipeline with subtle inconsistencies |
| 25 | 🔵 | 5 files | MQTT credentials hardcoded (currently `***REMOVED***` placeholders → nothing can connect) |
| 26 | 🔵 | esp32/Storing_algorithm.cpp | Test overwrites its own test data — doesn't test what the comment claims |
| 27 | 🔵 | RPI/Control/control_server.py | Reverse ("B") is never safety-blocked; no auth on the drive endpoint |
| 28 | 🔵 | RPI/run_all.sh | `wait` waits for *all* children (incl. `tail`), not "any child" as the comment says |

---

# 🔴 CRITICAL ISSUES

## Issue 1 — `Intelligent_Gateway.py` crashes on startup (`NameError`)

**File:** `Traffic_Light/Intelligent_Gateway.py`, lines 286 and 297.

The entry point uses four variables that are **never defined anywhere in the file**. The script cannot even start — it dies immediately with `NameError: name 'USERNAME' is not defined`. Every other Traffic_Light script defines these constants; this one lost them (probably during a refactor/redaction).

**Old code (current — broken):**
```python
# ============================================================
# ENTRY POINT
# ============================================================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)      # ← NameError: USERNAME undefined
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
...
client.connect(BROKER, PORT, 60)                # ← BROKER / PORT undefined too
```

**New code (fixed):**
```python
# ============================================================
# MQTT SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = os.environ.get("V2X_MQTT_PASSWORD", "")   # see Issue 25 — don't hardcode

# ... rest of the file unchanged ...
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
```

**Why:** the four constants must exist before `username_pw_set()`/`connect()` run. Best fix is a single shared `mqtt_config.py` used by all five scripts (see Issue 25) so this class of bug can't happen again.

---

## Issue 2 — ABBA deadlock between `state_lock` and `registry_lock`

**File:** `Traffic_Light/Intelligent_Gateway.py`.

Two threads acquire the same two locks in **opposite order**. This is the textbook ABBA deadlock: once it happens, the gateway freezes forever (no publishes, no cleanup) with no error message.

- `process_and_publish()` (called from MQTT callbacks): takes `state_lock` **then** `registry_lock`:

```python
def process_and_publish():
    with state_lock:            # ① takes state_lock first
        with registry_lock:     # ② then registry_lock
            nearby_vehicles = {...}
```

- `cleanup_registry()` (background thread, every 2 s): takes `registry_lock` **then** `state_lock`:

```python
def cleanup_registry():
    while True:
        time.sleep(2)
        with registry_lock:         # ① takes registry_lock first
            ...
            for pid in to_delete:
                ...
                if pid == AMBULANCE_ID:
                    with state_lock:    # ② then state_lock — DEADLOCK WINDOW
                        camera_ambulance = False
```

If an MQTT message arrives while the cleanup loop is inside `registry_lock`, the MQTT thread grabs `state_lock` and waits for `registry_lock`, while cleanup holds `registry_lock` and waits for `state_lock`. Both wait forever.

**New code (fixed) — never hold both locks at once in cleanup; collect first, mutate after:**
```python
def cleanup_registry():
    global camera_ambulance
    while True:
        time.sleep(2)
        current_time = int(time.time())
        ambulance_expired = False

        with registry_lock:                       # ONLY registry_lock here
            to_delete = [pid for pid, data in vehicle_registry.items()
                         if current_time - data.get("last_seen", 0) > 5]
            for pid in to_delete:
                print(f"🧹 Clearing stale vehicle data: {pid}")
                data = vehicle_registry.pop(pid, None)
                if pid == AMBULANCE_ID or (data and data.get("is_ambulance")):
                    ambulance_expired = True
            registry_empty = not vehicle_registry

        # state_lock taken AFTER registry_lock is fully released
        if ambulance_expired or registry_empty:
            with state_lock:
                camera_ambulance = False
```

**Why:** the fix enforces one global rule — *these two locks are never nested in this thread* — which removes the inverted ordering. (`process_and_publish` keeps its nesting; it's now the only place that nests, always in the same order, which is safe.) Note the `data.get("is_ambulance")` check also fixes Issue 5.

---

## Issue 3 — Motorcycle collision alert is dead code (can never fire)

**File:** `RPI/V2P/V2P.py`.

The motorcycle check requires `risk_level == "HIGH"`, but `analyze_intent()` **returns `"LOW"` for anything that is not a person** (`class_id != 0`). A motorcycle is class 3, so `risk_level` is always `"LOW"` for it → `frame_moto_flag` can never become 1 → the `motorcycle_alert` topic, the dashboard `motorcycleCollision` flag, and the control-server MOTORCYCLE block are all connected to a signal that never triggers.

**Old code (current — broken):**
```python
def analyze_intent(obj_id, history, class_id, bbox):
    if class_id != 0:
        return None, "LOW", (0, 255, 0)      # ← motorcycles always come out "LOW"
    ...

# main loop:
if class_id == 3:   # motorcycle
    in_zone = (y1+y2)//2 > zone_y
    if in_zone and prox_level == "DANGER" and risk_level == "HIGH":   # ← impossible
        frame_moto_flag = 1
```

**New code (fixed) — base the motorcycle risk on motion it can actually have (speed from the tracker), not on the person-only intent label:**
```python
# main loop:
if class_id == 3:   # motorcycle
    in_zone = (y1 + y2) // 2 > zone_y
    if in_zone and prox_level == "DANGER":
        # A motorcycle in the crossing zone at DANGER proximity is the risk
        # scenario. Optionally require that it is actually moving:
        pts = list(tracker.history[obj_id])
        moving = False
        if len(pts) >= 3:
            d = [math.hypot(pts[i][0]-pts[i-1][0], pts[i][1]-pts[i-1][1])
                 for i in range(1, len(pts))]
            moving = (sum(d[-3:]) / min(3, len(d))) > 1.0
        if moving:
            frame_moto_flag = 1
```

Alternatively (simpler): let `analyze_intent()` also compute speed-based risk for classes 1 and 3 instead of returning early. Either way, the condition must be satisfiable by a motorcycle.

**Why:** the whole motorcycle-safety chain (V2P → hub → dashboard_bridge → data.json → dashboard card + control-server forward-block) currently exists but is unreachable. This one-line logic error silently disables an entire advertised feature.

---

## Issue 4 — `AMBULANCE_ID` mismatch across the Traffic_Light system

**Files:** `Intelligent_Gateway.py` (`"REX"`), `Distance.py` (`"T4RR"`), `dis&AI_camera.py` (`"T4RR"`).

The cameras detect and tag plate `T4RR` as the ambulance, but the Gateway's own identity checks look for `REX`:

**Old code (current):**
```python
# Intelligent_Gateway.py
AMBULANCE_ID = "REX"

# Distance.py / dis&AI_camera.py
AMBULANCE_ID = "T4RR"
```

Consequences inside the Gateway:
- `plate_id.strip() == AMBULANCE_ID` (line 224) never matches the real ambulance plate — emergency only works because the cameras also send `is_ambulance: true`.
- `cleanup_registry()` clears `camera_ambulance` only when `pid == "REX"` expires — the real `T4RR` record expiring does **not** clear it (see Issue 5).
- V2X radio path: `v_id.strip() == AMBULANCE_ID` compares an ESP/vehicle id against the wrong constant.

**New code (fixed):** one shared constant in a config module imported by all three scripts:
```python
# Traffic_Light/v2x_config.py  (NEW FILE)
BROKER       = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT         = 8883
USERNAME     = "v2n_admin"
PASSWORD     = os.environ.get("V2X_MQTT_PASSWORD", "")
AMBULANCE_ID = "T4RR"          # single source of truth

# in each script:
from v2x_config import BROKER, PORT, USERNAME, PASSWORD, AMBULANCE_ID
```

**Why:** duplicated constants always drift — this one already did, and it silently weakened the emergency-detection redundancy (plate match + flag match) down to a single path.
