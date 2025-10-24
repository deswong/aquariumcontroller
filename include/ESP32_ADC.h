#ifndef ESP32_ADC_H
#define ESP32_ADC_H

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

/**
 * ESP32-S3 Hardware ADC Helper
 * 
 * Provides high-accuracy ADC readings using ESP32 hardware features:
 * - Hardware multisampling (reduces noise)
 * - ADC calibration (eFuse or two-point)
 * - Proper attenuation configuration
 * - DMA support for non-blocking reads (future)
 * 
 * Benefits over analogRead():
 * - 2-3x better accuracy (±50mV vs ±200mV)
 * - Hardware-based noise reduction
 * - Calibrated voltage readings
 * - Less CPU overhead
 */
class ESP32_ADC {
private:
    uint8_t pin;
    adc1_channel_t channel;
    adc_atten_t attenuation;
    adc_bits_width_t width;
    uint32_t samples;
    
    esp_adc_cal_characteristics_t *adc_chars;
    bool initialized;
    bool calibrated;
    
    // Convert GPIO pin to ADC1 channel
    adc1_channel_t gpioToADC1Channel(uint8_t gpio);
    
    // Get the best available calibration method
    esp_adc_cal_value_t getCalibrationMethod();

public:
    /**
     * Constructor
     * @param analogPin GPIO pin number (must be ADC1 pin)
     * @param atten Attenuation level (default: 11dB for 0-3.3V range)
     * @param numSamples Hardware multisampling count (default: 64 samples)
     */
    ESP32_ADC(uint8_t analogPin, 
              adc_atten_t atten = ADC_ATTEN_DB_11,
              uint32_t numSamples = 64);
    
    ~ESP32_ADC();
    
    /**
     * Initialize ADC with hardware calibration
     * @return true if successful, false otherwise
     */
    bool begin();
    
    /**
     * Read raw ADC value (0-4095 for 12-bit)
     * @return Raw ADC value
     */
    uint32_t readRaw();
    
    /**
     * Read calibrated voltage with hardware multisampling
     * @return Voltage in millivolts (mV)
     */
    uint32_t readVoltage_mV();
    
    /**
     * Read calibrated voltage
     * @return Voltage in volts (V)
     */
    float readVoltage();
    
    /**
     * Read multiple samples and return average voltage
     * @param count Number of samples to average
     * @return Average voltage in volts (V)
     */
    float readVoltageAverage(uint32_t count = 10);
    
    /**
     * Check if ADC is properly initialized and calibrated
     */
    bool isReady() const { return initialized && calibrated; }
    
    /**
     * Get calibration method used
     */
    const char* getCalibrationName() const;
    
    /**
     * Print ADC configuration and calibration info
     */
    void printInfo() const;
};

#endif
