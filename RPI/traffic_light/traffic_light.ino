#include "traffic_light.h"

// Define the GPIO pins for the ESP32
const uint8_t RED_LED_PIN = 25;
const uint8_t YELLOW_LED_PIN = 26;
const uint8_t GREEN_LED_PIN = 27;

// Instantiate the traffic light object
TrafficLight myTrafficLight(RED_LED_PIN, YELLOW_LED_PIN, GREEN_LED_PIN);

void setup() {
    Serial.begin(115200);
    Serial.println("Traffic Light System Starting...");
    
    // Initialize the traffic light
    myTrafficLight.begin();
}

void loop() {
    // Keep the state machine running smoothly every loop
    myTrafficLight.loop();
    
    // You can do other concurrent tasks here without being blocked!
    // For example:
    // readButton();
    // updateDisplay();
}
