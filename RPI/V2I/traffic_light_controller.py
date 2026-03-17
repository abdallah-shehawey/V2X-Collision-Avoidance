"""
 ******************************************************************************
 * @file           : traffic_light_controller.py
 * @author         : Your Name
 * @brief          : Traffic light controller module
 * @description    : Receives commands from ambulance server and controls
 *                   the actual traffic light hardware (simulated here).
 ******************************************************************************
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
from typing import Any, Dict

# ============================ Configuration ============================
BROKER = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

CONTROL_TOPIC = "v2n/traffic/light/override"

# ============================ Traffic Light Controller ============================
class TrafficLightController:
    """
    Simulates or controls a real traffic light.
    Listens for override commands and changes state accordingly.
    """
    def __init__(self):
        self.client: Optional[mqtt.Client] = None
        self.current_state = "RED"       # current light state
        self.override_active = False
        self.override_until = 0

    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Dict, rc: int) -> None:
        if rc == 0:
            print("✅ Traffic light controller connected")
            client.subscribe(CONTROL_TOPIC)
        else:
            print(f"❌ Connection failed, rc={rc}")

    def _on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        try:
            payload = json.loads(msg.payload.decode())
            print(f"📨 Received command: {payload}")

            command = payload.get("command")
            if command == "set_green":
                duration = payload.get("duration", 15)
                ambulance_id = payload.get("ambulance_id", "unknown")
                self._set_green(duration, ambulance_id)
            elif command == "restore":
                self._restore()
            else:
                print(f"⚠️ Unknown command: {command}")

        except Exception as e:
            print(f"❌ Error: {e}")

    def _set_green(self, duration: int, ambulance_id: str) -> None:
        """Set traffic light to GREEN for a specified duration."""
        print(f"🚦 Setting traffic light to GREEN for {duration}s (Ambulance {ambulance_id})")
        self.current_state = "GREEN"
        self.override_active = True
        self.override_until = time.time() + duration

        # Here you would add actual hardware control (e.g., GPIO)
        # e.g., GPIO.output(GREEN_PIN, HIGH)

    def _restore(self) -> None:
        """Restore traffic light to normal operation."""
        print("🔄 Restoring traffic light to normal state")
        self.current_state = "RED"   # or the normal cycle state
        self.override_active = False

        # Hardware restore
        # e.g., GPIO.output(GREEN_PIN, LOW); start normal cycle

    def start(self) -> None:
        """Start the controller (blocking)."""
        self.client = mqtt.Client()
        self.client.username_pw_set(USERNAME, PASSWORD)
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        self.client.connect(BROKER, PORT)
        print("🔄 Traffic light controller running...")
        self.client.loop_forever()

# ============================ Main ============================
if __name__ == "__main__":
    controller = TrafficLightController()
    controller.start()