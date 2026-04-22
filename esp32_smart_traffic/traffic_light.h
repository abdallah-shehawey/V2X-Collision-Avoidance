/******************************************************************************
 * @file           : traffic_light.h
 * @author         : Aya Gamal
 * @brief          : Traffic Light Logic Definitions
 * @description    : Class headers and states for managing traffic lights
 ******************************************************************************/
#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include <Arduino.h>

class TrafficLight {
public:
    enum State {
        RED_STATE,
        GREEN_STATE,
        YELLOW_STATE
    };

    TrafficLight(uint8_t redPin, uint8_t yellowPin, uint8_t greenPin);
    
    // Initialize hardware pins
    void begin();
    
    // Handle state transitions without blocking
    void loop();
    
    // Enable or disable override emergency mode (Thread-Safe)
    void setEmergencyMode(bool emergency);
    
    // Getters for publishing telemetry
    const char* getStateString() const;
    int getRemainingTimeSec() const;
    bool isEmergency() const;

private:
    uint8_t _redPin, _yellowPin, _greenPin;
    State _currentState;
    
    unsigned long _lastStateChangeTime;
    unsigned long _currentStateDuration; // Stores dynamic duration of current state
    
    // Marked volatile because it is modified by the FreeRTOS Network Task 
    // on Core 0 and read by the Traffic loop on Core 1
    volatile bool _isEmergency;
    
    void changeState(State newState);
    void setLeds(bool red, bool yellow, bool green);
};

#endif // TRAFFIC_LIGHT_H
