#ifndef CONFIG_VALIDATOR_H
#define CONFIG_VALIDATOR_H

#include <Arduino.h>
#include <vector>

// Validation result structure
struct ValidationError {
    String component;
    String parameter;
    String message;
    bool isCritical;  // If true, system should not start
};

class ConfigValidator {
public:
    ConfigValidator();
    
    // Main validation method
    bool validateAll();
    
    // Individual validation methods
    bool validateTemperatureConfig();
    bool validatePHConfig();
    bool validateTDSConfig();
    bool validateRelayConfig();
    bool validateTimingConfig();
    bool validateNetworkConfig();
    bool validatePinAssignments();
    bool validateMLConfig();
    bool validateDosingConfig();
    
    // Get validation results
    const std::vector<ValidationError>& getErrors() const { return errors; }
    void printErrors() const;
    bool hasErrors() const { return !errors.empty(); }
    bool hasCriticalErrors() const;
    
    // Clear previous errors
    void clearErrors() { errors.clear(); }
    
private:
    std::vector<ValidationError> errors;
    
    // Helper methods
    void addError(const char* component, const char* parameter, 
                  const char* message, bool critical = false);
    bool validateRange(float value, float min, float max, 
                      const char* component, const char* parameter);
    bool validatePin(int pin, const char* component, const char* parameter);
    bool validatePinNotUsed(int pin, const char* component);
    bool isPinValid(int pin);
    
    // Pin tracking
    std::vector<int> usedPins;
};

#endif // CONFIG_VALIDATOR_H
