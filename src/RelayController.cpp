#include "RelayController.h"

RelayController::RelayController(uint8_t relayPin, String relayName, bool invertLogic)
    : pin(relayPin), state(false), inverted(invertLogic), 
      safetyDisabled(false), name(relayName), lastToggleTime(0),
      mode(SIMPLE_THRESHOLD), currentDutyCycle(0), windowStartTime(0),
      windowSize(DEFAULT_WINDOW_SIZE) {
}

void RelayController::begin() {
    pinMode(pin, OUTPUT);
    off(); // Start in safe state
    windowStartTime = millis();
    Serial.printf("Relay '%s' initialized on pin %d (mode: %s)\n", 
                  name.c_str(), pin, 
                  mode == TIME_PROPORTIONAL ? "Time Proportional" : "Simple Threshold");
}

void RelayController::on() {
    if (safetyDisabled) {
        Serial.printf("Relay '%s' is safety disabled, cannot turn on\n", name.c_str());
        return;
    }
    
    unsigned long now = millis();
    if (now - lastToggleTime < MIN_TOGGLE_INTERVAL && state == true) {
        return; // Too soon to toggle again
    }
    
    state = true;
    digitalWrite(pin, inverted ? LOW : HIGH);
    lastToggleTime = now;
    Serial.printf("Relay '%s' turned ON\n", name.c_str());
}

void RelayController::off() {
    unsigned long now = millis();
    if (now - lastToggleTime < MIN_TOGGLE_INTERVAL && state == false) {
        return; // Too soon to toggle again
    }
    
    state = false;
    digitalWrite(pin, inverted ? HIGH : LOW);
    lastToggleTime = now;
    Serial.printf("Relay '%s' turned OFF\n", name.c_str());
}

void RelayController::setState(bool newState) {
    if (newState) {
        on();
    } else {
        off();
    }
}

void RelayController::safetyDisable() {
    safetyDisabled = true;
    off(); // Turn off immediately
    Serial.printf("SAFETY: Relay '%s' disabled\n", name.c_str());
}

void RelayController::safetyEnable() {
    safetyDisabled = false;
    Serial.printf("SAFETY: Relay '%s' enabled\n", name.c_str());
}

void RelayController::setMode(RelayMode newMode) {
    mode = newMode;
    windowStartTime = millis();
    Serial.printf("Relay '%s' mode set to %s\n", 
                  name.c_str(), 
                  mode == TIME_PROPORTIONAL ? "Time Proportional" : "Simple Threshold");
}

void RelayController::setWindowSize(unsigned long sizeMs) {
    windowSize = sizeMs;
    windowStartTime = millis();
    Serial.printf("Relay '%s' window size set to %lu ms\n", name.c_str(), windowSize);
}

void RelayController::setDutyCycle(float percentage) {
    percentage = constrain(percentage, 0, 100);
    currentDutyCycle = percentage;
    
    if (mode == SIMPLE_THRESHOLD) {
        // Simple on/off control based on threshold (50%)
        if (percentage > 50) {
            on();
        } else {
            off();
        }
    }
    // For TIME_PROPORTIONAL mode, update() handles the switching
}

void RelayController::updateTimeProportional() {
    if (safetyDisabled) {
        off();
        return;
    }
    
    unsigned long now = millis();
    unsigned long elapsed = now - windowStartTime;
    
    // Reset window if expired
    if (elapsed >= windowSize) {
        windowStartTime = now;
        elapsed = 0;
    }
    
    // Calculate on-time based on duty cycle
    unsigned long onTime = (unsigned long)((currentDutyCycle / 100.0) * windowSize);
    
    // Determine if relay should be on or off in this window
    if (elapsed < onTime) {
        // Within the "on" portion of the window
        if (!state) {
            on();
        }
    } else {
        // Within the "off" portion of the window
        if (state) {
            off();
        }
    }
}

void RelayController::update() {
    if (mode == TIME_PROPORTIONAL) {
        updateTimeProportional();
    }
    // SIMPLE_THRESHOLD mode doesn't need updates (controlled by setDutyCycle)
}
