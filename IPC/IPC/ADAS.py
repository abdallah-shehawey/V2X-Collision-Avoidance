"""
main4.py - ADAS Decision Engine
=================================
The Advanced Driver Assistance System (ADAS) is the "brain" of the V2X
stack. It subscribes to every relevant topic, fuses the incoming data,
and emits a driving recommendation every second.

In the real system, ADAS output would be forwarded back to the STM32
over UART to actuate braking, steering corrections, or warnings.
For this test it prints the recommended action to the console.

Fusion logic
------------
ADAS maintains a local copy of the most recent frame from each topic.
On every new data event it re-evaluates all conditions and produces a
single unified recommendation:

    STOP         Red light or pedestrian crossing
    SLOW DOWN    Yellow light or front obstacle < 150 cm
    PROCEED      Green light, no obstacles, no pedestrians
    EMERGENCY    Any ambulance or hazard detected

Topics subscribed : "traffic_light", "vehicle_data", "pedestrian_status"
Topic published   : "adas_command"
Data schema (published):
    {
        "action"        : "PROCEED" | "SLOW_DOWN" | "STOP" | "EMERGENCY",
        "reason"        : str,                human-readable explanation
        "speed_limit"   : float | None,       suggested max speed in km/h
        "confidence"    : float               0.0 – 1.0
    }
"""

import time
from ipc_node import IPCNode

# Shared state — updated by listener callbacks, read by the decision loop
state = {
    "traffic_light":    {"state": "RED",   "is_emergency": False},
    "vehicle_data":     {"speed_kmh": 0,   "brake_pressed": False,
                         "ultrasonic": {"front_cm": 999}},
    "pedestrian_status":{"crossing_safe": False, "alert_level": "none"},
}

node: IPCNode = None


# ──────────────────────────────────────────────────────────────────────────────
# Subscriber callbacks — update shared state
# ──────────────────────────────────────────────────────────────────────────────

def on_traffic_light(topic: str, data: dict, sender: str) -> None:
    state["traffic_light"] = data
    tl = data.get("state", "?")
    print(f"\n[ADAS] traffic_light update  : state={tl}")
    decide()


def on_vehicle_data(topic: str, data: dict, sender: str) -> None:
    state["vehicle_data"] = data
    spd   = data.get("speed_kmh", 0)
    front = data.get("ultrasonic", {}).get("front_cm", 999)
    print(f"[ADAS] vehicle_data update   : speed={spd}km/h | front={front}cm")
    decide()


def on_pedestrian_status(topic: str, data: dict, sender: str) -> None:
    state["pedestrian_status"] = data
    safe  = data.get("crossing_safe", False)
    alert = data.get("alert_level", "none")
    print(f"[ADAS] pedestrian update     : safe={safe} | alert={alert}")
    decide()


# ──────────────────────────────────────────────────────────────────────────────
# Decision engine
# ──────────────────────────────────────────────────────────────────────────────

def decide() -> None:
    """
    Fuse all available sensor/topic data and emit a driving command.

    Priority order (highest first):
        1. Emergency vehicle detected
        2. Pedestrian in crossing (crossing_safe = False with alert)
        3. Red or Yellow traffic light
        4. Front obstacle closer than 150 cm
        5. All clear -> proceed
    """
    tl      = state["traffic_light"]
    vd      = state["vehicle_data"]
    ped     = state["pedestrian_status"]

    tl_state    = tl.get("state", "RED")
    is_emerg    = tl.get("is_emergency", False)
    front_cm    = vd.get("ultrasonic", {}).get("front_cm", 999)
    ped_alert   = ped.get("alert_level", "none")
    crossing    = ped.get("crossing_safe", False)

    # ── Rule 1: Emergency ─────────────────────────────────────────────
    if is_emerg:
        _publish_command("EMERGENCY", "Ambulance detected — clear the road",
                         speed_limit=0, confidence=1.0)
        return

    # ── Rule 2: Pedestrians in crossing ───────────────────────────────
    if ped_alert in ("caution", "danger") and not crossing:
        _publish_command("STOP", "Pedestrians in crossing — do not enter",
                         speed_limit=0, confidence=0.95)
        return

    # ── Rule 3: Traffic light ─────────────────────────────────────────
    if tl_state == "RED":
        _publish_command("STOP", "Red light — wait for green",
                         speed_limit=0, confidence=1.0)
        return

    if tl_state == "YELLOW":
        _publish_command("SLOW_DOWN", "Yellow light — prepare to stop",
                         speed_limit=20, confidence=0.9)
        return

    # ── Rule 4: Close obstacle ────────────────────────────────────────
    if front_cm < 150:
        _publish_command("SLOW_DOWN",
                         f"Obstacle {front_cm}cm ahead — reduce speed",
                         speed_limit=15, confidence=0.85)
        return

    # ── Rule 5: All clear ─────────────────────────────────────────────
    _publish_command("PROCEED", "All clear — normal driving",
                     speed_limit=50, confidence=0.8)


def _publish_command(
    action: str,
    reason: str,
    speed_limit: float | None,
    confidence: float,
) -> None:
    """Publish a driving command and print it to the console."""
    command = {
        "action":      action,
        "reason":      reason,
        "speed_limit": speed_limit,
        "confidence":  confidence,
    }
    node.publish("adas_command", command)

    icon = {"PROCEED": "🟢", "SLOW_DOWN": "🟡", "STOP": "🔴",
            "EMERGENCY": "🚨"}.get(action, "⬜")
    print(
        f"[ADAS] command -> adas_command | "
        f"{icon} {action:<10} | {reason}"
    )


# ──────────────────────────────────────────────────────────────────────────────
def main() -> None:
    global node
    node = IPCNode("adas_engine")

    if not node.connect():
        return

    node.subscribe("traffic_light",    on_traffic_light)
    node.subscribe("vehicle_data",     on_vehicle_data)
    node.subscribe("pedestrian_status", on_pedestrian_status)
    node.start_listening()

    print("[ADAS] Decision engine ready")
    print("[ADAS] Subscribed to : 'traffic_light', 'vehicle_data', 'pedestrian_status'")
    print("[ADAS] Publishing to : 'adas_command'\n")

    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
