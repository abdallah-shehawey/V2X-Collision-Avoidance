/******************************************************************************
 * @file           : traffic_light.cpp
 * @author         : Aya Gamal
 * @brief          : Traffic Light State Machine
 * @description    : Implements the state transitions and hardware pin triggers
 ******************************************************************************/
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
    
    // Lock-free safe ordering for dual-core architecture
    if (emergency) {
        // First disable the main loop handler, then forcibly change values
        _isEmergency = true;
        setLeds(false, false, true);
        _currentState = GREEN_STATE;
    } else {
        // Setup new regular path variables first, then release lock back to normal loop
        changeState(GREEN_STATE);
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
    // If in emergency mode, halt execution of normal transitions immediately
    if (_isEmergency) return;
    
    unsigned long elapsed = millis() - _lastStateChangeTime;
    
    // Transition carefully once interval reaches the max threshold
    if (elapsed >= _currentStateDuration) {
        switch(_currentState) {
            case RED_STATE: 
                changeState(GREEN_STATE); 
                break;
            case GREEN_STATE: 
                changeState(YELLOW_STATE); 
                break;
            case YELLOW_STATE: 
                changeState(RED_STATE); 
                break;
        }
    }
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
