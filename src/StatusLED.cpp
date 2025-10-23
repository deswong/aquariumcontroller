#include "StatusLED.h"
#include "Logger.h"

StatusLED::StatusLED(int pin)
    : ledPin(pin), 
      currentState(STATE_INITIALIZING),
      previousState(STATE_INITIALIZING),
      ledState(false),
      lastUpdate(0),
      blinkOnTime(500),
      blinkOffTime(500),
      brightness(0),
      breathingUp(true) {
}

void StatusLED::begin() {
    if (ledPin >= 0) {
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, LOW);
        LOG_INFO("StatusLED", "Initialized on pin %d", ledPin);
    } else {
        LOG_WARN("StatusLED", "No LED pin configured");
    }
}

void StatusLED::update() {
    if (ledPin < 0) return;
    
    unsigned long now = millis();
    
    // Apply state-specific pattern if state changed
    if (currentState != previousState) {
        previousState = currentState;
        applyState();
    }
    
    updatePattern();
}

void StatusLED::setState(SystemState state) {
    if (currentState != state) {
        LOG_DEBUG("StatusLED", "State change: %d -> %d", currentState, state);
        currentState = state;
    }
}

void StatusLED::applyState() {
    switch (currentState) {
        case STATE_INITIALIZING:
            // Fast blink during initialization
            setFastBlink();
            break;
            
        case STATE_NORMAL:
            // Solid on
            setOn();
            break;
            
        case STATE_WARNING:
            // Slow blink
            setSlowBlink();
            break;
            
        case STATE_ERROR:
            // Fast blink
            setFastBlink();
            break;
            
        case STATE_CRITICAL:
            // Very fast blink (SOS pattern could be added)
            setBlinkPattern(100, 100);
            break;
            
        case STATE_AP_MODE:
            // Breathing pulse
            setPulse();
            break;
            
        case STATE_UPDATING:
            // Double blink pattern
            // This could be enhanced with a more complex pattern
            setBlinkPattern(200, 200);
            break;
    }
}

void StatusLED::updatePattern() {
    if (ledPin < 0) return;
    
    unsigned long now = millis();
    
    if (currentState == STATE_AP_MODE) {
        // Breathing effect
        if (now - lastUpdate > 10) {
            lastUpdate = now;
            
            if (breathingUp) {
                brightness += 5;
                if (brightness >= 255) {
                    brightness = 255;
                    breathingUp = false;
                }
            } else {
                if (brightness >= 5) {
                    brightness -= 5;
                } else {
                    brightness = 0;
                    breathingUp = true;
                }
            }
            
            analogWrite(ledPin, brightness);
        }
    } else if (currentState == STATE_NORMAL) {
        // Solid on
        if (!ledState) {
            ledState = true;
            digitalWrite(ledPin, HIGH);
        }
    } else {
        // Blinking pattern
        unsigned long interval = ledState ? blinkOnTime : blinkOffTime;
        
        if (now - lastUpdate > interval) {
            lastUpdate = now;
            ledState = !ledState;
            digitalWrite(ledPin, ledState ? HIGH : LOW);
        }
    }
}

void StatusLED::setOn() {
    if (ledPin < 0) return;
    digitalWrite(ledPin, HIGH);
    ledState = true;
}

void StatusLED::setOff() {
    if (ledPin < 0) return;
    digitalWrite(ledPin, LOW);
    ledState = false;
}

void StatusLED::setBrightness(uint8_t b) {
    if (ledPin < 0) return;
    brightness = b;
    analogWrite(ledPin, brightness);
}

void StatusLED::setBlinkPattern(unsigned long onTime, unsigned long offTime) {
    blinkOnTime = onTime;
    blinkOffTime = offTime;
    lastUpdate = millis();
}

void StatusLED::setFastBlink() {
    setBlinkPattern(100, 100);
}

void StatusLED::setSlowBlink() {
    setBlinkPattern(500, 500);
}

void StatusLED::setPulse() {
    brightness = 0;
    breathingUp = true;
    lastUpdate = millis();
}
