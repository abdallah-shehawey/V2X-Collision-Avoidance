#ifndef WIFI_MQTT_CONFIG_H
#define WIFI_MQTT_CONFIG_H

// ==========================================
// WiFi Configuration
// ==========================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ==========================================
// MQTT (HiveMQ Cloud) Configuration
// ==========================================
// Found in your HiveMQ Cloud console (e.g. xyz123.s1.eu.hivemq.cloud)
const char* MQTT_BROKER   = "YOUR_CLUSTER_URL.hivemq.cloud"; 
const int   MQTT_PORT     = 8883; // 8883 is the standard Secure MQTT/TLS port

const char* MQTT_USERNAME  = "YOUR_MQTT_USERNAME";
const char* MQTT_PASSWORD  = "YOUR_MQTT_PASSWORD";
const char* MQTT_CLIENT_ID = "ESP32_Secure_Client_01"; // Change for each device to avoid collisions

// ==========================================
// TLS Certificate Configuration
// ==========================================
// HiveMQ Cloud server certificate is signed by Let's Encrypt (ISRG Root X1).
// For production, paste the Let's Encrypt ISRG Root X1 certificate in PEM format below.
// If set to nullptr, the code will fallback to unverified TLS (encryption but no identity check) via setInsecure().
const char* ROOT_CA_CERT = nullptr; 

/* Example format for pasting the ROOT_CA_CERT:
const char* ROOT_CA_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRnXxt7qDs/EwDQYJKoZIhvcNAQELBQAw\n" \
"...(rest of ISRG Root X1 cert)...\n" \
"-----END CERTIFICATE-----\n";
*/

#endif // WIFI_MQTT_CONFIG_H
