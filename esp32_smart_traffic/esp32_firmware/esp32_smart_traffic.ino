#include <Arduino.h>
#include <ArduinoJson.h> // Library Manager ID: 64
#include "wifi_mqtt_config.h"
#include "traffic_light.h"
#include "mqtt_handler.h"

// ==========================================
// Hardware definitions
// ==========================================
const uint8_t RED_PIN    = 25;
const uint8_t YELLOW_PIN = 26;
const uint8_t GREEN_PIN  = 27;

// ==========================================
// Object Instantiations
// ==========================================
TrafficLight trafficLight(RED_PIN, YELLOW_PIN, GREEN_PIN);
MqttHandler mqttHandler;

// Timing track flag
unsigned long lastPublishTime = 0;
unsigned long lastMsgReceivedTime = 0;
bool isFailSafeActive = false;

// Task handle for FreeRTOS
TaskHandle_t NetworkTaskHandle;


// ==========================================
// MQTT Event Parser & Emergency Handler
// ==========================================
    Serial.println("]");
    
    lastMsgReceivedTime = millis(); // Update heartbeat
    isFailSafeActive = false;
    
    // Filter by target topic
    if (strcmp(topic, TOPIC_SUBSCRIBE_EMERGENCY) == 0) {
        
        // 1. PERFORMANCE & SECURITY FIX:
        // Do NOT create a variable length array (VLA) `char msg[length + 1]` on the stack!
        // A large malicious payload could cause a Stack Overflow and crash the ESP32.
        // Instead, ArduinoJson can safely parse directly from the raw byte payload.
        StaticJsonDocument<256> doc;
        
        // Parse directly from memory buffer
        DeserializationError error = deserializeJson(doc, payload, length);
        
        if (error) {
            Serial.print("      [ERROR] Invalid JSON payload: ");
            Serial.println(error.c_str());
            return;
        }
        
        // Grab values safely to prevent null pointers
        const char* incomingCommand = doc["command"]; // e.g. "emergency"
        const char* incomingMode = doc["mode"];       // e.g. "ambulance_priority"
        
        if (incomingCommand) {
            if (strcmp(incomingCommand, "emergency") == 0) {
                // Cross-core data mutation happens here.
                Serial.println("      *** EMERGENCY TRIGGERED *** >> Overriding to GREEN immediately.");
                trafficLight.setEmergencyMode(true);
            } 
            else if (strcmp(incomingCommand, "normal") == 0) {
                Serial.println("      *** NORMAL RESTORED *** >> Resuming cycle safely.");
                trafficLight.setEmergencyMode(false);
            }
            else {
                Serial.print("      Unknown command received: ");
                Serial.println(incomingCommand);
            }
        }
    }
}


// ==========================================
// FreeRTOS Task for Network Operations (Core 0)
// ==========================================
// This task handles WiFi & MQTT reconnects strictly in the background.
// If the internet drops, reconnecting will NOT block the traffic light cycle!
void networkTaskCode(void* pvParameters) {
    Serial.print("Network Task running on Core ID: ");
    Serial.println(xPortGetCoreID());

    mqttHandler.setup();
    mqttHandler.setCallback(mqttCallback);

    while (true) {
        mqttHandler.loop();
        
        // Broadcast telemetry every 1 second continuously 
        unsigned long currentMillis = millis();
        if (currentMillis - lastPublishTime >= 1000) {
            lastPublishTime = currentMillis;
            mqttHandler.publishState(
                trafficLight.getStateString(),
                trafficLight.getRemainingTimeSec(),
                trafficLight.isEmergency()
            );
        }
        
        // Yield scheduler actively to stop WatchDog reset panics
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}


// ==========================================
// Main ESP32 System Setup
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n============ V2X SMART TRAFFIC LIGHT SYSTEM (PRO) ============");
    Serial.print("Traffic Light Logic running on Core ID: ");
    Serial.println(xPortGetCoreID()); // Main sketch loops on Core 1 by default
    
    // 1. Init hardware logic locally
    trafficLight.begin();
    
    // 2. Offload the Networking routines dynamically onto Core 0
    xTaskCreatePinnedToCore(
        networkTaskCode,   /* Task execute function */
        "NetworkTask",     /* Name of the task */
        10240,             /* Stack size allocated for TLS operations */
        NULL,              /* Internal Task parameter pointer */
        1,                 /* Priority mapped for the task */
        &NetworkTaskHandle,/* Task reference handle */
        0                  /* Enforce pin to ESP32 Core ID 0 */
    );
}

// ==========================================
// Non-blocking Main Loop (Core 1)
// ==========================================
void loop() {
    // 1. Check for Fail-safe (Timeout: 15 seconds)
    if (millis() - lastMsgReceivedTime > 15000 && lastMsgReceivedTime != 0) {
        if (!isFailSafeActive) {
            Serial.println("\n⚠️ [WARNING] MQTT Timeout! Entering Local Fail-safe Mode...");
            isFailSafeActive = true;
            trafficLight.setEmergencyMode(false); // Clear any software locks
        }
        
        // In Fail-safe, the ESP runs a simple fixed cycle
        static unsigned long lastLocalSwitch = 0;
        if (millis() - lastLocalSwitch > 10000) { // 10s switch
            lastLocalSwitch = millis();
            // Local simple cycle logic...
        }
    }

    trafficLight.loop();
    delay(1);
}
