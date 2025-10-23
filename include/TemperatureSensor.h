#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Anomaly detection structure
struct SensorAnomaly {
    bool isStuck;           // Sensor reading hasn't changed
    bool hasSpike;          // Sudden large change
    bool outOfRange;        // Reading outside expected range
    float lastValue;
    unsigned long stuckDuration;
    String description;
};

class TemperatureSensor {
private:
    OneWire* oneWire;
    DallasTemperature* sensors;
    uint8_t pin;
    float currentTemp;
    float readings[10];
    int readIndex;
    int numReadings;
    float total;
    bool initialized;
    unsigned long lastReadTime;
    static const unsigned long READ_INTERVAL = 1000; // 1 second
    
    // Anomaly detection
    SensorAnomaly anomaly;
    float lastValidReading;
    unsigned long lastChangeTime;
    static constexpr float SPIKE_THRESHOLD = 5.0;      // 5°C sudden change
    static constexpr unsigned long STUCK_TIMEOUT = 300000; // 5 minutes
    static constexpr float MIN_EXPECTED_TEMP = 10.0;   // 10°C minimum
    static constexpr float MAX_EXPECTED_TEMP = 40.0;   // 40°C maximum
    
    void detectAnomalies(float newReading);
    void resetAnomaly();

public:
    TemperatureSensor(uint8_t dataPin);
    ~TemperatureSensor();
    
    bool begin();
    float readTemperature();
    float getTemperature();
    bool isValid();
    
    // Anomaly detection methods
    const SensorAnomaly& getAnomaly() const { return anomaly; }
    bool hasAnomaly() const { return anomaly.isStuck || anomaly.hasSpike || anomaly.outOfRange; }
    String getAnomalyDescription() const;
};

#endif
