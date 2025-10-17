#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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

public:
    TemperatureSensor(uint8_t dataPin);
    ~TemperatureSensor();
    
    bool begin();
    float readTemperature();
    float getTemperature();
    bool isValid();
};

#endif
