"""
 ******************************************************************************
 * @file           : ambulance_client.py
 * @author         : Alaa Hassan
 * @brief          : Ambulance vehicle client module
 * @description    : Handles sending priority requests and receiving confirmations
 ******************************************************************************
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
from typing import Optional, Dict, Any

# ============================ Configuration ============================
BROKER = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

REQUEST_TOPIC = "v2n/ambulance/request"
CONFIRM_TOPIC = "v2n/ambulance/confirmation"

# Vehicle fixed data (could be loaded from config)
AMBULANCE_ID = "AMB-001"
INTERSECTION_ID = "INT-001"

# Timeouts and retries
RESPONSE_TIMEOUT = 5      # seconds
RETRY_MAX = 3
RETRY_DELAY = 1           # seconds between retries

# ============================ Data Structures ============================
class ConfirmationMessage:
    """Represents a confirmation message received from server."""
    def __init__(self, ambulance_id: str, status: str, intersection_id: Optional[str] = None,
                 duration: Optional[int] = None, reason: Optional[str] = None,
                 timestamp: Optional[int] = None):
        self.ambulance_id = ambulance_id
        self.status = status
        self.intersection_id = intersection_id
        self.duration = duration
        self.reason = reason
        self.timestamp = timestamp

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> 'ConfirmationMessage':
        return ConfirmationMessage(
            ambulance_id=data.get("ambulance_id"),
            status=data.get("status"),
            intersection_id=data.get("intersection_id"),
            duration=data.get("duration"),
            reason=data.get("reason"),
            timestamp=data.get("timestamp")
        )

class AmbulanceClient:
    """
    Ambulance client module.
    Handles connection to MQTT broker, sending priority requests,
    and waiting for confirmations.
    """
    def __init__(self):
        self.client: Optional[mqtt.Client] = None
        self.confirmation_received = False
        self.received_confirmation: Optional[ConfirmationMessage] = None
        self._expected_id = AMBULANCE_ID

    # ============================ Private methods ============================
    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Dict, rc: int) -> None:
        """Callback when the client connects to the broker."""
        if rc == 0:
            print("✅ Connected to broker")
            # Subscribe to confirmation topic
            client.subscribe(CONFIRM_TOPIC)
        else:
            print(f"❌ Connection failed, return code {rc}")

    def _on_message(self, client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        """Callback when a message is received."""
        try:
            payload = json.loads(msg.payload.decode())
            print(f"📩 Received: {payload}")

            # Check if it's for our ambulance
            if payload.get("ambulance_id") == self._expected_id:
                self.confirmation_received = True
                self.received_confirmation = ConfirmationMessage.from_dict(payload)
            else:
                print(f"⚠️ Ignored message for another vehicle: {payload}")
        except Exception as e:
            print(f"❌ Error parsing message: {e}")

    def _setup_client(self) -> None:
        """Initialize and configure the MQTT client."""
        self.client = mqtt.Client()
        self.client.username_pw_set(USERNAME, PASSWORD)
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def _send_request(self, distance: int) -> None:
        """Send a priority request to the server."""
        request = {
            "ambulance_id": AMBULANCE_ID,
            "command": "request_priority",
            "intersection_id": INTERSECTION_ID,
            "distance": distance,
            "timestamp": int(time.time() * 1000)
        }
        print(f"📤 Sending request: {request}")
        self.client.publish(REQUEST_TOPIC, json.dumps(request))

    def _wait_for_confirmation(self, timeout: float) -> bool:
        """Wait for a confirmation message up to timeout seconds."""
        start_time = time.time()
        while not self.confirmation_received and (time.time() - start_time) < timeout:
            time.sleep(0.1)
        return self.confirmation_received

    def _send_clear(self) -> None:
        """Send a clear_priority message after passing the intersection."""
        clear_msg = {
            "ambulance_id": AMBULANCE_ID,
            "command": "clear_priority",
            "intersection_id": INTERSECTION_ID,
            "timestamp": int(time.time() * 1000)
        }
        print("📤 Sending clear_priority...")
        self.client.publish(REQUEST_TOPIC, json.dumps(clear_msg))

    # ============================ Public API ============================
    def request_priority(self, distance: int) -> bool:
        """
        Send a priority request to the server and wait for confirmation.
        Args:
            distance: distance to the intersection in meters.
        Returns:
            True if priority granted, False otherwise.
        """
        for attempt in range(1, RETRY_MAX + 1):
            print(f"\n📤 Attempt {attempt} with distance={distance}m")
            self.confirmation_received = False
            self.received_confirmation = None

            # Setup and connect
            self._setup_client()
            self.client.connect(BROKER, PORT)
            self.client.loop_start()
            time.sleep(1)  # Allow connection to establish

            # Send request
            self._send_request(distance)

            # Wait for confirmation
            success = self._wait_for_confirmation(RESPONSE_TIMEOUT)

            # Clean up connection
            self.client.loop_stop()
            self.client.disconnect()

            if success:
                print(f"✅ Got confirmation: {self.received_confirmation.status}")
                return True
            else:
                print(f"⏰ Timeout, retrying in {RETRY_DELAY}s...")
                time.sleep(RETRY_DELAY)

        print("❌ All attempts failed")
        return False

    def clear_priority(self) -> None:
        """Notify server that the ambulance has passed the intersection."""
        self._setup_client()
        self.client.connect(BROKER, PORT)
        self.client.loop_start()
        time.sleep(1)
        self._send_clear()
        time.sleep(1)  # Ensure message is sent
        self.client.loop_stop()
        self.client.disconnect()
        print("✅ Clear message sent")

# ============================ Main example ============================
if __name__ == "__main__":
    # Example usage
    client = AmbulanceClient()
    if client.request_priority(distance=50):
        print("🚑 Ambulance is passing the intersection...")
        time.sleep(5)  # Simulate passing
        client.clear_priority()
    else:
        print("⚠️ No confirmation, proceed with caution.")