#include "RelayController.h"

RelayController::RelayController(uint8_t relayPin, String relayName, bool invertLogic)
    : pin(relayPin), state(false), inverted(invertLogic), 
      safetyDisabled(false), name(relayName), lastToggleTime(0),
      mode(SIMPLE_THRESHOLD), currentDutyCycle(0), timer(nullptr), windowStartTime(0),
      windowSize(DEFAULT_WINDOW_SIZE), minOnTime(DEFAULT_MIN_ON_TIME),
      minOffTime(DEFAULT_MIN_OFF_TIME) {
    
    // Create hardware timer for time-proportional control
    String timerName = "Relay_" + name;
    timer = new ESP32_Timer(timerName.c_str());
}

RelayController::~RelayController() {
    if (timer) {
        timer->stop();
        delete timer;
    }
}

void RelayController::begin() {
    pinMode(pin, OUTPUT);
    off(); // Start in safe state
    windowStartTime = ESP32_Timer::getTimestamp();
    
    // Initialize hardware timer for time-proportional mode
    if (timer) {
        timer->begin(timerCallback, this, true); // Periodic timer
        Serial.printf("Relay '%s' hardware timer initialized\n", name.c_str());
    }
    
    Serial.printf("Relay '%s' initialized on pin %d (mode: %s)\n", 
                  name.c_str(), pin, 
                  mode == TIME_PROPORTIONAL ? "Time Proportional (HW Timer)" : "Simple Threshold");
}

void RelayController::on() {
    if (safetyDisabled) {
        Serial.printf("Relay '%s' is safety disabled, cannot turn on\n", name.c_str());
        return;
    }
    
    uint64_t now = ESP32_Timer::getTimestamp();
    if (now - lastToggleTime < MIN_TOGGLE_INTERVAL && state == true) {
        return; // Too soon to toggle again
    }
    
    state = true;
    digitalWrite(pin, inverted ? LOW : HIGH);
    lastToggleTime = now;
    Serial.printf("Relay '%s' turned ON\n", name.c_str());
}

void RelayController::off() {
    uint64_t now = ESP32_Timer::getTimestamp();
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
    windowStartTime = ESP32_Timer::getTimestamp();
    
    // Start/stop hardware timer based on mode
    if (mode == TIME_PROPORTIONAL && timer) {
        // Start timer with 100ms update interval for smooth control
        timer->startMs(100);
        Serial.printf("Relay '%s' hardware timer started\n", name.c_str());
    } else if (timer && timer->isRunning()) {
        timer->stop();
        Serial.printf("Relay '%s' hardware timer stopped\n", name.c_str());
    }
    
    Serial.printf("Relay '%s' mode set to %s\n", 
                  name.c_str(), 
                  mode == TIME_PROPORTIONAL ? "Time Proportional (HW Timer)" : "Simple Threshold");
}

void RelayController::setWindowSize(unsigned long sizeMs) {
    windowSize = sizeMs * 1000ULL; // Convert to microseconds
    windowStartTime = ESP32_Timer::getTimestamp();
    Serial.printf("Relay '%s' window size set to %lu ms\n", name.c_str(), sizeMs);
}

void RelayController::setMinOnTime(unsigned long minMs) {
    minOnTime = minMs * 1000ULL; // Convert to microseconds
    Serial.printf("Relay '%s' minimum on-time set to %lu ms\n", name.c_str(), minMs);
}

void RelayController::setMinOffTime(unsigned long minMs) {
    minOffTime = minMs * 1000ULL; // Convert to microseconds
    Serial.printf("Relay '%s' minimum off-time set to %lu ms\n", name.c_str(), minMs);
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

// Hardware timer callback (static)
void RelayController::timerCallback(void* arg) {
    RelayController* relay = static_cast<RelayController*>(arg);
    if (relay && relay->mode == TIME_PROPORTIONAL) {
        relay->updateTimeProportional();
    }
}

void RelayController::updateTimeProportional() {
    if (safetyDisabled) {
        off();
        return;
    }
    
    uint64_t now = ESP32_Timer::getTimestamp();
    uint64_t elapsed = now - windowStartTime;
    
    // Reset window if expired
    if (elapsed >= windowSize) {
        windowStartTime = now;
        elapsed = 0;
    }
    
    // Calculate on-time based on duty cycle
    uint64_t onTime = (uint64_t)((currentDutyCycle / 100.0) * windowSize);
    uint64_t offTime = windowSize - onTime;
    
    // Enforce minimum on/off times to protect hardware
    // If calculated on-time is too short, skip this cycle
    if (onTime > 0 && onTime < minOnTime) {
        // On-time too short, stay off entire window
        if (state) {
            off();
        }
        return;
    }
    
    // If calculated off-time is too short, stay on entire window
    if (offTime > 0 && offTime < minOffTime) {
        // Off-time too short, stay on entire window
        if (!state) {
            on();
        }
        return;
    }
    
    // Normal operation: determine if relay should be on or off in this window
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
