import paho.mqtt.client as mqtt
import ssl
import json

broker = "1470a790202943d6b60bda272c6760b8.s1.eu.hivemq.cloud"
port = 8883
username = "v2n_admin"
password = "V2n@2026!"

# العربية الخلفية تعرف نفسها
car_id = "B15"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Rear Car Connected as {car_id}")
        client.subscribe(f"V2X/cars/{car_id}")
        print(f"📡 Subscribed to V2X/cars/{car_id}")
    else:
        print("❌ Connection failed with code", rc)

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())

        if data.get("type") == "emergency_brake":
            print("\n🚨 EMERGENCY BRAKE WARNING 🚨")
            print("From:", data.get("from"))
            print("-----------------------------")
        else:
            print("📩 Unknown message:", data)

    except Exception as e:
        print("⚠ Error decoding message:", e)

client = mqtt.Client()
client.username_pw_set(username, password)

client.tls_set(cert_reqs=ssl.CERT_NONE)
client.tls_insecure_set(True)

client.on_connect = on_connect
client.on_message = on_message

client.connect(broker, port)
client.loop_forever()