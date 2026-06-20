import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading

# --- الإعدادات ---
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
            # التحقق مما إذا كانت هناك حالة طوارئ نشطة فعلياً
            if msg_emergency or "AMBULANCE" in warning_msg.upper():
                if not is_override:  # أول لقطة لبدء حالة الطوارئ
                    is_override = True
                    # التعديل الهندسي: لو الإشارة حمراء، اقلب أصفر أولاً لمدة 3 ثوانٍ لحماية المشاة
                    if current_state == "RED":
                        current_state = "YELLOW"
                        remaining_time = 3
                    elif current_state == "YELLOW":
                        remaining_time = 3  # تأكيد تثبيت الـ 3 ثوانٍ للأصفر
                    else:
                        # لو كانت خضراء بالفعل، تظل خضراء للإسعاف
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
                # منطق التحكم أثناء الطوارئ والـ Transition
                if current_state == "YELLOW":
                    if remaining_time <= 1:
                        # بعد انتهاء الـ 3 ثوانٍ الصفراء، اقلب أخضر بأمان للإسعاف
                        current_state = "GREEN"
                        remaining_time = 15
                    else:
                        remaining_time -= 1
                elif current_state == "GREEN":
                    if remaining_time > 1:
                        remaining_time -= 1
                    else:
                        remaining_time = 5  # تأمين امتداد الإشارة الخضراء حتى خروج الإسعاف تماماً
                elif current_state == "RED":
                    current_state = "YELLOW"
                    remaining_time = 3
            else:
                # الدورة الطبيعية للإشارة في غياب الطوارئ
                if remaining_time <= 1:
                    if   current_state == "RED":    current_state = "GREEN";  remaining_time = 15
                    elif current_state == "GREEN":  current_state = "YELLOW"; remaining_time = 3
                    elif current_state == "YELLOW": current_state = "RED";    remaining_time = 10
                else:
                    remaining_time -= 1

            snap_state = current_state
            snap_time  = remaining_time
            snap_emg   = is_override

        # نشر حالة الإشارة للكلاود وباقي شبكة الـ V2X
        status_msg = {
            "state":          snap_state,
            "remaining_time": snap_time,
            "is_emergency":   snap_emg
        }
        try:
            client.publish(STATE_TOPIC, json.dumps(status_msg))
        except:
            pass

        # الطباعة على شاشة التحكم والتحذير لايف
        icon = "🔴" if snap_state == "RED" else "🟢" if snap_state == "GREEN" else "🟡"
        print(f"{icon} Light: {snap_state} | Time: {snap_time}s | Emergency: {snap_emg}")
        
        # طباعة التحذير الصارم للعربيات العادية في السيرفر عند وجود حالة طوارئ
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

    # تشغيل الدورة المرورية الذكية في Thread منفصلة
    threading.Thread(target=traffic_cycle_logic, args=(client,), daemon=True).start()

    client.loop_forever()