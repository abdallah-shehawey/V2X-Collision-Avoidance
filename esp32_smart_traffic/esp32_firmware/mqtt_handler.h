#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h> // Library ID: 89 - PubSubClient by Nick O'Leary
#include <ArduinoJson.h>  // Library ID: 64 - ArduinoJson by Benoit Blanchon

class MqttHandler {
public:
    MqttHandler();
    
    void setup();
    void loop();
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    
    // Construct and publish status payload via JSON
    void publishState(const char* stateStr, int remainingTime, bool isEmergency);

private:
    void connectWiFi();
    void connectMQTT();
    
    WiFiClientSecure _secureClient;
    PubSubClient _mqttClient;
    unsigned long _lastReconnectAttempt;
};

#endif // MQTT_HANDLER_H
