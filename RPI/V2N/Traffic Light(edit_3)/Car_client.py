"""
******************************************************************************
* @file           : vehicle_client.py
* @author         : Amira Atef
* @brief          : Unified Vehicle Client — works for ALL vehicle types
* @description    :
*   كود موحد لكل العربيات.
*   السيرفر بيحدد نوع العربية من الـ ID:
*     - A-xxx   → إسعاف  → سيناريو الأولوية
*     - C-xxx   → عادية  → تستقبل حالة الإشارة بس
*   العربية نفسها مش محتاجة تعرف نوعها — السيرفر هو اللي بيقرر.
******************************************************************************
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading
from typing import Optional, Dict, Any
from enum import Enum
from config import (
    NORMAL_CAR_ID, AMBULANCE_ID,
    NORMAL_CAR_DISTANCE, AMBULANCE_DISTANCE,
    INTERSECTION_ID, CAR_LISTEN_DURATION, AMBULANCE_PASS_DURATION
)

# ============================ Configuration ============================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# Topics
VEHICLE_TOPIC    = "v2n/vehicle/presence"          # كل العربيات تبعت هنا
TRAFFIC_TOPIC    = "v2n/traffic/light/state"        # حالة الإشارة — كل العربيات تستقبل
CONFIRM_TOPIC    = "v2n/ambulance/confirmation"     # تأكيد الأولوية — الإسعاف بس

# Timing
RESPONSE_TIMEOUT = 5
RETRY_MAX        = 3
RETRY_DELAY      = 1
PRESENCE_INTERVAL = 2   # ثواني بين كل presence update


# ============================ Vehicle Type ============================
class VehicleType(Enum):
    NORMAL    = "normal"
    AMBULANCE = "ambulance"

def detect_vehicle_type(vehicle_id: str) -> VehicleType:
    """
    بيحدد نوع العربية من الـ ID.
    A-xxx → إسعاف (prefix موحد مع الـ Gateway)
    أي حاجة تانية → عادية
    """
    vid = vehicle_id.upper()
    if vid.startswith("A") and vid[1:].isdigit():
        return VehicleType.AMBULANCE
    return VehicleType.NORMAL


# ============================ Server Response ============================
class ServerResponse:
    """الرد اللي بييجي من السيرفر للعربية."""
    def __init__(self, vehicle_id: str, status: str,
                 intersection_id: Optional[str] = None,
                 duration: Optional[int]         = None,
                 reason: Optional[str]           = None,
                 timestamp: Optional[int]        = None):
        self.vehicle_id      = vehicle_id
        self.status          = status        # "granted" / "denied"
        self.intersection_id = intersection_id
        self.duration        = duration
        self.reason          = reason
        self.timestamp       = timestamp

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> 'ServerResponse':
        return ServerResponse(
            vehicle_id      = data.get("ambulance_id") or data.get("vehicle_id"),
            status          = data.get("status"),
            intersection_id = data.get("intersection_id"),
            duration        = data.get("duration"),
            reason          = data.get("reason"),
            timestamp       = data.get("timestamp")
        )

    def __repr__(self):
        return f"<ServerResponse status={self.status} duration={self.duration}s reason={self.reason}>"


# ============================ Unified Vehicle Client ============================
class VehicleClient:
    """
    كود موحد لكل العربيات.

    - كل عربية بتبعت presence مع الـ ID والمسافة بشكل دوري.
    - السيرفر هو اللي بيشوف الـ ID ويقرر:
        * A-xxx  → يفعّل سيناريو الأولوية ويبعت confirmation
        * C-xxx  → يبعتله حالة الإشارة بس

    Usage:
        # عربية عادية
        car = VehicleClient(vehicle_id="C123", intersection_id="INT-001")
        car.connect()
        car.start_presence_updates(distance=200)   # يبدأ يبعت presence كل 2 ثانية
        # العربية هتستقبل حالة الإشارة تلقائياً في الـ callback

        # إسعاف
        amb = VehicleClient(vehicle_id="A123", intersection_id="INT-001")
        amb.connect()
        if amb.request_priority(distance=50):
            time.sleep(5)
            amb.clear_priority()
        amb.disconnect()
    """

    def __init__(self, vehicle_id: str, intersection_id: str = "INT-001"):
        self.vehicle_id      = vehicle_id
        self.intersection_id = intersection_id
        self.vehicle_type    = detect_vehicle_type(vehicle_id)

        # MQTT
        self.client: Optional[mqtt.Client] = None
        self._connected = False

        # Ambulance-specific state
        self._confirmation_received = False
        self._received_response: Optional[ServerResponse] = None

        # Presence thread
        self._presence_active   = False
        self._presence_thread: Optional[threading.Thread] = None
        self._current_distance  = 999

        # Traffic light state (for normal cars)
        self.traffic_state: Optional[Dict] = None

        print(f"🚗 Vehicle initialized: {self.vehicle_id} | Type: {self.vehicle_type.value}")

    # ======================== MQTT Setup ========================

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            self._connected = True
            print(f"✅ [{self.vehicle_id}] Connected to broker")

            # كل العربيات تستقبل حالة الإشارة
            client.subscribe(TRAFFIC_TOPIC)

            # الإسعاف بس يستقبل الـ confirmation
            if self.vehicle_type == VehicleType.AMBULANCE:
                client.subscribe(CONFIRM_TOPIC)
                print(f"🚑 [{self.vehicle_id}] Subscribed to confirmation topic")
        else:
            print(f"❌ [{self.vehicle_id}] Connection failed, rc={reason_code}")

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        self._connected = False
        if reason_code != 0:
            print(f"⚠️ [{self.vehicle_id}] Unexpected disconnect, rc={reason_code}")

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
            topic   = msg.topic

            # ---- حالة الإشارة — كل العربيات ----
            if topic == TRAFFIC_TOPIC:
                self.traffic_state = payload
                self._handle_traffic_state(payload)

            # ---- Confirmation — الإسعاف بس ----
            elif topic == CONFIRM_TOPIC:
                if payload.get("ambulance_id") == self.vehicle_id or \
                   payload.get("vehicle_id")   == self.vehicle_id:
                    self._confirmation_received = True
                    self._received_response     = ServerResponse.from_dict(payload)
                    print(f"📩 [{self.vehicle_id}] Confirmation: {self._received_response}")
                # تجاهل الـ confirmations بتاعت العربيات التانية

        except Exception as e:
            print(f"❌ [{self.vehicle_id}] Error in on_message: {e}")

    def _handle_traffic_state(self, payload: Dict):
        """
        بيعالج حالة الإشارة اللي وصلت.
        العربية العادية تستخدمه عشان تعرف تعمل إيه.
        الإسعاف في حالة الطوارئ ممكن يتجاهله.
        """
        state     = payload.get("state", "?")
        remaining = payload.get("remaining_time", "?")
        emergency = payload.get("is_emergency", False)

        icon = "🔴" if state == "RED" else "🟢" if state == "GREEN" else "🟡"

        if emergency:
            print(f"🚨 [{self.vehicle_id}] EMERGENCY MODE — Light: {icon}{state} | Time: {remaining}s")
        else:
            print(f"{icon} [{self.vehicle_id}] Traffic Light: {state} | Time: {remaining}s")

    # ======================== Connection ========================

    def connect(self) -> bool:
        """اتصل بالبروكر مرة واحدة."""
        if self._connected:
            return True

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.username_pw_set(USERNAME, PASSWORD)
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS)
        self.client.on_connect    = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message    = self._on_message

        try:
            self.client.connect(BROKER, PORT)
            self.client.loop_start()
            for _ in range(30):
                if self._connected:
                    return True
                time.sleep(0.1)
            print(f"❌ [{self.vehicle_id}] Connection timeout")
            return False
        except Exception as e:
            print(f"❌ [{self.vehicle_id}] Connect error: {e}")
            return False

    def disconnect(self):
        """أغلق الاتصال."""
        self.stop_presence_updates()
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self._connected = False
            print(f"🔌 [{self.vehicle_id}] Disconnected")

    # ======================== Presence Updates ========================

    def _presence_loop(self):
        """Loop بيبعت presence message كل PRESENCE_INTERVAL ثانية."""
        while self._presence_active:
            self._send_presence(self._current_distance)
            time.sleep(PRESENCE_INTERVAL)

    def _send_presence(self, distance: int):
        """يبعت presence message للسيرفر."""
        msg = {
            "vehicle_id":     self.vehicle_id,
            "vehicle_type":   self.vehicle_type.value,
            "intersection_id": self.intersection_id,
            "distance":       distance,
            "timestamp":      int(time.time() * 1000)
        }
        self.client.publish(VEHICLE_TOPIC, json.dumps(msg))
        print(f"📡 [{self.vehicle_id}] Presence sent | distance={distance}m")

    def start_presence_updates(self, distance: int):
        """
        ابدأ إرسال presence updates بشكل دوري.
        للعربيات العادية: السيرفر هيستقبل الـ ID ويعرف إنها عادية.
        """
        if not self._connected:
            print(f"❌ [{self.vehicle_id}] Not connected")
            return

        self._current_distance  = distance
        self._presence_active   = True
        self._presence_thread   = threading.Thread(target=self._presence_loop, daemon=True)
        self._presence_thread.start()
        print(f"📡 [{self.vehicle_id}] Presence updates started")

    def update_distance(self, distance: int):
        """تحديث المسافة — الـ presence loop هيبعت القيمة الجديدة في الـ interval الجاي."""
        self._current_distance = distance

    def stop_presence_updates(self):
        """وقف إرسال الـ presence."""
        self._presence_active = False
        if self._presence_thread:
            self._presence_thread.join(timeout=3)

    # ======================== Ambulance-only API ========================

    def _assert_ambulance(self):
        if self.vehicle_type != VehicleType.AMBULANCE:
            raise RuntimeError(
                f"[{self.vehicle_id}] هذه الوظيفة للإسعاف فقط. "
                f"نوع العربية الحالي: {self.vehicle_type.value}"
            )

    def request_priority(self, distance: int) -> bool:
        """
        [إسعاف فقط] ابعت طلب أولوية وانتظر الـ confirmation.
        العربيات العادية لو استدعت الدالة دي هتاخد RuntimeError.
        """
        self._assert_ambulance()

        if not self._connected:
            if not self.connect():
                return False

        for attempt in range(1, RETRY_MAX + 1):
            print(f"\n📤 [{self.vehicle_id}] Priority request — attempt {attempt} | distance={distance}m")

            self._confirmation_received = False
            self._received_response     = None

            # بعت الـ request
            request = {
                "ambulance_id":    self.vehicle_id,
                "vehicle_type":    self.vehicle_type.value,
                "command":         "request_priority",
                "intersection_id": self.intersection_id,
                "distance":        distance,
                "timestamp":       int(time.time() * 1000)
            }
            self.client.publish(VEHICLE_TOPIC, json.dumps(request))

            # انتظر الـ confirmation
            start = time.time()
            while not self._confirmation_received and (time.time() - start) < RESPONSE_TIMEOUT:
                time.sleep(0.1)

            if self._confirmation_received:
                resp = self._received_response
                if resp.status == "granted":
                    print(f"✅ [{self.vehicle_id}] Priority GRANTED for {resp.duration}s")
                    return True
                else:
                    print(f"⛔ [{self.vehicle_id}] Priority DENIED — {resp.reason}")
                    return False
            else:
                print(f"⏰ [{self.vehicle_id}] Timeout, retrying in {RETRY_DELAY}s...")
                time.sleep(RETRY_DELAY)

        print(f"❌ [{self.vehicle_id}] All attempts failed")
        return False

    def clear_priority(self):
        """[إسعاف فقط] أبلغ السيرفر إن الإسعاف عدى التقاطع."""
        self._assert_ambulance()

        if not self._connected:
            if not self.connect():
                return

        clear_msg = {
            "ambulance_id":    self.vehicle_id,
            "vehicle_type":    self.vehicle_type.value,
            "command":         "clear_priority",
            "intersection_id": self.intersection_id,
            "timestamp":       int(time.time() * 1000)
        }
        self.client.publish(VEHICLE_TOPIC, json.dumps(clear_msg))
        time.sleep(0.5)
        print(f"✅ [{self.vehicle_id}] Clear priority sent")


# ============================ Main — Demo ============================
if __name__ == "__main__":

    print("=" * 55)
    print("  V2X Unified Vehicle Client Demo")
    print("=" * 55)

    # ---- سيناريو 1: عربية عادية ----
    print("\n🚗 Scenario 1: Normal Car")
    car = VehicleClient(vehicle_id=NORMAL_CAR_ID, intersection_id=INTERSECTION_ID)
    if car.connect():
        car.start_presence_updates(distance=NORMAL_CAR_DISTANCE)
        time.sleep(CAR_LISTEN_DURATION)
        car.disconnect()

    print("\n" + "-" * 55)

    # ---- سيناريو 2: إسعاف ----
    print("\n🚑 Scenario 2: Ambulance")
    amb = VehicleClient(vehicle_id=AMBULANCE_ID, intersection_id=INTERSECTION_ID)
    if amb.connect():
        if amb.request_priority(distance=AMBULANCE_DISTANCE):
            print("🚑 Ambulance passing intersection...")
            time.sleep(AMBULANCE_PASS_DURATION)
            amb.clear_priority()
        else:
            print("⚠️ No priority granted — proceed with caution.")
        amb.disconnect()
