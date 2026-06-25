import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading

# --- Configuration ---
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

OVERRIDE_TOPIC = "V2X/zone1/traffic/processed"
STATE_TOPIC    = "v2n/traffic/light/state"

# --- Shared State ---
current_state      = "RED"
is_override        = False
remaining_time     = 10
state_lock         = threading.Lock()

# ======================== MQTT Callbacks ========================

def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Traffic Light Simulator Online & Listening to Gateway commands...")
        client.subscribe(OVERRIDE_TOPIC)
    else:
        print(f"❌ Connection failed with code {reason_code}")

def on_message(client, userdata, msg):
    global current_state, is_override, remaining_time
    try:
        data = json.loads(msg.payload.decode())
        msg_emergency = data.get("is_emergency", False)
        warning_msg   = data.get("warning", "")
        
        with state_lock:
            # Check if there is an active emergency state
            if msg_emergency or "AMBULANCE" in warning_msg.upper():
                if not is_override:  # First detection of emergency
                    is_override = True
                    # If light is RED, switch to YELLOW for 3s to safely transition for pedestrians
                    if current_state == "RED":
                        current_state = "YELLOW"
                        remaining_time = 3
                    elif current_state == "YELLOW":
                        remaining_time = 3  # Confirm yellow duration
                    else:
                        # If already green, stay green for ambulance
                        current_state = "GREEN"
                        remaining_time = 15
            else:
                is_override = False
                
    except Exception as e:
        print(f"❌ Error in processing override command: {e}")

# ======================== Traffic Logic ========================

def traffic_cycle_logic(client):
    global current_state, is_override, remaining_time
    while True:
        with state_lock:
            if is_override:
                # Emergency override logic
                if current_state == "YELLOW":
                    if remaining_time <= 1:
                        # After 3s yellow, switch to Green for ambulance safety
                        current_state = "GREEN"
                        remaining_time = 15
                    else:
                        remaining_time -= 1
                elif current_state == "GREEN":
                    if remaining_time > 1:
                        remaining_time -= 1
                    else:
                        remaining_time = 5  # Extend green light safety
                elif current_state == "RED":
                    current_state = "YELLOW"
                    remaining_time = 3
            else:
                # Normal traffic cycle
                if remaining_time <= 1:
                    if   current_state == "RED":    current_state = "GREEN";  remaining_time = 15
                    elif current_state == "GREEN":  current_state = "YELLOW"; remaining_time = 3
                    elif current_state == "YELLOW": current_state = "RED";    remaining_time = 10
                else:
                    remaining_time -= 1

            snap_state = current_state
            snap_time  = remaining_time
            snap_emg   = is_override

        # Publish light state to Cloud and V2X network
        status_msg = {
            "state":          snap_state,
            "remaining_time": snap_time,
            "is_emergency":   snap_emg
        }
        try:
            client.publish(STATE_TOPIC, json.dumps(status_msg))
        except:
            pass

        # Print status on dashboard
        icon = "🔴" if snap_state == "RED" else "🟢" if snap_state == "GREEN" else "🟡"
        print(f"{icon} Light: {snap_state} | Time: {snap_time}s | Emergency: {snap_emg}")
        
        # Print strict warnings for normal cars during emergency
        if snap_emg:
            print("🚨 [⚠️ WARNING FOR NORMAL CARS] AMBULANCE DETECTED! DO NOT CROSS - CLEAR THE WAY!")
        
        time.sleep(1)

# ======================== Main Execution ========================
if __name__ == "__main__":
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    client.on_connect = on_connect
    client.on_message = on_message

    print("🌐 Connecting Traffic Light to HiveMQ Cloud...")
    client.connect(BROKER, PORT, 60)

    # Start Intelligent Traffic Cycle in a separate thread
    threading.Thread(target=traffic_cycle_logic, args=(client,), daemon=True).start()

    client.loop_forever()