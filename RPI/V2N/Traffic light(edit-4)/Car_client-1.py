"""
Car_client.py
محاكي شاشة السيارة المستقبلة للبيانات المعالجة (V2X Client)
"""

import paho.mqtt.client as mqtt
import ssl
import json
import os
import struct
import time
from ipc_node import IPCNode  # ◄--- 1. استيراد نود الـ IPC للربط المحلي

# ============================================================
# SERVER CONFIGURATION (نفس إعدادات الـ Gateway)
# ============================================================
BROKER = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# التوبيك المعالج النهائي اللي جاي من الـ Gateway
PROCESSED_TOPIC = "V2X/zone1/traffic/processed"

# ============================================================
# V2N bit-packed frame (نفس فكرة NEIGHBOR_FMT بتاع V2V في uart.py)
# ============================================================
# هدف الـ frame ده: أي حد على الـ hub يقدر ياخده لو عايز (طبقة منفصلة
# عن local/traffic/processed اللي فيها كل التفاصيل الخام). الـ frame ده
# هو الملخص المضغوط بس اللي بيوصل لداشبورد data.json.
#
# Byte layout (2 bytes):
#   byte0 bits [0:2] traffic_flag    0=مفيش إشارة في المنطقة
#                                    1=فيه إشارة، العربية لازم تقف (أحمر/أصفر)
#                                    2=فيه إشارة، العربية تقدر تعدي (أخضر)
#   byte0 bits [2:3] ambulance_flag  0=مفيش إسعاف / العربية العادية متقدرش تعدي
#                                    1=فيه إسعاف ومسموح لها تعدي بأمان
#   byte0 bits [3:8] reserved (0)
#   byte1            distance_to_light_m  0-254 متر، 255 = غير معروف
V2N_FRAME_FMT = "<BB"


def pack_v2n_frame(traffic_flag: int, ambulance_flag: int, distance_m) -> bytes:
    """Pack traffic_flag (2 bits) + ambulance_flag (1 bit) + distance (1 byte)."""
    traffic_flag   &= 0b11
    ambulance_flag &= 0b1
    packed = (ambulance_flag << 2) | traffic_flag

    if distance_m is None:
        dist_byte = 255   # sentinel = "unknown / no reading"
    else:
        dist_byte = max(0, min(254, int(round(distance_m))))

    return struct.pack(V2N_FRAME_FMT, packed, dist_byte)


def compute_traffic_flag(state: str, has_data: bool) -> int:
    """0=مفيش إشارة، 1=أحمر/أصفر (قف)، 2=أخضر (اعدي)."""
    if not has_data:
        return 0
    if state == "GREEN":
        return 2
    if state in ("RED", "YELLOW"):
        return 1
    return 0


def compute_ambulance_flag(is_emergency: bool) -> int:
    """0=العربية العادية ممنوعة تعدي، 1=الإسعاف موجودة ومسموح ليها تعدي بأمان."""
    return 1 if is_emergency else 0

# ============================================================
# INITIALIZE LOCAL IPC NODE
# ============================================================
# ◄--- 2. إنشاء وتسمية نود الـ IPC الخاصة بالعربية للاتصال بالهاب
ipc_local = IPCNode("car_display")

# ============================================================
# VARIABLES TO STORE RECEIVED DATA (بدون عرض)
# ============================================================
# المتغيرات دي هتحدث نفسها دايماً في الخلفية بقيم المسافة والوقت المتبقي
latest_received_distance = None
latest_remaining_time = None


# ============================================================
# MQTT CALLBACKS
# ============================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Car Display connected to HiveMQ Cloud!")
        client.subscribe(PROCESSED_TOPIC)
        print(f"📡 Listening to Processed Traffic Feed on: {PROCESSED_TOPIC}\n")
    else:
        print(f"❌ Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    global latest_received_distance, latest_remaining_time
    try:
        payload_str = msg.payload.decode().strip()
        data = json.loads(payload_str)

        # ◄--- 3. الخطوة الأساسية: تمرير الداتا فوراً للهاب المحلي بعد استقبالها من الـ Cloud
        ipc_local.publish("local/traffic/processed", data)

        # استخراج واستلام البيانات دايماً في الخلفية
        latest_remaining_time = data.get("remaining_time", None)

        closest_vehicle = data.get("closest_vehicle", {})
        if closest_vehicle:
            latest_received_distance = closest_vehicle.get("distance_m", None)
        else:
            latest_received_distance = None

        # 2. جلب البيانات المخصصة للعرض فقط
        state = data.get("state", "UNKNOWN")
        warning_msg = data.get("warning", "Normal Traffic")
        is_emergency = data.get("is_emergency", False)

        # ◄--- NEW: حساب V2N flags ونشرهم كـ frame مضغوط على topic منفصل
        # (v2n_frame) — أي حد على الـ hub يقدر ياخده. ده بيتلخص بعدين في
        # data.json عن طريق dashboard_bridge.py.
        has_traffic_data = state in ("RED", "YELLOW", "GREEN")
        traffic_flag   = compute_traffic_flag(state, has_traffic_data)
        ambulance_flag = compute_ambulance_flag(is_emergency)
        distance_m     = latest_received_distance

        v2n_frame_bytes = pack_v2n_frame(traffic_flag, ambulance_flag, distance_m)

        ipc_local.publish("v2n_frame", {
            "timestamp":           time.time(),
            "frame_hex":           v2n_frame_bytes.hex(),
            "traffic_flag":        traffic_flag,
            "ambulance_flag":      ambulance_flag,
            "distance_to_light_m": distance_m,
        })

        # 3. تنظيف الشاشة وعرض البيانات المطلوبة فقط (بدون المسافة والوقت المتبقي)
        os.system('cls' if os.name == 'nt' else 'clear')
        print("=" * 60)
        print("🚘 V2X CAR ON-BOARD DISPLAY (OBU) 🚘")
        print("=" * 60)
        print(f"🚦 Current Light State : {state}")
        print("-" * 60)

        if is_emergency:
            print(f"⚠️ NOTIFICATION : {warning_msg}")
        else:
            print(f"ℹ️ STATUS       : {warning_msg}")

        print("=" * 60)

    except Exception as e:
        print(f"❌ Error parsing display data: {e}")


# ============================================================
# MAIN RUNNER
# ============================================================
if __name__ == "__main__":

    print("=" * 60)
    print("  V2X Car Client Initializing")
    print("=" * 60)

    # ◄--- 4. الاتصال بالهاب المحلي أولاً وتفعيل الاستماع في الخلفية
    print("🏠 Connecting Car Display to Local IPC Hub...")
    if ipc_local.connect():
        ipc_local.start_listening()
        print("🔗 Successfully linked to Local Hub via IPC-Node.")
    else:
        print("⚠️ Local Hub not found! Running in Cloud-only mode.")

    # 5. الاتصال ببروكر السحابة HiveMQ
    print("🌐 Connecting Car HUD to HiveMQ Cloud...")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Car Display disconnected.")
