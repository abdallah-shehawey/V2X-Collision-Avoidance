import paho.mqtt.client as mqtt
import ssl
import json

broker = "1470a790202943d6b60bda272c6760b8.s1.eu.hivemq.cloud"
port = 8883
username = "v2n_admin"
password = "V2n@2026!"

vehicle_id = "A12"

def on_connect(client, userdata, flags, rc):
    print("Front Car Connected")
    client.subscribe("V2X/internal/target_id")

def on_message(client, userdata, msg):
    target_id = msg.payload.decode()
    print("🎯 Target received:", target_id)

    message = {
        "from": vehicle_id,
        "type": "emergency_brake"
    }

    client.publish(f"V2X/cars/{target_id}", json.dumps(message))
    print("🚨 Warning sent to", target_id)

client = mqtt.Client()
client.username_pw_set(username, password)
client.tls_set(cert_reqs=ssl.CERT_NONE)
client.tls_insecure_set(True)

client.on_connect = on_connect
client.on_message = on_message

client.connect(broker, port)
client.loop_forever()