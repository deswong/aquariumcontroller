#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "ESP32_Timer.h"

enum RelayMode {
    SIMPLE_THRESHOLD,      // Simple on/off based on threshold
    TIME_PROPORTIONAL      // Time proportional control with hardware timer
};

class RelayController {
private:
    uint8_t pin;
    bool state;
    bool inverted; // Some relays are active-low
    bool safetyDisabled;
    String name;
    
    uint64_t lastToggleTime;  // Changed to uint64_t for microsecond precision
    static const uint64_t MIN_TOGGLE_INTERVAL = 5000000; // Minimum 5 seconds in microseconds
    
    // Time proportional control with hardware timer
    RelayMode mode;
    float currentDutyCycle;
    ESP32_Timer* timer;        // Hardware timer for precise control
    uint64_t windowStartTime;  // Changed to uint64_t for microsecond precision
    uint64_t windowSize;       // Time window in microseconds
    uint64_t minOnTime;        // Minimum on-time in microseconds
    uint64_t minOffTime;       // Minimum off-time in microseconds
    static const uint64_t DEFAULT_WINDOW_SIZE = 10000000; // 10 seconds default in microseconds
    static const uint64_t DEFAULT_MIN_ON_TIME = 10000000;  // 10 seconds minimum on in microseconds
    static const uint64_t DEFAULT_MIN_OFF_TIME = 10000000; // 10 seconds minimum off in microseconds
    
    void updateTimeProportional();
    static void timerCallback(void* arg);

public:
    RelayController(uint8_t relayPin, String relayName, bool invertLogic = false);
    ~RelayController();
    
    void begin();
    void on();
    void off();
    void setState(bool newState);
    bool getState() { return state; }
    
    // Safety
    void safetyDisable();
    void safetyEnable();
    bool isSafetyDisabled() { return safetyDisabled; }
    
    // Control modes
    void setMode(RelayMode newMode);
    RelayMode getMode() { return mode; }
    void setWindowSize(unsigned long sizeMs);
    void setMinOnTime(unsigned long minMs);   // Set minimum on-time
    void setMinOffTime(unsigned long minMs);  // Set minimum off-time
    
    // For PID output (0-100%)
    void setDutyCycle(float percentage);
    
    // Must be called regularly in loop for time proportional control
    void update();
    
    // Get current duty cycle
    float getDutyCycle() { return currentDutyCycle; }
};

#endif
