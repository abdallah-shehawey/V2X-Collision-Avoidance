import paho.mqtt.client as mqtt
import ssl
import json
import threading

# ============================================================
# SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# Topics Config
CAMERA_TOPIC    = "v2n/camera/detection"        
TRAFFIC_TOPIC   = "v2n/traffic/light/state"     
VEHICLE_TOPIC   = "v2n/vehicle/presence"        
PROCESSED_TOPIC = "V2X/zone1/traffic/processed"  

# --- Shared State ---
latest_state = "RED"
latest_time = 10
car_counter = 0
ambulance_active = False
camera_confirmed = False 
state_lock = threading.Lock()

def process_and_publish():
    global ambulance_active, camera_confirmed, car_counter, latest_state, latest_time
    with state_lock:
        output = {
            "state": latest_state,
            "remaining_time": latest_time,
            "is_emergency": False,
            "warning": "Normal Traffic",
            "density": car_counter
        }
        
        # تفعيل الطوارئ بناءً على الكاميرا أو طلب الـ V2X
        if ambulance_active or camera_confirmed:
            output["is_emergency"] = True
            # بث تحذير صارم لمنع حركة السيارات العادية تماماً
            output["warning"] = "🚨 AMBULANCE APPROACHING! ALL NORMAL CARS MUST STOP - DO NOT CROSS! 🚨"
            
        client.publish(PROCESSED_TOPIC, json.dumps(output))

def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Intelligent Gateway connected to HiveMQ Cloud!")
        client.subscribe(CAMERA_TOPIC)
        client.subscribe(TRAFFIC_TOPIC)
        client.subscribe(VEHICLE_TOPIC)
    else:
        print(f"❌ Connection failed: {reason_code}")

def on_message(client, userdata, msg):
    global ambulance_active, camera_confirmed, car_counter, latest_state, latest_time
    try:
        payload_str = msg.payload.decode().strip()
        
        # 1. معالجة بيانات الكاميرا والـ AI
        if msg.topic == CAMERA_TOPIC:
            if "Ambulance verified" in payload_str or "ambulance" in payload_str.lower():
                with state_lock:
                    camera_confirmed = True
                print(f"📸 [Gateway AI-Log] Camera confirmed Ambulance nearby!")
            else:
                with state_lock:
                    camera_confirmed = False
            process_and_publish()

        # 2. حفظ عداد إشارة المرور وتحديثه باستمرار (طبيعي أو طوارئ)
        elif msg.topic == TRAFFIC_TOPIC:
            data = json.loads(payload_str)
            with state_lock:
                # [تعديل هندسي]: تم إزالة شرط الـ if السابق لكي يستمر السيرفر 
                # في قراءة وتحديث حالة الإشارة والوقت الحقيقي لايف حتى أثناء الطوارئ
                latest_state = data.get("state", "RED")
                latest_time = data.get("remaining_time", 10)

        # 3. معالجة بيانات الـ V2X من العربيات
        elif msg.topic == VEHICLE_TOPIC:
            data = json.loads(payload_str)
            vehicle_type = data.get("vehicle_type", "")
            command = data.get("command", "")
            v_id = data.get("ambulance_id", data.get("vehicle_id", ""))
            
            if vehicle_type == "AMBULANCE" or (v_id and "A-" in v_id):
                if command == "clear_priority":
                    print("🚑 [Gateway V2X] Ambulance passed completely. Clearing emergency.")
                    with state_lock:
                        ambulance_active = False
                else:
                    print("🚑 [Gateway V2X] Ambulance requesting priority alert!")
                    with state_lock:
                        ambulance_active = True
            else:
                with state_lock:
                    car_counter = (car_counter + 1) % 10
            process_and_publish()

    except Exception as e:
        print(f"❌ Error processing MQTT Message: {e}")

# ======================== MAIN EXECUTION ========================
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