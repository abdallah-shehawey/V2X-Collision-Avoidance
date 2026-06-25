"""
trafic_light.py - Smart Traffic Light Controller
=================================================
Runs on the ESP32 (or simulated). Publishes light state to:
  1. HiveMQ Cloud (MQTT) -> Intelligent Gateway -> Car_client
  2. Local IPC hub (topic: traffic_light_state) -> V2P reads it directly

transition_flag table (published every second):
    0  GREEN  -> YELLOW   (caution, start slowing)
    1  YELLOW -> RED      (must stop)
    2  RED    -> YELLOW   (wait, about to go)
    3  YELLOW -> GREEN    (about to go)
   -1  mid-phase, no imminent transition

The flag appears only in the last TRANSITION_WARN_SECS of each phase.
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading

from ipc_node import IPCNode

# HiveMQ Cloud Configuration
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

OVERRIDE_TOPIC = "V2X/zone1/traffic/processed"
STATE_TOPIC    = "v2n/traffic/light/state"

TRANSITION_WARN_SECS = 5

# Shared State
current_state  = "RED"
is_override    = False
remaining_time = 10
state_lock     = threading.Lock()
_prev_state    = "RED"

# IPC Hub — publishes to V2P directly
ipc = IPCNode("traffic_light_controller")

def connect_ipc():
    if ipc.connect():
        print("[TrafficLight] IPC hub connected.")
    else:
        print("[TrafficLight] WARNING: IPC hub not reachable.")

connect_ipc()


def get_transition_flag(state: str, remaining: int) -> int:
    """Return transition_flag only in last TRANSITION_WARN_SECS of phase."""
    if remaining > TRANSITION_WARN_SECS:
        return -1
    if state == "GREEN":  return 0
    if state == "RED":    return 2
    if state == "YELLOW":
        return 1 if _prev_state == "GREEN" else 3
    return -1


def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("Traffic Light connected to HiveMQ Cloud")
        client.subscribe(OVERRIDE_TOPIC)
    else:
        print(f"MQTT connection failed: {reason_code}")


def on_message(client, userdata, msg):
    global current_state, is_override, remaining_time
    try:
        data          = json.loads(msg.payload.decode())
        msg_emergency = data.get("is_emergency", False)
        warning_msg   = data.get("warning", "")
        with state_lock:
            if msg_emergency or "AMBULANCE" in warning_msg.upper():
                if not is_override:
                    is_override = True
                    if current_state == "RED":
                        current_state  = "YELLOW"
                        remaining_time = 3
                    elif current_state == "YELLOW":
                        remaining_time = 3
                    else:
                        current_state  = "GREEN"
                        remaining_time = 15
            else:
                is_override = False
    except Exception as e:
        print(f"Override error: {e}")


def traffic_cycle_logic(mqtt_client):
    global current_state, is_override, remaining_time, _prev_state

    while True:
        with state_lock:
            if is_override:
                if current_state == "YELLOW":
                    if remaining_time <= 1:
                        _prev_state    = "YELLOW"
                        current_state  = "GREEN"
                        remaining_time = 15
                    else:
                        remaining_time -= 1
                elif current_state == "GREEN":
                    remaining_time = remaining_time - 1 if remaining_time > 1 else 5
                elif current_state == "RED":
                    _prev_state    = "RED"
                    current_state  = "YELLOW"
                    remaining_time = 3
            else:
                if remaining_time <= 1:
                    if current_state == "RED":
                        _prev_state    = "RED"
                        current_state  = "YELLOW"
                        remaining_time = 3
                    elif current_state == "YELLOW":
                        if _prev_state == "RED":
                            _prev_state    = "YELLOW"
                            current_state  = "GREEN"
                            remaining_time = 15
                        else:
                            _prev_state    = "YELLOW"
                            current_state  = "RED"
                            remaining_time = 10
                    elif current_state == "GREEN":
                        _prev_state    = "GREEN"
                        current_state  = "YELLOW"
                        remaining_time = 3
                else:
                    remaining_time -= 1

            snap_state = current_state
            snap_time  = remaining_time
            snap_emg   = is_override
            snap_tflag = get_transition_flag(snap_state, snap_time)

        payload = {
            "state":           snap_state,
            "remaining_time":  snap_time,
            "is_emergency":    snap_emg,
            "transition_flag": snap_tflag,
        }

        # 1. MQTT -> Gateway -> Car_client
        try:
            mqtt_client.publish(STATE_TOPIC, json.dumps(payload))
        except Exception:
            pass

        # 2. IPC hub -> V2P (direct, no cloud hop)
        try:
            ipc.publish("traffic_light_state", payload)
        except Exception:
            pass

        icon   = {"RED": "[R]", "GREEN": "[G]", "YELLOW": "[Y]"}.get(snap_state, "[?]")
        tf_lbl = {0:"G->Y", 1:"Y->R", 2:"R->Y", 3:"Y->G", -1:"--"}.get(snap_tflag, "?")
        print(f"{icon} {snap_state:6s} | t={snap_time:3d}s | tf={tf_lbl} | emg={snap_emg}")
        if snap_emg:
            print("EMERGENCY - AMBULANCE - CLEARING WAY!")

        time.sleep(1)


if __name__ == "__main__":
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    client.on_connect = on_connect
    client.on_message = on_message

    print("Connecting Traffic Light to HiveMQ Cloud...")
    client.connect(BROKER, PORT, 60)

    threading.Thread(
        target=traffic_cycle_logic,
        args=(client,),
        daemon=True,
    ).start()

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nTraffic Light stopped.")
