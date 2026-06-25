# -*- coding: utf-8 -*-
"""
trafic_light.py — V2X Smart Traffic Light Simulator
=====================================================

Role in the system
------------------
This process simulates a physical traffic light connected to the V2X network.
It runs on a separate machine (different lab) from the Gateway and Car OBU.

What it does
------------
1. Runs a normal RED → GREEN → YELLOW → RED cycle automatically.
2. Listens to the Gateway's processed topic for emergency override commands.
   When an ambulance is detected, it overrides the normal cycle to give the
   ambulance a clear GREEN path.
3. Every second, publishes the current light state to the MQTT broker so the
   Intelligent Gateway can forward it to all connected Car OBUs.

Transition Flag Encoding (computed HERE, forwarded by Gateway to Car OBU)
--------------------------------------------------------------------------
The transition_flag tells the Car OBU what change is ABOUT TO HAPPEN next,
so it can compute the crossing_flag correctly even near phase boundaries.

    Flag  |  Current → Next
    ------|-----------------
      0   |  GREEN  → YELLOW   (green is ending soon)
      1   |  YELLOW → RED      (yellow is ending soon)
      2   |  RED    → YELLOW   (red is ending soon)
      3   |  YELLOW → GREEN    (yellow is ending soon, green coming)
     -1   |  mid-phase, no imminent transition

Why compute it HERE and not in the Gateway or Car?
---------------------------------------------------
The traffic light is the ONLY component that knows its own state machine and
what comes next. The Gateway and Car are downstream consumers — they should
not need to understand the light's internal cycle. Keeping the flag here
ensures a single source of truth.

MQTT Topics
-----------
Subscribes : V2X/zone1/traffic/processed   (from Intelligent Gateway)
Publishes  : v2n/traffic/light/state       (consumed by Intelligent Gateway)

Published payload fields
------------------------
    state           : str   — "RED" | "GREEN" | "YELLOW"
    next_state      : str   — the state that will follow the current one
    transition_code : int   — see Transition Flag Encoding above
    remaining_time  : int   — seconds until state changes
    is_emergency    : bool  — True when ambulance override is active

Normal cycle timings
--------------------
    GREEN  : 15 seconds
    YELLOW : 3  seconds
    RED    : 10 seconds

Emergency override behaviour
-----------------------------
    If current state is RED  → immediately switch to YELLOW (3 s) then GREEN
    If current state is YELLOW → hold YELLOW for 3 s then switch to GREEN
    If current state is GREEN  → extend GREEN for 15 s
    Once the emergency flag clears, the normal cycle resumes from wherever
    the light currently is.

Startup order (this machine)
----------------------------
    python3 trafic_light.py   ← this file only; Gateway is on another machine
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading

# ============================================================
# MQTT Configuration (HiveMQ Cloud — shared broker)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# Topic the Gateway publishes processed packets to (we read emergency flag)
OVERRIDE_TOPIC = "V2X/zone1/traffic/processed"

# Topic this simulator publishes light state to (Gateway reads this)
STATE_TOPIC    = "v2n/traffic/light/state"

# ============================================================
# Normal cycle: defines state → (duration_seconds, next_state)
# ============================================================
CYCLE = {
    "GREEN":  {"duration": 15, "next": "YELLOW"},
    "YELLOW": {"duration":  3, "next": "RED"},
    "RED":    {"duration": 10, "next": "GREEN"},
}

# ============================================================
# Transition flag lookup: (current_state, next_state) → flag
#   0  GREEN  → YELLOW
#   1  YELLOW → RED
#   2  RED    → YELLOW   (RED transitions through YELLOW before GREEN)
#   3  YELLOW → GREEN
#  -1  mid-phase (no imminent transition defined for this pair)
# ============================================================
TRANSITION_FLAG = {
    ("GREEN",  "YELLOW"): 0,
    ("YELLOW", "RED"):    1,
    ("RED",    "YELLOW"): 2,
    ("YELLOW", "GREEN"):  3,
}

# ============================================================
# Shared state (written by MQTT thread, read by cycle thread)
# ============================================================
current_state  = "RED"
is_override    = False
remaining_time = 10
state_lock     = threading.Lock()


# ============================================================
# Helper: compute next_state and transition_code
# ============================================================
def _next_state_for(state: str, override: bool) -> str:
    """Return the state that follows *state* under normal or override logic."""
    if override:
        # Under emergency, after YELLOW comes GREEN; GREEN stays GREEN
        if state == "YELLOW":
            return "GREEN"
        if state == "GREEN":
            return "GREEN"   # stays green while override active
        if state == "RED":
            return "YELLOW"  # RED → YELLOW first
    return CYCLE[state]["next"]


def _transition_code(current: str, nxt: str) -> int:
    """Return the integer flag for (current → next) pair, or -1 if unknown."""
    return TRANSITION_FLAG.get((current, nxt), -1)


# ============================================================
# MQTT callbacks
# ============================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Traffic Light Simulator connected to HiveMQ Cloud.")
        client.subscribe(OVERRIDE_TOPIC)
        print(f"   Listening for emergency overrides on: {OVERRIDE_TOPIC}")
    else:
        print(f"❌ MQTT connection failed: {reason_code}")


def on_message(client, userdata, msg):
    """
    Handle a processed packet from the Intelligent Gateway.

    Reads
    -----
    is_emergency : bool   — True when an ambulance has been detected
    warning      : str    — contains "AMBULANCE" when emergency is active

    Side-effects
    ------------
    Sets/clears is_override and adjusts remaining_time / current_state so
    the ambulance gets a clear GREEN path as quickly as possible.
    """
    global current_state, is_override, remaining_time
    try:
        data          = json.loads(msg.payload.decode())
        msg_emergency = data.get("is_emergency", False)
        warning_msg   = data.get("warning", "")

        with state_lock:
            if msg_emergency or "AMBULANCE" in warning_msg.upper():
                if not is_override:          # first detection → act immediately
                    is_override = True
                    if current_state == "RED":
                        # RED → YELLOW (3 s) → GREEN so pedestrians get warning
                        current_state  = "YELLOW"
                        remaining_time = 3
                    elif current_state == "YELLOW":
                        remaining_time = 3   # confirm the yellow window
                    else:
                        # Already GREEN → extend for ambulance passage
                        current_state  = "GREEN"
                        remaining_time = 15
            else:
                is_override = False          # emergency cleared; resume normal cycle

    except Exception as exc:
        print(f"❌ Error processing override command: {exc}")


# ============================================================
# Traffic cycle logic (runs in its own thread, once per second)
# ============================================================
def traffic_cycle_logic(client: mqtt.Client) -> None:
    """
    Advance the traffic light state machine every second.

    Decision tree each tick
    -----------------------
    Override active:
        YELLOW: count down; when 0 → switch to GREEN (for ambulance)
        GREEN : count down; when 0 → extend GREEN (keep ambulance path clear)
        RED   : immediately switch to YELLOW
    Normal cycle:
        Count down remaining_time; when 0 → advance to next state

    After updating state, compute next_state and transition_code, then
    publish a JSON status message to STATE_TOPIC for the Gateway.
    """
    global current_state, is_override, remaining_time

    while True:
        with state_lock:
            # ── Advance state machine ────────────────────────────────
            if is_override:
                if current_state == "YELLOW":
                    if remaining_time <= 1:
                        current_state  = "GREEN"
                        remaining_time = 15
                    else:
                        remaining_time -= 1
                elif current_state == "GREEN":
                    if remaining_time > 1:
                        remaining_time -= 1
                    else:
                        remaining_time = 5   # keep extending while override holds
                elif current_state == "RED":
                    current_state  = "YELLOW"
                    remaining_time = 3
            else:
                # Normal cycle
                if remaining_time <= 1:
                    current_state  = CYCLE[current_state]["next"]
                    remaining_time = CYCLE[current_state]["duration"]
                else:
                    remaining_time -= 1

            # ── Compute next_state and transition_code ───────────────
            nxt  = _next_state_for(current_state, is_override)
            code = _transition_code(current_state, nxt)

            # Take snapshots so we can release the lock before I/O
            snap_state = current_state
            snap_time  = remaining_time
            snap_emg   = is_override
            snap_next  = nxt
            snap_code  = code

        # ── Publish to MQTT (outside lock) ───────────────────────────
        status_msg = {
            "state":           snap_state,     # current light colour
            "next_state":      snap_next,      # colour that follows
            "transition_code": snap_code,      # flag (0-3 or -1)
            "remaining_time":  snap_time,      # seconds until change
            "is_emergency":    snap_emg,       # True = ambulance override
        }
        try:
            client.publish(STATE_TOPIC, json.dumps(status_msg))
        except Exception:
            pass

        # ── Console display ──────────────────────────────────────────
        icon = {"RED": "🔴", "GREEN": "🟢", "YELLOW": "🟡"}.get(snap_state, "⚪")
        tf_labels = {0: "G→Y", 1: "Y→R", 2: "R→Y", 3: "Y→G", -1: "--"}
        print(
            f"{icon} {snap_state:6s} | next={snap_next:6s} "
            f"| tf={tf_labels.get(snap_code,'?'):3s} "
            f"| t={snap_time:2d}s "
            f"| emg={'YES' if snap_emg else 'No'}"
        )
        if snap_emg:
            print("  🚨 EMERGENCY OVERRIDE ACTIVE — GREEN PATH FOR AMBULANCE")

        time.sleep(1)


# ============================================================
# Main
# ============================================================
if __name__ == "__main__":
    print("=" * 60)
    print("  V2X Traffic Light Simulator starting …")
    print("=" * 60)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    client.on_connect = on_connect
    client.on_message = on_message

    print("🌐 Connecting to HiveMQ Cloud …")
    client.connect(BROKER, PORT, 60)

    # Start the cycle logic in a background daemon thread
    threading.Thread(
        target=traffic_cycle_logic,
        args=(client,),
        daemon=True,
        name="TrafficCycleThread",
    ).start()

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Traffic Light Simulator stopped.")
