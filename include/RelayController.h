#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>

enum RelayMode {
    SIMPLE_THRESHOLD,      // Simple on/off based on threshold
    TIME_PROPORTIONAL      // Time proportional control
};

class RelayController {
private:
    uint8_t pin;
    bool state;
    bool inverted; // Some relays are active-low
    bool safetyDisabled;
    String name;
    
    unsigned long lastToggleTime;
    static const unsigned long MIN_TOGGLE_INTERVAL = 5000; // Minimum 5 seconds between toggles
    
    // Time proportional control
    RelayMode mode;
    float currentDutyCycle;
    unsigned long windowStartTime;
    unsigned long windowSize; // Time window in milliseconds
    static const unsigned long DEFAULT_WINDOW_SIZE = 10000; // 10 seconds default
    
    void updateTimeProportional();

public:
    RelayController(uint8_t relayPin, String relayName, bool invertLogic = false);
    
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
    
    // For PID output (0-100%)
    void setDutyCycle(float percentage);
    
    // Must be called regularly in loop for time proportional control
    void update();
    
    // Get current duty cycle
    float getDutyCycle() { return currentDutyCycle; }
};

#endif
