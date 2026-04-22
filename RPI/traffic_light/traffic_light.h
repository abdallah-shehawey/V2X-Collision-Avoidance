#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include <Arduino.h>

class TrafficLight {
public:
    // Define the states of the traffic light
    enum State {
        RED_STATE,
        GREEN_STATE,
        YELLOW_STATE
    };

    // Constructor to assign pins
    TrafficLight(uint8_t redPin, uint8_t yellowPin, uint8_t greenPin);
    
    // Initialize pins and initial state
    void begin();
    
    // Non-blocking state machine logic (call in main loop)
    void loop();

private:
    uint8_t _redPin;
    uint8_t _yellowPin;
    uint8_t _greenPin;
    
    State _currentState;
    unsigned long _lastStateChangeTime;
    
    // Helper to control LEDs
    void setLeds(bool red, bool yellow, bool green);
    
    // Handle transition actions and timer reset
    void changeState(State newState);
};

#endif // TRAFFIC_LIGHT_H
