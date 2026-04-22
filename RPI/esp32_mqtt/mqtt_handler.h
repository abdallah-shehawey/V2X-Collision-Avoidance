#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h> // Requires 'PubSubClient' library by Nick O'Leary

class MqttHandler {
public:
    MqttHandler();
    
    // Initializes WiFi connection, TLS certificate, and MQTT broker setup
    void setup();
    
    // Maintains WiFi and MQTT connection states (with auto-reconnect), & processes incoming MQTT messages
    void loop();
    
    // Helper function to safely publish a message to a topic
    bool publish(const char* topic, const char* payload);
    
    // Allows main application to set a custom callback for subscribed incoming MQTT messages
    void setCallback(MQTT_CALLBACK_SIGNATURE);

private:
    void connectWiFi();
    void connectMQTT();
    
    WiFiClientSecure secureClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt;
};

#endif // MQTT_HANDLER_H
