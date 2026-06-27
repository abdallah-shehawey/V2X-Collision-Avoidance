#include "traffic_light.h"

TrafficLight::TrafficLight(uint8_t redPin, uint8_t yellowPin, uint8_t greenPin) {
    _redPin = redPin;
    _yellowPin = yellowPin;
    _greenPin = greenPin;
    
    _currentState = RED_STATE;
    _lastStateChangeTime = 0;
}

void TrafficLight::begin() {
    // Configure pins
    pinMode(_redPin, OUTPUT);
    pinMode(_yellowPin, OUTPUT);
    pinMode(_greenPin, OUTPUT);
    
    // Start with RED state by default
    changeState(RED_STATE);
}

void TrafficLight::setLeds(bool red, bool yellow, bool green) {
    digitalWrite(_redPin, red ? HIGH : LOW);
    digitalWrite(_yellowPin, yellow ? HIGH : LOW);
    digitalWrite(_greenPin, green ? HIGH : LOW);
}

void TrafficLight::changeState(State newState) {
    _currentState = newState;
    _lastStateChangeTime = millis(); // Reset timer upon change
    
    // Take immediate action for the new state
    switch (_currentState) {
        case RED_STATE:
            setLeds(true, false, false);
            break;
        case GREEN_STATE:
            setLeds(false, false, true);
            break;
        case YELLOW_STATE:
            setLeds(false, true, false);
            break;
    }
}

void TrafficLight::loop() {
    unsigned long currentMillis = millis();
    
    // Check if time elapsed for current state
    switch (_currentState) {
        case RED_STATE:
            // RED lasts 5 seconds
            if (currentMillis - _lastStateChangeTime >= 5000) {
                changeState(GREEN_STATE); // Transition to Green
            }
            break;
            
        case GREEN_STATE:
            // GREEN lasts 5 seconds
            if (currentMillis - _lastStateChangeTime >= 5000) {
                changeState(YELLOW_STATE); // Transition to Yellow
            }
            break;
            
        case YELLOW_STATE:
            // YELLOW lasts 2 seconds
            if (currentMillis - _lastStateChangeTime >= 2000) {
                changeState(RED_STATE); // Transition back to Red
            }
            break;
    }
}
