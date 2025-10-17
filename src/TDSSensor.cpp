#include "TDSSensor.h"

TDSSensor::TDSSensor(uint8_t analogPin, float kFactor) 
    : pin(analogPin), currentTDS(0), readIndex(0), numReadings(10), 
      total(0), initialized(false), lastReadTime(0), temperature(25.0), kValue(kFactor) {
    
    // Initialize readings array
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }
}

TDSSensor::~TDSSensor() {
}

bool TDSSensor::begin() {
    pinMode(pin, INPUT);
    initialized = true;
    Serial.println("TDS sensor initialized");
    return true;
}

float TDSSensor::readVoltage() {
    // Read analog value and convert to voltage
    int rawValue = analogRead(pin);
    float voltage = (rawValue / 4095.0) * 3.3;
    return voltage;
}

float TDSSensor::voltageToTDS(float voltage, float temp) {
    // Temperature compensation formula
    float compensationCoefficient = 1.0 + 0.02 * (temp - 25.0);
    float compensationVoltage = voltage / compensationCoefficient;
    
    // Convert voltage to TDS value (ppm)
    // Formula: TDS = (133.42 * voltage^3 - 255.86 * voltage^2 + 857.39 * voltage) * kValue
    float tdsValue = (133.42 * pow(compensationVoltage, 3) 
                     - 255.86 * pow(compensationVoltage, 2) 
                     + 857.39 * compensationVoltage) * kValue;
    
    return tdsValue;
}

float TDSSensor::readTDS(float waterTemp) {
    if (!initialized) return 0;
    
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime < READ_INTERVAL) {
        return currentTDS;
    }
    
    temperature = waterTemp;
    float voltage = readVoltage();
    float tds = voltageToTDS(voltage, temperature);
    
    // Sanity check (typical freshwater aquarium: 50-400 ppm)
    if (tds >= 0 && tds < 2000) {
        // Update moving average
        total = total - readings[readIndex];
        readings[readIndex] = tds;
        total = total + readings[readIndex];
        readIndex = (readIndex + 1) % numReadings;
        
        currentTDS = total / numReadings;
        lastReadTime = currentTime;
    }
    
    return currentTDS;
}

float TDSSensor::getTDS() {
    return currentTDS;
}

bool TDSSensor::isValid() {
    return initialized && currentTDS >= 0 && currentTDS < 2000;
}
