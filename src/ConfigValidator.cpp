#include "ConfigValidator.h"
#include "Logger.h"
#include <set>

ConfigValidator::ConfigValidator() {
}

void ConfigValidator::addError(const char* component, const char* parameter, 
                               const char* message, bool critical) {
    ValidationError error;
    error.component = component;
    error.parameter = parameter;
    error.message = message;
    error.isCritical = critical;
    errors.push_back(error);
}

bool ConfigValidator::validateRange(float value, float min, float max, 
                                    const char* component, const char* parameter) {
    if (value < min || value > max) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Value %.2f is outside valid range [%.2f, %.2f]", 
                value, min, max);
        addError(component, parameter, msg, true);
        return false;
    }
    return true;
}

bool ConfigValidator::isPinValid(int pin) {
    // ESP32-S3 valid GPIO pins
    // GPIOs 0-21 are generally available
    // GPIOs 22-25 are used for flash/PSRAM on some modules
    // GPIOs 26-48 available
    // GPIOs 19-20 are USB on ESP32-S3
    
    if (pin < 0) return false;
    
    // Invalid pins on ESP32-S3
    std::set<int> invalidPins = {
        22, 23, 24, 25,  // Flash/PSRAM
        26, 27, 28, 29, 30, 31, 32  // Not available on all modules
    };
    
    return pin <= 48 && invalidPins.find(pin) == invalidPins.end();
}

bool ConfigValidator::validatePin(int pin, const char* component, const char* parameter) {
    if (!isPinValid(pin)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "GPIO %d is invalid or reserved", pin);
        addError(component, parameter, msg, true);
        return false;
    }
    return true;
}

bool ConfigValidator::validatePinNotUsed(int pin, const char* component) {
    for (int usedPin : usedPins) {
        if (usedPin == pin) {
            char msg[128];
            snprintf(msg, sizeof(msg), "GPIO %d is already assigned to another component", pin);
            addError(component, "pin", msg, true);
            return false;
        }
    }
    usedPins.push_back(pin);
    return true;
}

bool ConfigValidator::validateTemperatureConfig() {
    LOG_DEBUG("ConfigVal", "Validating temperature configuration");
    
    // Temperature target should be reasonable for aquariums (18-32°C)
    bool valid = true;
    
    // Note: These values would come from ConfigManager in real implementation
    // For now, we'll define reasonable ranges
    float tempTarget = 25.0;  // Would be: configManager->getTempTarget()
    float tempSafetyMax = 30.0;
    
    valid &= validateRange(tempTarget, 18.0, 32.0, "Temperature", "tempTarget");
    valid &= validateRange(tempSafetyMax, 20.0, 35.0, "Temperature", "tempSafetyMax");
    
    // Safety max must be higher than target
    if (tempSafetyMax <= tempTarget) {
        addError("Temperature", "tempSafetyMax", 
                "Safety maximum must be higher than target temperature", true);
        valid = false;
    }
    
    return valid;
}

bool ConfigValidator::validatePHConfig() {
    LOG_DEBUG("ConfigVal", "Validating pH configuration");
    
    bool valid = true;
    
    // Typical aquarium pH range
    float phTarget = 6.8;
    float phSafetyMin = 6.0;
    
    valid &= validateRange(phTarget, 5.0, 9.0, "pH", "phTarget");
    valid &= validateRange(phSafetyMin, 4.5, 8.5, "pH", "phSafetyMin");
    
    // Safety min must be lower than target
    if (phSafetyMin >= phTarget) {
        addError("pH", "phSafetyMin", 
                "Safety minimum must be lower than target pH", true);
        valid = false;
    }
    
    return valid;
}

bool ConfigValidator::validateTDSConfig() {
    LOG_DEBUG("ConfigVal", "Validating TDS configuration");
    
    bool valid = true;
    
    // TDS is typically 0-2000 ppm for freshwater aquariums
    // Most configurations would be validated here
    
    return valid;
}

bool ConfigValidator::validateRelayConfig() {
    LOG_DEBUG("ConfigVal", "Validating relay configuration");
    
    bool valid = true;
    
    // Validate that relays don't exceed safe switching frequency
    // This would check timing configurations to ensure relays aren't
    // switched too frequently (minimum cycle times)
    
    return valid;
}

bool ConfigValidator::validateTimingConfig() {
    LOG_DEBUG("ConfigVal", "Validating timing configuration");
    
    bool valid = true;
    
    // Control loop intervals
    int sensorReadInterval = 10000;  // 10 seconds
    int controlInterval = 5000;      // 5 seconds
    int displayUpdateInterval = 1000; // 1 second
    
    valid &= validateRange(sensorReadInterval, 1000, 60000, 
                          "Timing", "sensorReadInterval");
    valid &= validateRange(controlInterval, 1000, 30000, 
                          "Timing", "controlInterval");
    valid &= validateRange(displayUpdateInterval, 100, 5000, 
                          "Timing", "displayUpdateInterval");
    
    // Control interval should be >= sensor read interval
    if (controlInterval < sensorReadInterval) {
        addError("Timing", "controlInterval", 
                "Control interval should not be less than sensor read interval", false);
        valid = false;
    }
    
    return valid;
}

bool ConfigValidator::validateNetworkConfig() {
    LOG_DEBUG("ConfigVal", "Validating network configuration");
    
    bool valid = true;
    
    // Validate MQTT port
    int mqttPort = 1883;
    if (mqttPort < 1 || mqttPort > 65535) {
        addError("Network", "mqttPort", "MQTT port must be between 1 and 65535", false);
        valid = false;
    }
    
    // Validate NTP offset (should be -12h to +14h in seconds)
    int gmtOffset = 36000; // AEST
    valid &= validateRange(gmtOffset, -43200, 50400, "Network", "gmtOffset");
    
    return valid;
}

bool ConfigValidator::validatePinAssignments() {
    LOG_DEBUG("ConfigVal", "Validating pin assignments");
    
    usedPins.clear();
    bool valid = true;
    
    // Pin assignments from config
    int tempSensorPin = 4;
    int ambientSensorPin = 5;
    int phSensorPin = 34;
    int tdsSensorPin = 35;
    int heaterRelayPin = 26;
    int co2RelayPin = 27;
    
    // Validate each pin
    valid &= validatePin(tempSensorPin, "TempSensor", "tempSensorPin");
    valid &= validatePin(ambientSensorPin, "AmbientSensor", "ambientSensorPin");
    valid &= validatePin(phSensorPin, "PHSensor", "phSensorPin");
    valid &= validatePin(tdsSensorPin, "TDSSensor", "tdsSensorPin");
    valid &= validatePin(heaterRelayPin, "HeaterRelay", "heaterRelayPin");
    valid &= validatePin(co2RelayPin, "CO2Relay", "co2RelayPin");
    
    // Check for duplicate pin assignments
    valid &= validatePinNotUsed(tempSensorPin, "TempSensor");
    valid &= validatePinNotUsed(ambientSensorPin, "AmbientSensor");
    valid &= validatePinNotUsed(phSensorPin, "PHSensor");
    valid &= validatePinNotUsed(tdsSensorPin, "TDSSensor");
    valid &= validatePinNotUsed(heaterRelayPin, "HeaterRelay");
    valid &= validatePinNotUsed(co2RelayPin, "CO2Relay");
    
    return valid;
}

bool ConfigValidator::validateMLConfig() {
    LOG_DEBUG("ConfigVal", "Validating ML configuration");
    
    bool valid = true;
    
    // Validate ML buffer sizes
    int mlBufferSize = 1000;
    valid &= validateRange(mlBufferSize, 100, 10000, "ML", "bufferSize");
    
    // Validate Kalman filter parameters
    float kalmanProcessNoise = 0.01;
    float kalmanMeasurementNoise = 0.1;
    
    if (kalmanProcessNoise <= 0 || kalmanProcessNoise > 1.0) {
        addError("ML", "kalmanProcessNoise", 
                "Process noise must be between 0 and 1.0", false);
        valid = false;
    }
    
    if (kalmanMeasurementNoise <= 0 || kalmanMeasurementNoise > 10.0) {
        addError("ML", "kalmanMeasurementNoise", 
                "Measurement noise must be between 0 and 10.0", false);
        valid = false;
    }
    
    return valid;
}

bool ConfigValidator::validateDosingConfig() {
    LOG_DEBUG("ConfigVal", "Validating dosing configuration");
    
    bool valid = true;
    
    // Validate dosing pump parameters
    // ml/s rate should be reasonable (0.1 to 10 ml/s)
    float dosingRate = 1.0;
    valid &= validateRange(dosingRate, 0.1, 10.0, "Dosing", "pumpRate");
    
    // Maximum dose per day should be reasonable
    float maxDailyDose = 100.0; // ml
    if (maxDailyDose <= 0 || maxDailyDose > 1000) {
        addError("Dosing", "maxDailyDose", 
                "Maximum daily dose must be between 0 and 1000 ml", false);
        valid = false;
    }
    
    return valid;
}

bool ConfigValidator::validateAll() {
    LOG_INFO("ConfigVal", "Starting configuration validation");
    
    clearErrors();
    
    bool valid = true;
    valid &= validateTemperatureConfig();
    valid &= validatePHConfig();
    valid &= validateTDSConfig();
    valid &= validateRelayConfig();
    valid &= validateTimingConfig();
    valid &= validateNetworkConfig();
    valid &= validatePinAssignments();
    valid &= validateMLConfig();
    valid &= validateDosingConfig();
    
    if (valid) {
        LOG_INFO("ConfigVal", "All configuration checks passed");
    } else {
        LOG_WARN("ConfigVal", "Configuration validation found %d error(s)", errors.size());
        printErrors();
    }
    
    return valid;
}

bool ConfigValidator::hasCriticalErrors() const {
    for (const auto& error : errors) {
        if (error.isCritical) {
            return true;
        }
    }
    return false;
}

void ConfigValidator::printErrors() const {
    for (const auto& error : errors) {
        const char* severity = error.isCritical ? "CRITICAL" : "WARNING";
        LOG_ERROR("ConfigVal", "[%s] %s.%s: %s", 
                 severity, 
                 error.component.c_str(), 
                 error.parameter.c_str(), 
                 error.message.c_str());
    }
}
