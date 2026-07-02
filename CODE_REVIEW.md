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
| 29 | 🟠 | RPI/DashBoard/js/app.js | Every alarm/beep is **silent** until the page is tapped once (browser autoplay policy) |
| 30 | 🟡 | server.py + dashboard_bridge.py | Two processes write data.json through the **same** `.tmp` path with no cross-process lock |

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

---

# 🟠 HIGH ISSUES

## Issue 5 — `camera_ambulance` stays latched ON while other cars remain visible

**File:** `Traffic_Light/Intelligent_Gateway.py`, `cleanup_registry()`.

Once the camera sets `camera_ambulance = True`, there are only two ways it clears:
1. a registry entry with `pid == AMBULANCE_ID` ( = `"REX"`, which never exists — see Issue 4) expires, or
2. the **entire** registry becomes empty.

So in the realistic scene "ambulance passed, but normal cars are still driving by", the emergency stays ON forever: the ambulance record (`T4RR`) expires without matching `"REX"`, and the registry is never empty because normal cars keep refreshing it. The junction stays in emergency mode indefinitely.

**Old code (current — broken):**
```python
for pid in to_delete:
    print(f"🧹 Clearing stale vehicle data: {pid}")
    vehicle_registry.pop(pid, None)
    if pid == AMBULANCE_ID:            # "REX" — never matches "T4RR"
        with state_lock:
            camera_ambulance = False

if not vehicle_registry:               # only clears when EVERYTHING is gone
    with state_lock:
        camera_ambulance = False
```

**New code (fixed):** check the record's own `is_ambulance` flag (the fixed version shown in Issue 2 already includes this):
```python
data = vehicle_registry.pop(pid, None)
if pid == AMBULANCE_ID or (data and data.get("is_ambulance")):
    ambulance_expired = True
```
Even better: after every cleanup pass, recompute the flag from the registry itself instead of latching it:
```python
with registry_lock:
    any_ambulance = any(d.get("is_ambulance") for d in vehicle_registry.values())
with state_lock:
    camera_ambulance = any_ambulance
```

**Why:** deriving the flag from current registry contents makes it impossible to get stuck — the state is always a pure function of what the camera actually sees.

---

## Issue 6 — Legacy camera check fires on "NO ambulance" messages too

**File:** `Traffic_Light/Intelligent_Gateway.py`, `on_message()` / `CAMERA_TOPIC` branch.

**Old code (current — broken):**
```python
if "Ambulance verified" in payload_str or "ambulance" in payload_str.lower():
    with state_lock:
        camera_confirmed = True
```

The second condition matches **any** payload containing the word "ambulance", including `"no ambulance detected"`, `"ambulance cleared"`, `"searching for ambulance"` — all of these switch the junction into emergency mode.

**New code (fixed):** exact-match a small set of positive phrases (and make the protocol explicit):
```python
POSITIVE_PHRASES = ("ambulance verified", "ambulance detected", "ambulance confirmed")
NEGATIVE_HINTS   = ("no ambulance", "not ambulance", "cleared")

p = payload_str.lower()
is_positive = any(s in p for s in POSITIVE_PHRASES) and not any(s in p for s in NEGATIVE_HINTS)
with state_lock:
    camera_confirmed = is_positive
```
Best long-term fix: stop sending free text on this topic — send JSON `{"ambulance": true/false}` and parse it, like every other topic in the system already does.

**Why:** a substring check on a natural-language payload is an accidental emergency trigger. Emergency preemption stops all normal traffic — it must not fire on a negation.

---

## Issue 7 — Emergency warning reports the wrong vehicle's distance

**File:** `Traffic_Light/Intelligent_Gateway.py`, `process_and_publish()`.

`closest` is the closest vehicle **of any kind**. During an emergency the warning prints the ambulance's plate together with `closest`'s distance — if a normal car is nearer than the ambulance, drivers see *"AMBULANCE at 3 m"* while the real ambulance is 40 m away.

**Old code (current — broken):**
```python
if closest and closest.get("distance_m") is not None:
    dist_val = closest["distance_m"]     # ← distance of the closest ANY vehicle
    output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}] at {dist_val}m! ..."
```

**New code (fixed):** look up the ambulance's own registry entry for the distance:
```python
emergency_plate, emergency_dist = None, None
with registry_lock:
    for pid, data in vehicle_registry.items():
        if pid == AMBULANCE_ID or data.get("is_ambulance"):
            emergency_plate = pid
            emergency_dist  = data.get("distance_m")
            break

if emergency_dist is not None:
    output["warning"] = (f"🚨 AMBULANCE APPROACHING [{emergency_plate}] "
                         f"at {emergency_dist}m! NORMAL CARS MUST STOP! 🚨")
else:
    output["warning"] = (f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}]! "
                         f"NORMAL CARS MUST STOP! 🚨")
```

**Why:** the message is consumed by `Car_client.py` and shown to drivers — it should describe the emergency vehicle, not whichever car happens to be closest to the camera.

---

## Issue 8 — Tkinter widgets updated from the MQTT network thread

**File:** `Traffic_Light/Traffic_light_GUI.py`.

`_on_mqtt_connect()` runs on **paho's network thread** (started by `loop_start()`), and it calls `_publish_state()`, which does `self.code_label.config(...)`. Tkinter is not thread-safe: touching widgets from a non-main thread causes intermittent crashes (`RuntimeError: main thread is not in main loop`) or silent UI corruption. The file already does this correctly elsewhere (`set_connection_status` uses `root.after(0, ...)`) — this path just misses it.

**Old code (current — broken):**
```python
def _on_mqtt_connect(self, client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        self.mqtt_connected = True
        self.set_connection_status(True)
        self._publish_state()          # ← runs on MQTT thread; touches code_label
```

**New code (fixed):** hop back to the Tk main loop, same pattern as `set_connection_status`:
```python
def _on_mqtt_connect(self, client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        self.mqtt_connected = True
        self.set_connection_status(True)
        self.root.after(0, self._publish_state)   # ← schedule on the Tk thread
```

**Why:** every widget access must happen on the thread running `mainloop()`. `root.after(0, fn)` is the standard thread-safe handoff.

---

## Issue 9 — Double publish at each phase start + phase-length drift

**File:** `Traffic_Light/Traffic_light_GUI.py`, `_run_sim_step()` / `_sim_tick()`.

Two problems:

1. **Duplicate publish:** `_run_sim_step()` calls `_apply_update(state, duration)` (publish #1), then immediately calls `_sim_tick(duration)`, which calls `_apply_update(self.state, remaining)` again with the same values (publish #2). Every phase change sends the same MQTT packet twice back-to-back.
2. **Drift:** at `remaining <= 0` the code waits an extra `after(700, ...)` before the next phase, so every phase actually lasts `duration + ~1.7 s` (the 700 ms pause + the final 1 s tick at zero). Downstream, `Car_client.py` uses `remaining_time` for its can-I-cross calculation — the light says "0 seconds left" for almost two real seconds.

**Old code (current):**
```python
def _run_sim_step(self):
    if not self.simulation_active:
        return
    state, duration = self.SIM_SEQUENCE[self.sim_index % len(self.SIM_SEQUENCE)]
    self._apply_update(state, duration)      # publish #1
    self._sim_tick(duration)                 # → immediately publishes again

def _sim_tick(self, remaining):
    if not self.simulation_active:
        return
    self._apply_update(self.state, remaining)   # publish #2 with identical data
    if remaining <= 0:
        self.sim_index += 1
        self.root.after(700, self._run_sim_step)   # +0.7 s dead time
    else:
        self.root.after(1000, lambda: self._sim_tick(remaining - 1))
```

**New code (fixed):**
```python
def _run_sim_step(self):
    if not self.simulation_active:
        return
    state, duration = self.SIM_SEQUENCE[self.sim_index % len(self.SIM_SEQUENCE)]
    self.state = state
    self._sim_tick(duration)                 # single entry point does the publish

def _sim_tick(self, remaining):
    if not self.simulation_active:
        return
    self._apply_update(self.state, remaining)
    if remaining <= 0:
        self.sim_index += 1
        self.root.after(0, self._run_sim_step)    # no artificial dead time
    else:
        self.root.after(1000, lambda: self._sim_tick(remaining - 1))
```

**Why:** each phase now publishes exactly once per second, phases last exactly their configured duration, and `remaining_time` seen by the cars matches wall-clock reality.

---

## Issue 10 — Video loops forever while `VideoWriter` keeps writing → disk fills up

**File:** `Traffic_Light/Distance.py`.

When the input video ends, the loop rewinds to frame 0 and keeps going **forever**, and *every* iteration writes a frame to `processed_output.mp4`. Left running (which is the intended demo mode), the output file grows without bound until the disk is full — on a Pi/SD card this also kills the card.

**Old code (current — broken):**
```python
ret, frame = cap.read()
if not ret:
    print("\n🔄 Video ended. Restarting loop...")
    cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
    ret, frame = cap.read()
    ...
# bottom of the loop — runs on EVERY frame, forever:
out.write(frame)
```

**New code (fixed):** only record the first pass through the video:
```python
total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
recording = True

ret, frame = cap.read()
if not ret:
    print("\n🔄 Video ended. Restarting loop...")
    if recording:
        recording = False
        out.release()               # first pass fully recorded — stop growing the file
        print(f"💾 Recording finished: {output_path}")
    cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
    ret, frame = cap.read()
    ...
# bottom of the loop:
if recording:
    out.write(frame)
```

**Why:** the MQTT publishing (the actual purpose of looping) keeps running forever, but disk usage is now bounded to one copy of the video.

---

## Issue 11 — Real OCR confidence discarded, replaced by a fake formula

**File:** `Traffic_Light/Distance.py`.

EasyOCR returns a real confidence per detection — the code receives it as `conf` and then ignores it, computing a fake "confidence" from the **string length** instead. There is also no minimum-confidence filter, so garbage reads (\"HIIB\", \"IIII\") pass straight through to MQTT and the CSV. The sister file `dis&AI_camera.py` does this correctly (`prob > 0.35`).

**Old code (current — broken):**
```python
for (bbox, text, conf) in ocr_results:
    cleaned = re.sub(r'[^A-Z0-9]', '', text.upper().replace(' ', ''))
    if len(cleaned) >= 4:
        ...
        confidence = min(95, 60 + len(cleaned) * 5)   # ← fake number, conf unused
```

**New code (fixed):**
```python
MIN_OCR_CONF = 0.35   # same threshold the live camera script already uses

for (bbox, text, conf) in ocr_results:
    cleaned = re.sub(r'[^A-Z0-9]', '', text.upper().replace(' ', ''))
    if len(cleaned) >= 4 and conf >= MIN_OCR_CONF:
        ...
        confidence = round(conf * 100)                # ← the real confidence
```

**Why:** the fake formula reports 80–95% for any random string, hiding bad reads; the missing threshold lets noise become "vehicles" in the Gateway registry (which then affects `nearby_count`, `closest_vehicle`, and the dashboard).

---

## Issue 12 — `IPCNode.publish()` is not thread-safe → corrupted frames on the hub

**File:** `RPI/hub/ipc_node.py` (affects `Car_client.py` directly).

`Car_client.py` calls `ipc.publish()` from **two different threads**: the paho MQTT thread (`on_message` → `_publish_v2n_frame`) and the IPC listener thread (`_on_vehicle_speed` → `_publish_v2n_frame`). Both end up in `_send_raw()` → `sock.sendall()` on the same socket with no lock. `sendall()` on a busy socket can write in several chunks, so two concurrent frames can interleave bytes → the hub receives corrupted JSON lines → `json.loads` raises → **the hub drops the whole client connection** (see hub.py's exception handling).

**Old code (current — broken):**
```python
def _send_raw(self, obj: dict) -> None:
    self.sock.sendall((json.dumps(obj) + "\n").encode("utf-8"))
```

**New code (fixed):** one write lock per node:
```python
def __init__(self, name: str) -> None:
    self.name = name
    self.sock = None
    self._callbacks: dict[str, list] = {}
    self._send_lock = threading.Lock()          # NEW

def _send_raw(self, obj: dict) -> None:
    data = (json.dumps(obj) + "\n").encode("utf-8")
    with self._send_lock:                       # NEW — one frame at a time
        self.sock.sendall(data)
```

**Why:** newline-framed protocols are only safe if each frame is written atomically with respect to other writers on the same fd. A per-socket mutex guarantees that.

---

## Issue 13 — `_recv_one()` can swallow a delivered frame

**File:** `RPI/hub/ipc_node.py`.

`_recv_one()` reads chunks until it sees a `\n`, then returns **only the first line and throws the rest of the buffer away**. If the hub's ack and a delivered pub/sub frame arrive in the same TCP segment (perfectly possible), the delivered frame is silently lost. Today all callers subscribe before `start_listening()`, so the window is small — but any subscribe-after-listen refactor also makes the ack race the listener thread.

**Old code (current — broken):**
```python
def _recv_one(self, timeout: float = 3.0) -> dict | None:
    self.sock.settimeout(timeout)
    try:
        buf = b""
        while b"\n" not in buf:
            chunk = self.sock.recv(512)
            if not chunk:
                return None
            buf += chunk
        self.sock.settimeout(None)
        return json.loads(buf.split(b"\n")[0])   # ← everything after the first \n is LOST
    except (socket.timeout, json.JSONDecodeError):
        return None
```

**New code (fixed):** keep the remainder in a shared buffer that `_recv_loop` starts from:
```python
def __init__(self, name: str) -> None:
    ...
    self._rx_buf = b""                    # NEW: shared receive buffer

def _recv_one(self, timeout: float = 3.0) -> dict | None:
    self.sock.settimeout(timeout)
    try:
        while b"\n" not in self._rx_buf:
            chunk = self.sock.recv(512)
            if not chunk:
                return None
            self._rx_buf += chunk
        self.sock.settimeout(None)
        line, self._rx_buf = self._rx_buf.split(b"\n", 1)   # keep the remainder
        return json.loads(line)
    except (socket.timeout, json.JSONDecodeError):
        return None

def _recv_loop(self) -> None:
    buf = self._rx_buf.decode("utf-8")    # NEW: start from what _recv_one left over
    self._rx_buf = b""
    while True:
        ...
```

**Why:** on a stream socket, message boundaries are yours to manage — discarding buffered bytes is equivalent to dropping random messages under load.

---

## Issue 14 — Hub can interleave writes to the same subscriber socket

**File:** `RPI/hub/hub.py`.

Each client is served by its own thread. When two publishers publish (each from its own handler thread) to topics that share a subscriber, both threads call `sub_conn.sendall(frame)` on the **same socket** concurrently — same interleaving risk as Issue 12, this time on the hub side. The subscriber then receives spliced JSON and its `json.loads` fails.

**Old code (current — broken):**
```python
clients: dict[str, socket.socket] = {}
...
sub_conn = clients.get(subscriber_name)
if sub_conn:
    try:
        sub_conn.sendall(frame.encode("utf-8"))
```

**New code (fixed):** store a write-lock with each connection:
```python
clients: dict[str, tuple[socket.socket, threading.Lock]] = {}

# on register:
with lock:
    clients[client_name] = (conn, threading.Lock())

# on deliver:
entry = clients.get(subscriber_name)
if entry:
    sub_conn, wlock = entry
    try:
        with wlock:                                   # serialize writes per socket
            sub_conn.sendall(frame.encode("utf-8"))
```
(Also update `_send(conn, …)` call sites to use the same per-connection lock where the target socket belongs to a registered client.)

**Why:** with more than two publishers (V2P at frame rate + Car_client at MQTT rate + uart_bridge at 10 Hz) concurrent fan-out to `dashboard_bridge` is the *normal* case, not the corner case.

---

## Issue 15 — Tracker memory leak: `history` never cleaned

**File:** `RPI/V2P/V2P.py`, `CentroidTracker`.

`deregister()` removes the object from `objects`, `objects_bbox`, and `disappeared` — but **not** from `history` (a `defaultdict`). Every vehicle/person that ever crossed the camera leaves a permanent `deque(maxlen=30)` behind. On a Pi running for hours at a demo, thousands of dead IDs accumulate (each with up to 30 coordinate tuples) — slow, unbounded memory growth in a long-running safety process.

**Old code (current — broken):**
```python
def deregister(self, obj_id: int) -> None:
    self.objects.pop(obj_id, None)
    self.objects_bbox.pop(obj_id, None)
    self.disappeared.pop(obj_id, None)
    # history[obj_id] stays forever
```

**New code (fixed):**
```python
def deregister(self, obj_id: int) -> None:
    self.objects.pop(obj_id, None)
    self.objects_bbox.pop(obj_id, None)
    self.disappeared.pop(obj_id, None)
    self.history.pop(obj_id, None)      # NEW: free the motion trail too
```

**Why:** `history` is keyed by the same IDs and has no other cleanup path; one line closes the leak.

---

# 🟡 MEDIUM ISSUES (performance / reliability)

## Issue 16 — System-wide publish cascade: data.json written ~30–40×/second

**Files:** `RPI/V2P/V2P.py`, `RPI/V2N/Car_client.py`, `RPI/DashBoard/server.py`, `RPI/hub/dashboard_bridge.py`.

The data flow multiplies itself:

1. `server.py` publishes `vehicle_speed` on **every UART packet** (10 Hz), even when the speed hasn't changed.
2. `Car_client._on_vehicle_speed` republishes a full `v2n_frame` for **each** of those (10 Hz), even when nothing in the frame changed.
3. `V2P.py` publishes `v2p_frame` on **every camera frame** (up to 30 Hz), flags changed or not.
4. `dashboard_bridge.py` does a full **read → parse → serialize → write-to-SD** of `data.json` for *every one* of those frames (~40 writes/second).
5. Every write bumps the file mtime → every SSE client re-reads and re-sends the whole snapshot.

This is exactly the SD-card wear + latency problem that `server.py`'s own design comment says it was built to avoid. It also spams V2P's console (`[V2P] traffic update ← …` at 10 Hz).

**Fix (publish only on change — three small patches):**

`server.py` — only publish a *changed* speed:
```python
# old:
_ipc_node.publish("vehicle_speed", {"speed_kmh": state["drive"]["speedKmh"]})

# new:
speed = round(state["drive"]["speedKmh"], 1)
if speed != _last_pub_speed:          # module-level: _last_pub_speed = None
    _last_pub_speed = speed
    _ipc_node.publish("vehicle_speed", {"speed_kmh": speed})
```

`V2P.py` — publish the frame only when a flag actually changed (same pattern already used for `motorcycle_alert`):
```python
# old:
_publish_v2p_frame(frame_ped_flag, frame_pos_flag, frame_lead_car_flag)

# new:
_v2p_now = (frame_ped_flag, frame_pos_flag, frame_lead_car_flag)
if _v2p_now != _prev_v2p_frame:       # module-level: _prev_v2p_frame = None
    _prev_v2p_frame = _v2p_now
    _publish_v2p_frame(*_v2p_now)
```

`dashboard_bridge.py` — skip the disk write when the values are already in the file:
```python
def _update_data(mutate) -> None:
    with _file_lock:
        data = _load_data()
        before = json.dumps(data, sort_keys=True)
        mutate(data)
        if json.dumps(data, sort_keys=True) != before:   # only write real changes
            _save_data(data)
```

**Why:** the flags change a few times per minute, not 40 times per second. Publishing deltas cuts hub traffic, SD writes, and SSE churn by ~99% with zero behavior change on the dashboard.

---

## Issue 17 — V2P `skip_frames = 1` makes the pipeline inference-bound

**File:** `RPI/V2P/V2P.py`.

```python
# ★ OPTIMIZED: process every frame instead of skipping (skip_frames = 1)
skip_frames  = 1
```

Running a 640×640 ONNX detector on **every** captured frame on a Pi CPU is not an optimization — inference takes longer than a frame period, so the loop becomes inference-bound: real FPS drops, latency rises, and the intent analyzer (which assumes evenly spaced samples) sees jerky motion. The tracking/drawing code already supports skipping (it reuses `last_objects`).

**Fix:**
```python
skip_frames = 3   # infer at ~10 Hz, draw at full FPS — tune with the on-screen FPS counter
```

**Why:** detection at 8–10 Hz is plenty for pedestrians at prototype speeds; the smooth 30 FPS overlay comes from reusing the last tracked objects, which the code already does. (Also revisit `CONF_THRESH = 0.25` — lowering it "for better detection" adds false positives *and* more NMS/tracker work; 0.30–0.35 is usually the better trade.)

---

## Issue 18 — `os.system('clear')` on every MQTT message

**File:** `RPI/V2N/Car_client.py`, `on_message()`.

```python
# old:
os.system('cls' if os.name == 'nt' else 'clear')
```

This forks a shell + spawns `/usr/bin/clear` for every incoming packet (~1/second, more during emergencies) — needless process churn on the Pi, and it breaks when stdout is a log file (as under `run_all.sh`, where it writes garbage escape bytes into `logs/car_client.log`).

**Fix:**
```python
# new: pure ANSI escape — no subprocess, harmless in a log file
print("\033[H\033[2J", end="")
```
Or better, since `run_all.sh` redirects output to a log anyway: drop the clearing entirely and print a single status line only when values change.

---

## Issue 19 — ESP32: heavy Serial printing inside RX paths

**Files:** `esp32/master/master.ino`, `esp32/slave/slave.ino`.

Every received packet (both UART and ESP-NOW directions) prints ~10 `Serial.printf` lines. `OnDataRecv` runs in the Wi-Fi task context — blocking it with prints delays ESP-NOW handling; and long prints in `loop()` are exactly why the UART RX buffer overflowed before (the comment in `setup()` admits the workaround: `setRxBufferSize(1024)` "while loop() is busy (e.g. ... Serial prints)").

**Old code (current):**
```cpp
Serial.println("====== [UART RX] ======");
Serial.printf("Vehicle ID : %d\n", n.vehicle_id);
... // 8 more lines per packet
```

**New code (fixed):** gate the verbose dump behind a debug flag; keep one short line in release:
```cpp
#define DEBUG_VERBOSE 0   // set 1 only when debugging with the serial monitor

#if DEBUG_VERBOSE
  Serial.println("====== [UART RX] ======");
  Serial.printf("Vehicle ID : %d\n", n.vehicle_id);
  ...
#else
  Serial.printf("[RX] id=%d spd=%.1f\n", n.vehicle_id, n.speed);
#endif
```

**Why:** at 115200 baud a 10-line dump takes ~30 ms of blocking time per packet — that's the real cause the enlarged RX buffer only papers over. Cutting the prints removes the root cause.

---

## Issue 20 — IPC nodes never reconnect if the hub restarts

**File:** `RPI/hub/ipc_node.py` (affects every RPI process).

If `hub.py` crashes or is restarted, `_recv_loop` prints "hub closed the connection" and exits — and the node is dead forever: subsequent `publish()` calls raise `BrokenPipeError`. In `V2P.py` that exception is **not caught in the main loop**, so a hub restart kills the whole V2P camera process; in `Car_client.py` it just spams an error per message.

**Fix (minimal, safe):** make `publish()` never throw, and attempt a lazy reconnect:
```python
def publish(self, topic: str, data: dict) -> None:
    try:
        self._send_raw({"cmd": "publish", "topic": topic, "data": data})
    except (BrokenPipeError, OSError, AttributeError):
        # hub gone — try one quick reconnect, else drop the frame silently
        if self.connect(retries=1):
            for t in self._callbacks:               # re-subscribe after reconnect
                self._send_raw({"cmd": "subscribe", "topic": t})
            self.start_listening()
            try:
                self._send_raw({"cmd": "publish", "topic": topic, "data": data})
            except OSError:
                pass
```

**Why:** the hub is a single point of failure by design; the clients should degrade gracefully (drop frames) instead of dying, exactly like the MQTT side already does with paho's auto-reconnect.

---

## Issue 21 — MQTT auth failure printed as "✅ Successfully connected"

**Files:** `Traffic_Light/Distance.py`, `Traffic_Light/dis&AI_camera.py`.

`mqtt_client.connect()` only opens the TCP/TLS connection — authentication happens asynchronously in CONNACK. Neither script registers `on_connect`, so with a wrong password they print **"✅ Successfully connected to MQTT Broker!"** and then silently publish into the void.

**Old code (current — misleading):**
```python
try:
    mqtt_client.connect(BROKER, PORT, 60)
    mqtt_client.loop_start()
    print("✅ Successfully connected to MQTT Broker!")   # ← auth not verified yet
except Exception as e:
    print(f"❌ MQTT Connection Failed: {e}")
```

**New code (fixed):**
```python
def _on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ MQTT connected & authenticated.")
    else:
        print(f"❌ MQTT auth/connect failed: {reason_code}")

mqtt_client.on_connect = _on_connect
try:
    mqtt_client.connect(BROKER, PORT, 60)
    mqtt_client.loop_start()
    print("🌐 MQTT connecting … (result reported by callback)")
except Exception as e:
    print(f"❌ MQTT Connection Failed: {e}")
```

**Why:** with the credentials currently scrubbed (Issue 25) every script would claim success while nothing flows — this exact confusion will cost debugging time on demo day.

---

## Issue 22 — `RPI/V2P/model2.onnx` in git is 0 bytes

**File:** `RPI/V2P/model2.onnx` (size = 0).

The ONNX model committed to the repo is an **empty file**. On any fresh clone (or after an accidental checkout over the real file on the Pi), `V2P.py` crashes at startup:
`onnxruntime ... InvalidProtobuf: model2.onnx failed to load`.

**Fix:**
1. Keep big binaries out of git: add `*.onnx` (and `*.pt`) to `.gitignore`, remove the empty blob from the repo.
2. Document in the README where to get the model (or use git-lfs if it must be versioned).
3. Add a guard so the failure is self-explaining:
```python
if not os.path.exists(MODEL_PATH) or os.path.getsize(MODEL_PATH) == 0:
    sys.exit(f"[V2P] FATAL: {MODEL_PATH} is missing or empty — copy the real model first.")
```

**Why:** the real model lives only on the Pi right now; one `git checkout`/`git pull --force` away from deleting it. (Same consideration for `Traffic_Light/yolov8n.pt`, which *is* committed — 6.5 MB binary in history.)

---

# 🔵 LOW ISSUES (quality / maintainability)

## Issue 23 — `master.ino` and `slave.ino` are 99% identical

**Files:** `esp32/master/master.ino` (236 lines), `esp32/slave/slave.ino` (234 lines).

The two sketches differ **only** in `#define VEHICLE_ID 1` vs `2` (and one brace style). Every bug fix must be applied twice — the resync logic, the RX-buffer fix, and the struct comment were all duplicated by hand already.

**Fix:** one sketch, one line to change per board:
```cpp
// esp32/v2v_node/v2v_node.ino  (single sketch replaces master/ and slave/)
#ifndef VEHICLE_ID
#define VEHICLE_ID 1   // ← set 1 or 2 before flashing each board
#endif
```
With arduino-cli you can even keep one source and select the ID at build time:
```bash
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DVEHICLE_ID=2" ...
```
The shared `Neighbor` struct should also live in one header (`esp32/common/neighbor.h`) included by the node sketch and the sniffer, since it "MUST match the STM32 struct byte-for-byte" — three hand-synced copies of a wire-format struct is how mismatches happen.

---

## Issue 24 — `Distance.py` and `dis&AI_camera.py` duplicate the pipeline with drifted parameters

**Files:** `Traffic_Light/Distance.py`, `Traffic_Light/dis&AI_camera.py`.

Both implement: MQTT setup, distance-from-plate-width math, OCR preprocessing (gray → equalizeHist → Otsu), plate cleaning, ambulance matching, publish. They have already drifted:

| Parameter | Distance.py | dis&AI_camera.py |
|-----------|-------------|------------------|
| Min plate length | `len >= 4` | `len >= 3` |
| OCR confidence filter | none (fake formula) | `prob > 0.35` |
| Plate width from bbox | `max(x) - min(x)` ✅ robust | `bbox[1][0] - bbox[0][0]` ⚠ breaks on rotated text |
| Vehicle classes | `{2}` (car only) | `{2, 5, 7}` (car/bus/truck) |
| Timestamp in payload | yes | no |

**Fix:** extract a shared module:
```python
# Traffic_Light/plate_common.py  (NEW FILE)
def preprocess_for_ocr(roi): ...
def clean_plate_text(text): ...            # one regex, one min-length rule
def plate_width_px(bbox):                  # the robust version
    xs = [p[0] for p in bbox]
    return max(xs) - min(xs)
def calculate_distance(width_px, focal=700.0, known_w=0.45): ...
def make_payload(plate, distance, is_ambulance): ...
```
Both scripts then shrink to: capture loop + detection + calls into `plate_common` + their own display logic.

**Why:** the parameter drift above is not hypothetical — it means the recorded-video pipeline and the live pipeline accept *different plates at different distances*, which makes test results non-reproducible on the live system.

---

## Issue 25 — Hardcoded MQTT credentials in five files (currently scrubbed placeholders)

**Files:** `Distance.py`, `dis&AI_camera.py`, `Traffic_light_GUI.py`, `Car_client.py` (and `Intelligent_Gateway.py`, which lost its block entirely — Issue 1).

Current state:
```python
PASSWORD = "***REMOVED***"     # ← placeholder: NOTHING can authenticate right now
```
Two problems: (a) real credentials were committed to git history at some point (they should be rotated on HiveMQ), and (b) the scrub replaced them with a literal string, so **every MQTT client in the project will fail auth** until fixed — combined with Issue 21 they'll fail *silently*.

**Fix:** one shared config that reads the environment, imported everywhere:
```python
# Traffic_Light/v2x_config.py  +  RPI/hub/v2x_config.py (or one file on PYTHONPATH)
import os
BROKER   = os.environ.get("V2X_MQTT_BROKER",
                          "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud")
PORT     = int(os.environ.get("V2X_MQTT_PORT", "8883"))
USERNAME = os.environ.get("V2X_MQTT_USER", "v2n_admin")
PASSWORD = os.environ.get("V2X_MQTT_PASSWORD", "")
if not PASSWORD:
    raise SystemExit("Set V2X_MQTT_PASSWORD in the environment (see README).")
AMBULANCE_ID = "T4RR"
```
Add to `.gitignore` a `.env`/`secrets.sh` file and source it in `run_all.sh`. Rotate the old HiveMQ password since it exists in git history.

---

## Issue 26 — `Storing_algorithm.cpp` test overwrites its own test data

**File:** `esp32/Storing_algorithm.cpp`, `main()`.

**Old code (current — broken test):**
```cpp
Neighbor msg1 = {1, 10.0f, 20.0f, 0.0f, 60.0f, 90.0f, 1000};
msg1 = {5, 100.0f, 200.0f, 0.0f, 80.0f, 270.0f, 3000};    // ← msg1 is now id 5!
Neighbor msg2 = {2, 15.0f, 25.0f, 0.0f, 50.0f, 180.0f, 1500};
Neighbor msg3 = {1, 12.0f, 22.0f, 0.0f, 65.0f, 95.0f, 2000};
msg3 = {6, 120.0f, 220.0f, 0.0f, 90.0f, 360.0f, 3500};    // ← msg3 is now id 6!

update_neighbor(msg1);
update_neighbor(msg1); // "Duplicate ID, should update existing entry"  ← WRONG:
                       //  it inserts id 5 twice; the duplicate-update path for
                       //  DIFFERENT data (msg3 with id 1) is never exercised.
```

**New code (fixed):**
```cpp
Neighbor msg1 = {1, 10.0f, 20.0f, 0.0f, 60.0f,  90.0f, 1000};
Neighbor msg2 = {2, 15.0f, 25.0f, 0.0f, 50.0f, 180.0f, 1500};
Neighbor msg3 = {1, 12.0f, 22.0f, 0.0f, 65.0f,  95.0f, 2000};  // same id, new data

update_neighbor(msg1);
update_neighbor(msg2);
update_neighbor(msg3);   // must UPDATE entry id=1, not insert — verify table size == 2

remove_stale_neighbors(4000);   // 4000-2000 <= 2000 keeps id 1; 4000-1500 > 2000 drops id 2
```
Also add the missing assertion (`neighbor_table.size() == 2` after the updates) so the test actually fails when broken.

---

## Issue 27 — Control server: reverse is never blocked, and the endpoint has no auth

**File:** `RPI/Control/control_server.py`.

1. `blocked_reason()` guards `F`, `L`, `R` — but **`B` (reverse) has no guard at all**, even though the car has three rear ultrasonic sensors and the dashboard computes rear proximity. A pedestrian behind the car doesn't block reverse.

**Fix (uses data the `/adas` endpoint can already carry):**
```python
if direction == "B":
    if _adas.get("rearObstacle", 0) == SYS_CRITICAL:   # expose it from server.py's US state
        return "REAR-OBSTACLE"
```
(`server.py` would add e.g. `"rearObstacle": 2 if ultrasonic["rear"] < 10 else 0` into `send_adas()`.)

2. Anyone on the same Wi-Fi can open `http://<pi>:8001` and drive the car. Acceptable for a supervised demo; if the demo Wi-Fi is shared (university network!), add at least a trivial token:
```python
TOKEN = os.environ.get("V2X_CTRL_TOKEN", "")
# in do_POST: if TOKEN and self.headers.get("X-Auth") != TOKEN: return self._send_json({...}, 403)
```

---

## Issue 28 — `run_all.sh`: comment says "wait for any child", code waits for all

**File:** `RPI/run_all.sh`, last line.

```bash
# Wait for any child to exit; keep the script (and log tails) alive until then.
wait
```
Plain `wait` blocks until **all** background jobs exit — and the `tail -F` pipelines never exit, so the script never notices a crashed component after startup (only the immediate-crash check inside `start()` runs). Behavior is "stay alive forever", which is fine for Ctrl+C usage, but the comment misleads and crashes go unnoticed.

**Fix (bash ≥ 4.3):** watch the *component* PIDs specifically:
```bash
# Wait for any COMPONENT (not the log tails) to exit, then report it.
while true; do
  wait -n "${PIDS[@]}" 2>/dev/null || true
  for i in "${!PIDS[@]}"; do
    if ! kill -0 "${PIDS[$i]}" 2>/dev/null; then
      echo "✗ ${NAMES[$i]} exited — see $LOG_DIR/${NAMES[$i]}.log"
      unset 'PIDS[i]' 'NAMES[i]'
    fi
  done
  [[ ${#PIDS[@]} -eq 0 ]] && break
done
```

---

## Issue 29 — Dashboard alarms are silent until the page is tapped once (autoplay policy)

**File:** `RPI/DashBoard/js/app.js` (`getAudioCtx()` and every `play*` function).

Chrome/Firefox block audio that was not initiated by a user gesture. `getAudioCtx()` creates the `AudioContext` lazily on the **first beep**, which is always triggered by an SSE update — never by a click — so the context is born in the `suspended` state and stays there. Result: the critical alarm loop, the toast beeps, the ultrasonic proximity beeps, and the traffic-light STOP beep are all **completely silent** until someone happens to click/tap the page (and even then, nothing calls `resume()`, so on Chrome they stay silent forever).

On a safety dashboard this is the worst kind of failure: the red popup appears, but the alarm that is supposed to grab the driver's attention never sounds — and it works fine on the developer's machine (where you've clicked the page a hundred times), then fails on demo day on a freshly opened kiosk screen.

**Old code (current — broken):**
```javascript
let audioCtx = null;
function getAudioCtx() {
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  return audioCtx;   // created outside a user gesture → state === "suspended" → silent
}
```

**New code (fixed):** resume the context on the first user gesture, and surface a one-time "tap to enable sound" hint until that happens:
```javascript
let audioCtx = null;
function getAudioCtx() {
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  return audioCtx;
}

// Autoplay policy: an AudioContext created outside a user gesture starts
// "suspended" and never produces sound. Unlock it on the first interaction.
function unlockAudio() {
  const ctx = getAudioCtx();
  if (ctx.state === "suspended") ctx.resume();
  document.removeEventListener("pointerdown", unlockAudio);
  document.removeEventListener("keydown", unlockAudio);
}
document.addEventListener("pointerdown", unlockAudio);
document.addEventListener("keydown", unlockAudio);
```
Optionally show a small banner while `ctx.state === "suspended"` ("🔇 tap anywhere to enable alarms") so the operator knows sound is not armed yet.

**Why:** the demo procedure must not depend on someone remembering to click the page. One gesture listener makes the first accidental tap arm all alarms permanently.

---

## Issue 30 — Two processes write `data.json` via the SAME `.tmp` file, with no cross-process lock

**Files:** `RPI/DashBoard/server.py` (`_apply_vehicle_id()`), `RPI/hub/dashboard_bridge.py` (`_save_data()` / `_update_data()`).

The bridge is documented as "the ONLY writer to data.json" — but `server.py` also writes it once at startup (to set `meta.vehicleId`). Both writers:

1. use the **identical** temp path `data.json.tmp` (`DATA_FILE + ".tmp"`), and
2. do read → modify → `os.replace` with only a **thread**-level lock (`_file_lock` protects threads inside the bridge process, not against the other process).

Two failure modes when `server.py` starts while the bridge is writing (with the bridge currently writing 30–40×/s — Issue 16 — a collision at startup is not exotic):

- **Corrupted/failed replace:** both processes open the same `data.json.tmp` in `"w"` at once → interleaved bytes; then two `os.replace` calls race — the second one can raise `FileNotFoundError` (tmp already moved) or move a half-written file into place.
- **Lost update:** server reads the file (old v2p flags) → bridge writes fresh flags → server replaces the file with old-flags + vehicleId. Today the next frame (~30 ms later) repairs it; **after the Issue 16 fix (publish-on-change) the stale flag persists until that flag next changes** — i.e. fixing Issue 16 makes this race stickier, so fix them together.

**New code (fixed):** give each process a unique tmp name, and route the one-shot vehicleId write through the same guarded pattern:
```python
# in BOTH writers — unique tmp per process, atomic replace stays atomic:
tmp = f"{DATA_FILE}.{os.getpid()}.tmp"
with open(tmp, "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
os.replace(tmp, DATA_FILE)
```
plus an inter-process lock around the read-modify-replace so updates can't be lost:
```python
import fcntl

LOCK_FILE = DATA_FILE + ".lock"

def _locked_update(mutate) -> None:
    with open(LOCK_FILE, "w") as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)      # blocks the other process, not just threads
        data = _load_data()
        mutate(data)
        _save_data(data)                     # unique-tmp version above
```
(Cleaner long-term: `server.py` shouldn't touch the file at all — publish `{"vehicleId": …}` on a hub topic and let the bridge, the single writer, persist it.)

**Why:** "atomic replace" only protects readers from partial files; it does nothing about two writers racing each other through the same tmp path. One `flock` + unique tmp names closes both holes with ~6 lines.

---

# Additional observations (no action required, worth knowing)

- **`Intelligent_Gateway.py` `car_counter = (car_counter + 1) % 10`** — "density" silently wraps 9 → 0 and never decreases when cars leave. If density matters for the dashboard, derive it from `len(vehicle_registry)` instead of an ever-incrementing counter.
- **`Car_client.py` crossing check** uses `closest_vehicle.distance_m` (camera→nearest car) as the host car's distance to the light — a documented approximation; fine for the prototype, but label it in the demo script.
- **`V2P.py` AMBER path** (`CAR_TRAFFIC_LIGHT in ("AMBER", "YELLOW")`) is reachable only via the keyboard simulator — the hub feed maps flags to `"RED"/"GREEN"` only. Not a bug, just dead-in-production code to be aware of.
- **`hub.py`**: registering the same client name twice overwrites the previous socket (old client keeps its thread but stops receiving); a malformed JSON line drops the whole connection instead of skipping the line. Both acceptable for a closed system, easy to harden later.
- **`Traffic_light_GUI.py` pause/resume** restarts from `sim_index = 0` (always RED) rather than resuming the paused phase — decide if that's intended UX.
- **Dashboard `render()`** trusts `d.meta` / `d.drive` to exist; a hand-edited `data.json` missing a section throws in the SSE handler (caught, but the frame is skipped). A `d = {meta:{}, drive:{}, ...d}` merge would make TEST MODE hand-editing safer.
- **paho `reason_code == 0`** works but the v2 API prefers `reason_code.is_failure` checks — more explicit with MQTT v5 reason codes.
- **`Car_client.py` console block** (`on_message`, after `_publish_v2n_frame()`) reads `_distance_m`/`_speed_kmh`/… **without** `_state_lock` — worst case a display line mixes values from two packets. Harmless (display only), but grab the lock once and copy, like `_publish_v2n_frame` already does.
- **`control_server.py` `Motors._kick()`** sleeps `KICK_T` (120 ms) while holding `self._lock`, so a concurrent STOP command waits up to 120 ms before the motors actually stop. Same order of magnitude as the watchdog (`IDLE_T` 300 ms), fine for the prototype — just don't lengthen `KICK_T` without remembering this.
- **Dashboard `updateRisk()`** ignores `trafficLight == STOP`: a red light plays the warning sound and turns the T-LIGHT card red, but the risk gauge can still read "SECURE". Decide whether a red light should count as at least Medium Risk for consistency.

---

# Recommended fix order (action plan)

1. **Make it run at all:** Issue 1 (NameError), Issue 25 (credentials/env), Issue 22 (empty model guard).
2. **Make it safe/correct:** Issue 2 (deadlock), Issue 3 (motorcycle dead code), Issues 4+5 (ambulance latch), Issue 6 (false trigger), Issue 12+14 (socket corruption), Issue 8 (Tk thread), Issue 29 (silent alarms).
3. **Make it honest:** Issue 21 (fake "connected"), Issue 7 (wrong distance in warning), Issue 11 (fake confidence), Issue 9 (timing drift).
4. **Make it fast/durable:** Issue 16 (publish-on-change — biggest single win) **together with Issue 30** (data.json write race — the Issue 16 fix makes it stickier), Issue 17 (skip_frames), Issue 15 (leak), Issue 10 (disk fill), Issues 18–19 (console churn), Issue 20 (hub reconnect).
5. **Make it maintainable:** Issues 23, 24, 26, 27, 28.

*Report generated by an automated full-codebase review; every issue above was verified against the actual source (line numbers refer to the current `main` branch).*
