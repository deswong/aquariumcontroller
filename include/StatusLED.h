#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>

// System states
enum SystemState {
    STATE_INITIALIZING,
    STATE_NORMAL,
    STATE_WARNING,
    STATE_ERROR,
    STATE_CRITICAL,
    STATE_AP_MODE,
    STATE_UPDATING
};

class StatusLED {
public:
    StatusLED(int pin = -1);
    
    void begin();
    void update();
    void setState(SystemState state);
    SystemState getState() const { return currentState; }
    
    // Manual LED control
    void setOn();
    void setOff();
    void setBrightness(uint8_t brightness);
    
    // Blink patterns
    void setBlinkPattern(unsigned long onTime, unsigned long offTime);
    void setFastBlink();   // 100ms on, 100ms off
    void setSlowBlink();   // 500ms on, 500ms off
    void setPulse();       // Breathing effect
    
private:
    int ledPin;
    SystemState currentState;
    SystemState previousState;
    
    bool ledState;
    unsigned long lastUpdate;
    unsigned long blinkOnTime;
    unsigned long blinkOffTime;
    
    // PWM for breathing effect
    uint8_t brightness;
    bool breathingUp;
    
    void updatePattern();
    void applyState();
};

#endif // STATUS_LED_H
