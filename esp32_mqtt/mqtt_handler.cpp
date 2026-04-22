#include "mqtt_handler.h"
#include "wifi_mqtt_config.h"

MqttHandler::MqttHandler() : lastReconnectAttempt(0) {
    // Inject the underlying secure WiFi client into the PubSub (MQTT) client
    mqttClient.setClient(secureClient);
}

void MqttHandler::setup() {
    connectWiFi();
    
    // Setup TLS certificate
    if (ROOT_CA_CERT != nullptr) {
        secureClient.setCACert(ROOT_CA_CERT);
    } else {
        // Fallback: This bypasses CA verification but maintains encrypted link
        Serial.println("WARNING: CA Certificate is NULL. Using insecure TLS connection (not recommended for production).");
        secureClient.setInsecure();
    }
    
    // Configure settings for broker connection
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    
    // You can optionally increase the Keep-Alive value here (PubSub defaults to 15s)
    mqttClient.setKeepAlive(60); 
}

void MqttHandler::connectWiFi() {
    Serial.print("\nConnecting to WiFi SSID: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait until connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi connected successfully.");
    Serial.print("Local IP address: ");
    Serial.println(WiFi.localIP());
}

void MqttHandler::connectMQTT() {
    Serial.print("\nAttempting to connect to MQTT broker: ");
    Serial.println(MQTT_BROKER);
    
    // Attempt connection
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("MQTT connected successfully!");
        
        // Automatically publish an 'Online' statement out upon connecting
        mqttClient.publish("esp32/status", "online");
        
        // Subscribe to a generic command topic upon connection
        mqttClient.subscribe("esp32/commands");
        Serial.println("Subscribed to topic: esp32/commands");
        
    } else {
        Serial.print("MQTT connection failed, state: ");
        Serial.print(mqttClient.state());
        Serial.println(" - retrying in 5 seconds...");
    }
}

void MqttHandler::loop() {
    // 1. Maintain WiFi Connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Attempting to reconnect...");
        connectWiFi();
    }

    // 2. Maintain MQTT Connection
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        
        // Non-blocking reconnect loop - attempts a reconnect every 5 seconds
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            connectMQTT();
            // Reset the reconnect timer after an attempt to prevent rapid fire blocked retries
            lastReconnectAttempt = millis();
        }
    } else {
        // 3. Service the MQTT background processes and yield incoming messages
        mqttClient.loop();
    }
}

bool MqttHandler::publish(const char* topic, const char* payload) {
    if (mqttClient.connected()) {
        return mqttClient.publish(topic, payload);
    }
    return false;
}

void MqttHandler::setCallback(MQTT_CALLBACK_SIGNATURE) {
    mqttClient.setCallback(callback);
}
