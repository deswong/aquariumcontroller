#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(uint8_t dataPin) 
    : pin(dataPin), currentTemp(0), readIndex(0), numReadings(10), 
      total(0), initialized(false), lastReadTime(0) {
    
    oneWire = new OneWire(pin);
    sensors = new DallasTemperature(oneWire);
    
    // Initialize readings array
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }
}

TemperatureSensor::~TemperatureSensor() {
    delete sensors;
    delete oneWire;
}

bool TemperatureSensor::begin() {
    sensors->begin();
    int deviceCount = sensors->getDeviceCount();
    
    if (deviceCount > 0) {
        sensors->setResolution(12); // 12-bit resolution
        sensors->setWaitForConversion(false); // Async mode
        initialized = true;
        Serial.printf("Temperature sensor initialized. Found %d device(s)\n", deviceCount);
        return true;
    }
    
    Serial.println("ERROR: No temperature sensors found!");
    return false;
}

float TemperatureSensor::readTemperature() {
    if (!initialized) return -127.0;
    
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime < READ_INTERVAL) {
        return currentTemp;
    }
    
    sensors->requestTemperatures();
    delay(100); // Wait for conversion
    
    float temp = sensors->getTempCByIndex(0);
    
    // Check for valid reading
    if (temp != DEVICE_DISCONNECTED_C && temp > -55 && temp < 125) {
        // Update moving average
        total = total - readings[readIndex];
        readings[readIndex] = temp;
        total = total + readings[readIndex];
        readIndex = (readIndex + 1) % numReadings;
        
        currentTemp = total / numReadings;
        lastReadTime = currentTime;
    }
    
    return currentTemp;
}

float TemperatureSensor::getTemperature() {
    return currentTemp;
}

bool TemperatureSensor::isValid() {
    return initialized && currentTemp > -55 && currentTemp < 125;
}
