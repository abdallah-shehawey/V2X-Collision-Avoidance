#ifndef WIFI_MQTT_CONFIG_H
#define WIFI_MQTT_CONFIG_H

// ==========================================
// 1. WiFi Configuration
// ==========================================
constexpr const char* WIFI_SSID     = "YOUR_WIFI_SSID";
constexpr const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ==========================================
// 2. MQTT (HiveMQ Cloud) Configuration
// ==========================================
constexpr const char* MQTT_BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"; 
constexpr int   MQTT_PORT     = 8883; // Secure MQTT TLS Port

constexpr const char* MQTT_USERNAME  = "v2n_admin";
constexpr const char* MQTT_PASSWORD  = "V2n@2026!";
constexpr const char* MQTT_CLIENT_ID = "ESP32_Smart_Traffic_Light"; 

// ==========================================
// 3. MQTT Topics
// ==========================================
// Topic to subscribe for incoming command events (from Gateway)
constexpr const char* TOPIC_SUBSCRIBE_EMERGENCY = "V2X/zone1/traffic/processed";

// Topic to publish traffic light state telemetry
constexpr const char* TOPIC_PUBLISH_STATE       = "v2n/traffic/light/state";

// ==========================================
// 4. TLS Certificate Configuration
// ==========================================
// Production systems MUST use a valid CA Certificate to verify the server identity
// and prevent Man-In-The-Middle (MITM) attacks.
// For HiveMQ Cloud, you must provide the "ISRG Root X1" certificate in PEM format.
// Replace `nullptr` with the actual certificate string when going to production.
constexpr const char* ROOT_CA_CERT = nullptr; 

#endif // WIFI_MQTT_CONFIG_H
