#include <Arduino.h>
#include "traffic_light.h"

TrafficLight::TrafficLight(uint8_t redPin, uint8_t yellowPin, uint8_t greenPin)
    : _redPin(redPin), _yellowPin(yellowPin), _greenPin(greenPin),
      _currentState(RED_STATE), _lastStateChangeTime(0),
      _currentStateDuration(5000), _isEmergency(false) {}

void TrafficLight::begin() {
    pinMode(_redPin, OUTPUT);
    pinMode(_yellowPin, OUTPUT);
    pinMode(_greenPin, OUTPUT);
    
    // Start with RED state
    changeState(RED_STATE);
}

void TrafficLight::setEmergencyMode(bool emergency) {
    if (_isEmergency == emergency) return; // Prevent duplicate execution
    
    if (emergency) {
        _isEmergency = true;
        // If we are RED, we must go YELLOW for 3 seconds first
        if (_currentState == RED_STATE) {
            Serial.println("      [LOG] Currently RED. Transitioning to YELLOW for 3s before GREEN.");
            _currentState = YELLOW_STATE;
            _currentStateDuration = 3000;
            _lastStateChangeTime = millis();
            setLeds(false, true, false);
        } 
        // If we are already GREEN, stay GREEN
        else if (_currentState == GREEN_STATE) {
            Serial.println("      [LOG] Already GREEN. Staying GREEN.");
            setLeds(false, false, true);
        }
    } else {
        // When emergency ends, go back to normal RED to reset the cycle safely
        Serial.println("      [LOG] Emergency cleared. Returning to RED.");
        changeState(RED_STATE);
        _isEmergency = false;
    }
}

void TrafficLight::changeState(State newState) {
    _currentState = newState;
    _lastStateChangeTime = millis();
    
    switch(_currentState) {
        case RED_STATE:
            _currentStateDuration = 5000;
            setLeds(true, false, false);
            break;
        case GREEN_STATE:
            _currentStateDuration = 5000;
            setLeds(false, false, true);
            break;
        case YELLOW_STATE:
            _currentStateDuration = 2000;
            setLeds(false, true, false);
            break;
    }
}

void TrafficLight::setLeds(bool red, bool yellow, bool green) {
    digitalWrite(_redPin, red ? HIGH : LOW);
    digitalWrite(_yellowPin, yellow ? HIGH : LOW);
    digitalWrite(_greenPin, green ? HIGH : LOW);
}

void TrafficLight::loop() {
    // If in emergency and we are in the 3-second YELLOW transition
    if (_isEmergency) {
        if (_currentState == YELLOW_STATE) {
            unsigned long elapsed = millis() - _lastStateChangeTime;
            if (elapsed >= _currentStateDuration) {
                Serial.println("      [LOG] Yellow transition complete. Switching to GREEN.");
                _currentState = GREEN_STATE;
                setLeds(false, false, true);
            }
        }
        return;
    }

    // Normal simulation-like behavior can be re-enabled here if needed, 
    // but per requirements, it follows commands.
}

// ---------------------------------------------------------
// Telemetry Getters
// ---------------------------------------------------------
const char* TrafficLight::getStateString() const {
    if (_currentState == RED_STATE) return "RED";
    if (_currentState == GREEN_STATE) return "GREEN";
    if (_currentState == YELLOW_STATE) return "YELLOW";
    return "UNKNOWN";
}

int TrafficLight::getRemainingTimeSec() const {
    if (_isEmergency) return 0; // Infinite effectively during active emergency
    
    unsigned long elapsed = millis() - _lastStateChangeTime;
    if (elapsed >= _currentStateDuration) return 0;
    
    return (_currentStateDuration - elapsed) / 1000;
}

bool TrafficLight::isEmergency() const {
    return _isEmergency;
}
