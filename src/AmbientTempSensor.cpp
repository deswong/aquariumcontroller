#include "AmbientTempSensor.h"

AmbientTempSensor::AmbientTempSensor(uint8_t oneWirePin) 
    : pin(oneWirePin), currentTemp(25.0), readIndex(0), numReadings(5), 
      total(0), initialized(false), lastReadTime(0) {
    
    oneWire = new OneWire(pin);
    sensors = new DallasTemperature(oneWire);
    
    // Initialize readings array
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 25.0;
    }
    total = 25.0 * numReadings;
}

AmbientTempSensor::~AmbientTempSensor() {
    if (sensors) delete sensors;
    if (oneWire) delete oneWire;
}

bool AmbientTempSensor::begin() {
    sensors->begin();
    
    int deviceCount = sensors->getDeviceCount();
    if (deviceCount == 0) {
        Serial.println("ERROR: No ambient temperature sensor found!");
        return false;
    }
    
    sensors->setResolution(12); // 12-bit resolution (0.0625°C)
    sensors->setWaitForConversion(true);
    
    initialized = true;
    Serial.printf("Ambient temperature sensor initialized (%d device found)\n", deviceCount);
    return true;
}

float AmbientTempSensor::readTemperature() {
    if (!initialized) return 25.0;
    
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime < READ_INTERVAL) {
        return currentTemp;
    }
    
    sensors->requestTemperatures();
    float temp = sensors->getTempCByIndex(0);
    
    // Sanity check for ambient air temperature (15-40°C typical)
    if (temp > 10.0 && temp < 45.0) {
        // Update moving average
        total = total - readings[readIndex];
        readings[readIndex] = temp;
        total = total + readings[readIndex];
        readIndex = (readIndex + 1) % numReadings;
        
        currentTemp = total / numReadings;
        lastReadTime = currentTime;
    } else {
        Serial.printf("WARNING: Ambient temperature out of range: %.1f°C\n", temp);
    }
    
    return currentTemp;
}

float AmbientTempSensor::getTemperature() {
    return currentTemp;
}

bool AmbientTempSensor::isValid() {
    return initialized && currentTemp > 10.0 && currentTemp < 45.0;
}
