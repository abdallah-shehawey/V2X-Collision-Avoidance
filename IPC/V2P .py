"""
main2.py - V2P Camera Process (Pedestrian Safety)
===================================================
Simulates the Vehicle-to-Pedestrian (V2P) subsystem running on the
Raspberry Pi. In the real system, this process drives a YOLO-based
camera pipeline that detects pedestrians and decides whether to issue
a crossing warning based on the current traffic light state.

Behaviour
---------
1. Subscribes to "traffic_light" to know when it is safe or dangerous
   for pedestrians to cross.
2. When the light turns RED  -> activates the camera (pedestrians may cross).
3. When the light turns GREEN -> deactivates the camera (vehicles moving).
4. When the light turns YELLOW -> raises an alert (transition warning).
5. Publishes a "pedestrian_status" message after each state change so
   the ADAS process (main4) can factor pedestrian presence into its
   decision making.

Topics subscribed : "traffic_light"
Topic published   : "pedestrian_status"
Data schema (published):
    {
        "camera_active"    : bool,
        "crossing_safe"    : bool,
        "alert_level"      : "none" | "caution" | "danger",
        "triggered_by"     : "RED" | "GREEN" | "YELLOW"
    }
"""

import time
from ipc_node import IPCNode

node: IPCNode = None


def on_traffic_light(topic: str, data: dict, sender: str) -> None:
    """
    React to a traffic light state change.

    Activates or deactivates the pedestrian camera and publishes a
    status update so downstream processes stay informed.

    Parameters
    ----------
    topic  : str   Always "traffic_light".
    data   : dict  Traffic light payload (state, remaining_sec, ...).
    sender : str   Name of the publishing process.
    """
    state     = data.get("state", "UNKNOWN")
    remaining = data.get("remaining_sec", 0)

    print(f"\n[V2P] received traffic_light | state={state} | {remaining}s remaining")

    if state == "RED":
        print("[V2P] ✅ Camera ACTIVATED — pedestrians may cross safely")
        _publish_status(camera_active=True,  crossing_safe=True,  alert="none",    triggered=state)

    elif state == "GREEN":
        print("[V2P] 🚗 Camera DEACTIVATED — vehicles are moving, do not cross")
        _publish_status(camera_active=False, crossing_safe=False, alert="none",    triggered=state)

    elif state == "YELLOW":
        print("[V2P] ⚠️  CAUTION — light changing, pedestrians must wait")
        _publish_status(camera_active=True,  crossing_safe=False, alert="caution", triggered=state)


def _publish_status(
    camera_active: bool,
    crossing_safe: bool,
    alert: str,
    triggered: str,
) -> None:
    """Publish a pedestrian status update to the 'pedestrian_status' topic."""
    node.publish("pedestrian_status", {
        "camera_active": camera_active,
        "crossing_safe": crossing_safe,
        "alert_level":   alert,
        "triggered_by":  triggered,
    })
    print(f"[V2P] published -> pedestrian_status | safe={crossing_safe} | alert={alert}")


def main() -> None:
    global node
    node = IPCNode("v2p_camera")

    if not node.connect():
        return

    node.subscribe("traffic_light", on_traffic_light)
    node.start_listening()

    print("[V2P] Pedestrian camera process ready")
    print("[V2P] Subscribed to: 'traffic_light'\n")

    # Keep the process alive; all work is done in the listener thread.
    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
