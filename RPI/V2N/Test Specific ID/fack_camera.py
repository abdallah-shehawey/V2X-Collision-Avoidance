import paho.mqtt.client as mqtt
import ssl
import time

broker = "1470a790202943d6b60bda272c6760b8.s1.eu.hivemq.cloud"
port = 8883
username = "v2n_admin"
password = "V2n@2026!"

client = mqtt.Client()
client.username_pw_set(username, password)
client.tls_set(cert_reqs=ssl.CERT_NONE)
client.tls_insecure_set(True)

client.connect(broker, port)

while True:
    detected_id = "B15"  # نحاكي إن الكاميرا شافت العربية دي
    client.publish("V2X/internal/target_id", detected_id)
    print("📷 Fake Camera sent target:", detected_id)
    time.sleep(10)  # كل 5 ثواني