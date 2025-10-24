#ifndef TDS_SENSOR_H
#define TDS_SENSOR_H

#include <Arduino.h>
#include "ESP32_ADC.h"

class TDSSensor {
private:
    uint8_t pin;
    ESP32_ADC* adc;  // ESP32 hardware ADC
    float currentTDS;
    float readings[10];
    int readIndex;
    int numReadings;
    float total;
    bool initialized;
    unsigned long lastReadTime;
    static const unsigned long READ_INTERVAL = 1000; // 1 second
    
    float temperature; // Temperature compensation
    float kValue; // TDS conversion factor (typically around 0.5)
    
    float readVoltage();
    float voltageToTDS(float voltage, float temp);

public:
    TDSSensor(uint8_t analogPin, float kFactor = 0.5);
    ~TDSSensor();
    
    bool begin();
    float readTDS(float waterTemp = 25.0);
    float getTDS();
    bool isValid();
    void setKValue(float k) { kValue = k; }
};

#endif
