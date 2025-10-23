#include "TemperatureSensor.h"
#include "Logger.h"

TemperatureSensor::TemperatureSensor(uint8_t dataPin) 
    : pin(dataPin), currentTemp(0), readIndex(0), numReadings(10), 
      total(0), initialized(false), lastReadTime(0),
      lastValidReading(0), lastChangeTime(0) {
    
    oneWire = new OneWire(pin);
    sensors = new DallasTemperature(oneWire);
    
    // Initialize readings array
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }
    
    resetAnomaly();
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
        // Detect anomalies before updating
        detectAnomalies(temp);
        
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

void TemperatureSensor::detectAnomalies(float newReading) {
    unsigned long now = millis();
    
    // Reset anomaly flags
    resetAnomaly();
    
    // Check if reading is out of expected range
    if (newReading < MIN_EXPECTED_TEMP || newReading > MAX_EXPECTED_TEMP) {
        anomaly.outOfRange = true;
        anomaly.description = "Reading outside expected range (" + 
                             String(MIN_EXPECTED_TEMP) + "°C to " + 
                             String(MAX_EXPECTED_TEMP) + "°C)";
        LOG_WARN("TempSensor", "Out of range: %.2f°C", newReading);
    }
    
    // Check for spike (sudden large change)
    if (lastValidReading != 0) {
        float change = abs(newReading - lastValidReading);
        if (change > SPIKE_THRESHOLD) {
            anomaly.hasSpike = true;
            anomaly.description = "Sudden change of " + String(change, 2) + "°C detected";
            LOG_WARN("TempSensor", "Spike detected: %.2f°C change", change);
        }
    }
    
    // Check if sensor is stuck (reading hasn't changed)
    if (lastValidReading != 0 && abs(newReading - lastValidReading) < 0.01) {
        // Reading hasn't changed significantly
        if (lastChangeTime == 0) {
            lastChangeTime = now;
        } else {
            anomaly.stuckDuration = now - lastChangeTime;
            if (anomaly.stuckDuration > STUCK_TIMEOUT) {
                anomaly.isStuck = true;
                anomaly.description = "Sensor stuck at " + String(newReading, 2) + 
                                     "°C for " + String(anomaly.stuckDuration / 1000) + " seconds";
                LOG_WARN("TempSensor", "Sensor appears stuck at %.2f°C", newReading);
            }
        }
    } else {
        // Reading changed, reset stuck timer
        lastChangeTime = now;
    }
    
    anomaly.lastValue = newReading;
    lastValidReading = newReading;
}

void TemperatureSensor::resetAnomaly() {
    anomaly.isStuck = false;
    anomaly.hasSpike = false;
    anomaly.outOfRange = false;
    anomaly.stuckDuration = 0;
    anomaly.description = "";
}

String TemperatureSensor::getAnomalyDescription() const {
    if (!hasAnomaly()) {
        return "No anomalies detected";
    }
    return anomaly.description;
}
