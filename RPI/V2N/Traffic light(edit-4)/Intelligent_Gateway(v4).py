# -*- coding: utf-8 -*-
import paho.mqtt.client as mqtt
import ssl
import json
import threading
import time

# ============================================================
# SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# ============================================================
# TOPICS
# ============================================================
CAMERA_TOPIC          = "v2n/camera/detection"         
CAMERA_DETECTION_TOPIC= "v2n/camera/vehicle_data"      
TRAFFIC_TOPIC         = "v2n/traffic/light/state"
VEHICLE_TOPIC         = "v2n/vehicle/presence"
PROCESSED_TOPIC       = "V2X/zone1/traffic/processed"

# ============================================================
# SHARED STATE
# ============================================================
latest_state      = "RED"
latest_time       = 10
car_counter       = 0
ambulance_active  = False   # طلب طوارئ من V2X
camera_confirmed  = False   # تأكيد طوارئ من الكاميرا القديمة
camera_ambulance  = False   # إسعاف اتكشف من كود الكاميرا الحديث
state_lock        = threading.Lock()

# قاموس يخزن آخر بيانات كل لوحة مكتشفة
vehicle_registry  = {}
registry_lock     = threading.Lock()

# ============================================================
# PUBLISH PROCESSED DATA
# ============================================================
def process_and_publish():
    """يجمع كل البيانات ويبعت على الـ processed topic واثق وآمن برمجياً"""
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time

    with state_lock:
        # نحسب المركبات القريبة (أقل من 50 متر)
        with registry_lock:
            nearby_vehicles = {
                pid: data for pid, data in vehicle_registry.items()
                if data.get("distance_m", 999) <= 50
            }
            closest = min(
                nearby_vehicles.values(),
                key=lambda x: x["distance_m"],
                default=None
            )

        is_emergency = ambulance_active or camera_confirmed or camera_ambulance

        output = {
            "state"          : latest_state,
            "remaining_time" : latest_time,
            "is_emergency"   : is_emergency,
            "warning"        : "Normal Traffic",
            "density"        : car_counter,
            "closest_vehicle": {
                "plate_id"  : closest["plate_id"]   if closest else None,
                "distance_m": closest["distance_m"] if closest else None
            },
            "nearby_count"   : len(nearby_vehicles)
        }

        # 🛠️ إصلاح منطق التحذير وتحديث الاسم الاحتياطي ليكون متوافقاً مع Rex
        if is_emergency:
            emergency_plate = None
            with registry_lock:
                for pid, data in vehicle_registry.items():
                    if data.get("is_ambulance"):
                        emergency_plate = pid
                        break

            # فحص آمن: إذا وجدنا مسافة من الكاميرا نطبعها، وإلا نرسل تحذيراً عاماً فورياً دون توقف الكود
            if closest and closest.get("distance_m") is not None:
                dist_val = closest["distance_m"]
                output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate if emergency_plate else 'Rex'}] at {dist_val}m! ALL NORMAL CARS MUST STOP! 🚨"
            else:
                output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate if emergency_plate else 'Rex'}]! ALL NORMAL CARS MUST STOP - DO NOT CROSS! 🚨"

        client.publish(PROCESSED_TOPIC, json.dumps(output))
        print(f"📤 Published → state:{latest_state} | emergency:{is_emergency} | nearby:{len(nearby_vehicles)} vehicles")

# ============================================================
# MQTT CALLBACKS
# ============================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Intelligent Gateway connected to HiveMQ Cloud!")
        client.subscribe(CAMERA_TOPIC)
        client.subscribe(CAMERA_DETECTION_TOPIC)   
        client.subscribe(TRAFFIC_TOPIC)
        client.subscribe(VEHICLE_TOPIC)
        print(f"📡 Subscribed to topics:")
        print(f"   • {CAMERA_TOPIC}")
        print(f"   • {CAMERA_DETECTION_TOPIC}  ← New vehicle data feed")
        print(f"   • {TRAFFIC_TOPIC}")
        print(f"   • {VEHICLE_TOPIC}")
    else:
        print(f"❌ Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time

    try:
        payload_str = msg.payload.decode().strip()

        # ----------------------------------------------------------
        # 1. بيانات العربيات من كود الكاميرا
        # ----------------------------------------------------------
        if msg.topic == CAMERA_DETECTION_TOPIC:
            data       = json.loads(payload_str)
            plate_id   = data.get("plate_id", "???")
            distance   = data.get("distance_m", 999)
            is_amb     = data.get("is_ambulance", False)

            # تحديث سجل العربيات
            with registry_lock:
                vehicle_registry[plate_id] = {
                    "plate_id"   : plate_id,
                    "distance_m" : distance,
                    "is_ambulance": is_amb,
                    "last_seen"  : data.get("timestamp", 0)
                }

            print(f"\n🎥 [CameraFeed] plate={plate_id} | dist={distance}m | ambulance={is_amb}")

            # 🛠️ التعديل: تفعيل طوارئ الكاميرا إذا تطابق الاسم الفعلي Rex بنفس حالة الأحرف
            with state_lock:
                if is_amb or (plate_id.strip() == "Rex"):
                    camera_ambulance = True
                else:
                    camera_ambulance = False

            process_and_publish()

        # ----------------------------------------------------------
        # 2. نصوص من كاميرا أخرى (القديمة)
        # ----------------------------------------------------------
        elif msg.topic == CAMERA_TOPIC:
            if "Ambulance verified" in payload_str or "ambulance" in payload_str.lower():
                with state_lock:
                    camera_confirmed = True
                print(f"📸 [Gateway AI-Log] Camera confirmed Ambulance nearby!")
            else:
                with state_lock:
                    camera_confirmed = False
            process_and_publish()

        # ----------------------------------------------------------
        # 3. حالة إشارة المرور
        # ----------------------------------------------------------
        elif msg.topic == TRAFFIC_TOPIC:
            data = json.loads(payload_str)
            with state_lock:
                latest_state = data.get("state", "RED")
                latest_time  = data.get("remaining_time", 10)
            print(f"🚦 [TrafficLight] state={latest_state} | time={latest_time}s")

        # ----------------------------------------------------------
        # 4. بيانات V2X من العربيات مباشرة
        # ----------------------------------------------------------
        elif msg.topic == VEHICLE_TOPIC:
            data         = json.loads(payload_str)
            vehicle_type = data.get("vehicle_type", "")
            command      = data.get("command", "")
            v_id         = data.get("ambulance_id", data.get("vehicle_id", ""))

            # 🛠️ التعديل: فحص ومطابقة المعرف الثابت Rex بنفس حالة الأحرف لدعم الـ V2X بالتوازي
            is_v2x_ambulance = (vehicle_type == "AMBULANCE") or (v_id and v_id.strip() == "Rex")

            if is_v2x_ambulance:
                if command == "clear_priority":
                    print("🚑 [Gateway V2X] Ambulance passed. Clearing emergency.")
                    with state_lock:
                        ambulance_active = False
                    with registry_lock:
                        if v_id in vehicle_registry:
                            vehicle_registry.pop(v_id, None)
                else:
                    print(f"🚑 [Gateway V2X] Ambulance [{v_id}] requesting priority!")
                    with state_lock:
                        ambulance_active = True
            else:
                with state_lock:
                    car_counter = (car_counter + 1) % 10

            process_and_publish()

    except Exception as e:
        print(f"❌ Error processing MQTT Message: {e}")

# ============================================================
# MAIN
# ============================================================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

client.on_connect = on_connect
client.on_message = on_message

print("🌐 Core Connecting to HiveMQ Cloud Server...")
client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n🛑 Gateway stopped safely.")