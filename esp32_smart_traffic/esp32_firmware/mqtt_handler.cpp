#include "mqtt_handler.h"
#include "wifi_mqtt_config.h"

MqttHandler::MqttHandler() : _lastReconnectAttempt(0) {
    _mqttClient.setClient(_secureClient);
}

void MqttHandler::setup() {
    connectWiFi();
    
    // SECURITY WARNING: Force CA certificate mapping for production environments
    // The fallback 'setInsecure()' opens the device up to Man in The Middle overrides.
    if (ROOT_CA_CERT != nullptr) {
        _secureClient.setCACert(ROOT_CA_CERT);
    } else {
        Serial.println("\n[SECURITY ALERT] ROOT_CA_CERT is missing!");
        Serial.println("Using insecure TLS. This encrypts data but DOES NOT verify server identity.");
        Serial.println("Highly vulnerable to MITM attacks. Please add ISRG Root X1 in production!\n");
        _secureClient.setInsecure(); // Explicit bypass ONLY for active developer iteration.
    }
    
    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setBufferSize(512); 
}

void MqttHandler::connectWiFi() {
    Serial.print("\nConnecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    // Ensure ESP32 is in Station mode and clear any old stuck configs
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Since this runs in a dedicated FreeRTOS wrapper now, this blocking 
    // behavior handles exactly as expected strictly without freezing the LED core.
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected successfully.");
}

void MqttHandler::connectMQTT() {
    Serial.print("Connecting to MQTT: ");
    Serial.println(MQTT_BROKER);
    
    if (_mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("MQTT securely connected!");
        
        // Setup Emergency command listen channel
        _mqttClient.subscribe(TOPIC_SUBSCRIBE_EMERGENCY);
        Serial.print("Subscribed to Event Topic: ");
        Serial.println(TOPIC_SUBSCRIBE_EMERGENCY);
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(_mqttClient.state());
        Serial.println(" -> retrying in 5s");
    }
}

void MqttHandler::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        // 5s Non-blocking reconnection interval for the network layer
        if (now - _lastReconnectAttempt > 5000) {
            _lastReconnectAttempt = now;
            connectMQTT();
            _lastReconnectAttempt = millis(); 
        }
    } else {
        _mqttClient.loop();
    }
}

void MqttHandler::setCallback(MQTT_CALLBACK_SIGNATURE) {
    _mqttClient.setCallback(callback);
}

void MqttHandler::publishState(const char* stateStr, int remainingTime, bool isEmergency) {
    if (!_mqttClient.connected()) return; // Prevent writing to dead sockets

    // Use Stack-allocated lightweight doc structure
    StaticJsonDocument<256> doc;
    doc["state"] = stateStr;
    doc["remaining_time"] = remainingTime;
    doc["is_emergency"] = isEmergency;
    
    char payloadBuffer[256];
    serializeJson(doc, payloadBuffer);
    
    bool result = _mqttClient.publish(TOPIC_PUBLISH_STATE, payloadBuffer);
    
    if (result) {
        Serial.print("✅ الداتا اترفعت بنجاح! (Data Uploaded Successfully): ");
        Serial.println(payloadBuffer);
    } else {
        Serial.println("❌ فشل في رفع الداتا (Publish Error).");
    }
}
