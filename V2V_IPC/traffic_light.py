"""
main1.py - Traffic Light Simulator
====================================
Simulates the ESP32-based smart traffic light from the V2X system.

In the real hardware setup, the ESP32 reads its own internal timer,
cycles through RED / GREEN / YELLOW, and publishes each state change
to the Cloud MQTT broker (HiveMQ). The V2N bridge then forwards
those states into the IPC hub via the "traffic_light" topic.

This process replaces that entire chain for local testing: it publishes
directly to "traffic_light" so the rest of the stack (V2P, ADAS) can
be validated without any hardware or cloud connectivity.

Topic published : "traffic_light"
Data schema     :
    {
        "state"          : "RED" | "GREEN" | "YELLOW",
        "remaining_sec"  : int,          seconds until next transition
        "is_emergency"   : bool,         ambulance override active
        "zone"           : "zone1"
    }
"""

import time
from ipc_node import IPCNode

CYCLE = [
    ("RED",    8),
    ("GREEN",  6),
    ("YELLOW", 3),
]


def main() -> None:
    node = IPCNode("traffic_light_sim")

    if not node.connect():
        return

    print("[TL-SIM] Traffic light simulator started")
    print("[TL-SIM] Publishing to topic: 'traffic_light'\n")

    cycle_index = 0

    while True:
        state, duration = CYCLE[cycle_index]

        for remaining in range(duration, 0, -1):
            payload = {
                "state":         state,
                "remaining_sec": remaining,
                "is_emergency":  False,
                "zone":          "zone1",
            }
            node.publish("traffic_light", payload)
            print(
                f"[TL-SIM] publish  -> traffic_light | "
                f"state={state:<6} remaining={remaining}s"
            )
            time.sleep(1)

        cycle_index = (cycle_index + 1) % len(CYCLE)


if __name__ == "__main__":
    main()
