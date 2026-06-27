"""
 ******************************************************************************
 * @file           : ambulance_server.py
 * @author         : Alaa Hassan
 * @brief          : Central server for ambulance priority management
 * @description    : Listens for ambulance requests, grants priority,
 *                   and commands the traffic light controller.
 ******************************************************************************
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading
from typing import Dict, Optional, Any

# ============================ Configuration ============================
BROKER = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

REQUEST_TOPIC = "v2n/ambulance/request"
CONFIRM_TOPIC = "v2n/ambulance/confirmation"
TRAFFIC_CONTROL_TOPIC = "v2n/traffic/light/override"

PRIORITY_DURATION = 15          # seconds
DISTANCE_THRESHOLD = 100        # meters
MAX_ACTIVE_REQUESTS = 1         # only one active at a time (simplified)

# ============================ Data Structures ============================
class ActiveRequest:
    """Represents an active ambulance priority request."""
    def __init__(self, ambulance_id: str, intersection_id: str, duration: int):
        self.ambulance_id = ambulance_id
        self.intersection_id = intersection_id
        self.start_time = time.time()
        self.duration = duration
        self.timer: Optional[threading.Timer] = None

class AmbulanceServer:
    """
    Central server module.
    Manages incoming requests, grants priority, and sends commands to traffic light.
    """
    def __init__(self):
        self.client: Optional[mqtt.Client] = None
        self.active_requests: Dict[str, ActiveRequest] = {}  # ambulance_id -> ActiveRequest

    # ============================ Private methods ============================
    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Dict, rc: int) -> None:
        """Callback when connected to broker."""
        if rc == 0:
            print("✅ Server connected to broker")
            client.subscribe(REQUEST_TOPIC)
        else:
            print(f"❌ Connection failed, return code {rc}")

    def _on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        """Callback when a message is received on request topic."""
        try:
            payload = json.loads(msg.payload.decode())
            print(f"📨 Received: {payload}")

            command = payload.get("command")
            ambulance_id = payload.get("ambulance_id")

            if command == "request_priority":
                self._handle_request(
                    ambulance_id,
                    payload.get("distance"),
                    payload.get("intersection_id")
                )
            elif command == "clear_priority":
                self._handle_clear(ambulance_id)
            elif command == "cancel_priority":
                self._handle_cancel(ambulance_id)
            else:
                print(f"⚠️ Unknown command: {command}")

        except Exception as e:
            print(f"❌ Error processing message: {e}")

    def _handle_request(self, ambulance_id: str, distance: int, intersection_id: str) -> None:
        """Process a priority request."""
        # Check distance
        if distance > DISTANCE_THRESHOLD:
            self._send_confirmation(ambulance_id, "wait", intersection_id, reason="too_far")
            return

        # Check if already active for this ambulance
        if ambulance_id in self.active_requests:
            self._send_confirmation(ambulance_id, "already_active", intersection_id)
            return

        # Check if another ambulance is active (simplified: only one allowed)
        if self.active_requests:
            self._send_confirmation(ambulance_id, "queued", intersection_id, reason="another_active")
            return

        # Grant priority
        print(f"🚑 Granting priority to {ambulance_id}")
        req = ActiveRequest(ambulance_id, intersection_id, PRIORITY_DURATION)
        self.active_requests[ambulance_id] = req

        # Command traffic light to turn green
        self._override_traffic_light("GREEN", PRIORITY_DURATION, ambulance_id)

        # Start timeout timer
        timer = threading.Timer(PRIORITY_DURATION, self._priority_timeout, args=[ambulance_id])
        timer.daemon = True
        timer.start()
        req.timer = timer

        # Send confirmation
        self._send_confirmation(ambulance_id, "priority_granted", intersection_id, duration=PRIORITY_DURATION)

    def _handle_clear(self, ambulance_id: str) -> None:
        """Process a clear_priority message (ambulance passed)."""
        if ambulance_id in self.active_requests:
            print(f"✅ Clearing priority for {ambulance_id}")
            req = self.active_requests[ambulance_id]
            if req.timer:
                req.timer.cancel()
            del self.active_requests[ambulance_id]

            # If no active requests left, restore traffic light
            if not self.active_requests:
                self._restore_traffic_light(ambulance_id)

            self._send_confirmation(ambulance_id, "priority_cleared", None)
        else:
            print(f"⚠️ Clear request for non-active ambulance {ambulance_id}")

    def _handle_cancel(self, ambulance_id: str) -> None:
        """Process a cancel_priority (ambulance changed route)."""
        # Same as clear for simplicity
        self._handle_clear(ambulance_id)

    def _priority_timeout(self, ambulance_id: str) -> None:
        """Called when priority duration expires without clear."""
        print(f"⏰ Priority timeout for ambulance {ambulance_id}")
        if ambulance_id in self.active_requests:
            del self.active_requests[ambulance_id]
            if not self.active_requests:
                self._restore_traffic_light(ambulance_id)

    def _override_traffic_light(self, state: str, duration: int, ambulance_id: str) -> None:
        """Send command to traffic light controller."""
        command = {
            "command": "set_green",  # For now only GREEN supported
            "duration": duration,
            "ambulance_id": ambulance_id
        }
        self.client.publish(TRAFFIC_CONTROL_TOPIC, json.dumps(command))
        print(f"🚦 Sent override command: {command}")

    def _restore_traffic_light(self, ambulance_id: Optional[str] = None) -> None:
        """Send restore command to traffic light controller."""
        command = {"command": "restore"}
        if ambulance_id:
            command["ambulance_id"] = ambulance_id
        self.client.publish(TRAFFIC_CONTROL_TOPIC, json.dumps(command))
        print("🔄 Sent restore command")

    def _send_confirmation(self, ambulance_id: str, status: str, intersection_id: Optional[str],
                           duration: Optional[int] = None, reason: Optional[str] = None) -> None:
        """Send confirmation message to the ambulance."""
        confirm = {
            "ambulance_id": ambulance_id,
            "status": status,
            "timestamp": int(time.time() * 1000)
        }
        if intersection_id:
            confirm["intersection_id"] = intersection_id
        if duration:
            confirm["duration"] = duration
        if reason:
            confirm["reason"] = reason

        self.client.publish(CONFIRM_TOPIC, json.dumps(confirm))
        print(f"📤 Confirmation sent: {confirm}")

    # ============================ Public API ============================
    def start(self) -> None:
        """Start the server (blocking)."""
        self.client = mqtt.Client()
        self.client.username_pw_set(USERNAME, PASSWORD)
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        self.client.connect(BROKER, PORT)
        print("🔄 Server is running, waiting for requests...")
        self.client.loop_forever()

# ============================ Main entry ============================
if __name__ == "__main__":
    server = AmbulanceServer()
    server.start()