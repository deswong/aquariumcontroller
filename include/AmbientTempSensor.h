#ifndef AMBIENT_TEMP_SENSOR_H
#define AMBIENT_TEMP_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class AmbientTempSensor {
private:
    uint8_t pin;
    OneWire* oneWire;
    DallasTemperature* sensors;
    float currentTemp;
    float readings[5];  // Smaller buffer than water temp (slower changes)
    int readIndex;
    int numReadings;
    float total;
    bool initialized;
    unsigned long lastReadTime;
    static const unsigned long READ_INTERVAL = 5000; // 5 seconds (air temp changes slowly)

public:
    AmbientTempSensor(uint8_t oneWirePin);
    ~AmbientTempSensor();
    
    bool begin();
    float readTemperature();
    float getTemperature();
    bool isValid();
};

#endif
