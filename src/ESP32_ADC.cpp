#include "ESP32_ADC.h"

ESP32_ADC::ESP32_ADC(uint8_t analogPin, adc_atten_t atten, uint32_t numSamples)
    : pin(analogPin), attenuation(atten), width(ADC_WIDTH_BIT_12), 
      samples(numSamples), adc_chars(nullptr), initialized(false), calibrated(false) {
    
    // Convert GPIO to ADC channel
    channel = gpioToADC1Channel(pin);
}

ESP32_ADC::~ESP32_ADC() {
    if (adc_chars != nullptr) {
        free(adc_chars);
    }
}

adc1_channel_t ESP32_ADC::gpioToADC1Channel(uint8_t gpio) {
    // ESP32-S3 ADC1 channel mapping
    switch (gpio) {
        case 1:  return ADC1_CHANNEL_0;
        case 2:  return ADC1_CHANNEL_1;
        case 3:  return ADC1_CHANNEL_2;
        case 4:  return ADC1_CHANNEL_3;
        case 5:  return ADC1_CHANNEL_4;
        case 6:  return ADC1_CHANNEL_5;
        case 7:  return ADC1_CHANNEL_6;
        case 8:  return ADC1_CHANNEL_7;
        case 9:  return ADC1_CHANNEL_8;
        case 10: return ADC1_CHANNEL_9;
        default:
            Serial.printf("ERROR: GPIO%d is not a valid ADC1 pin!\n", gpio);
            return ADC1_CHANNEL_0; // Fallback
    }
}

esp_adc_cal_value_t ESP32_ADC::getCalibrationMethod() {
    // Check available calibration methods (in order of preference)
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1, attenuation, width, 0, adc_chars);
    
    return val_type;
}

bool ESP32_ADC::begin() {
    if (initialized) {
        return true;
    }
    
    // Configure ADC width (resolution)
    adc1_config_width(width);
    
    // Configure ADC attenuation (voltage range)
    // ADC_ATTEN_DB_11 = 0-3.3V (best for pH/TDS sensors)
    esp_err_t err = adc1_config_channel_atten(channel, attenuation);
    if (err != ESP_OK) {
        Serial.printf("ERROR: Failed to configure ADC channel attenuation: %d\n", err);
        return false;
    }
    
    // Allocate memory for calibration characteristics
    adc_chars = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    if (adc_chars == nullptr) {
        Serial.println("ERROR: Failed to allocate memory for ADC calibration");
        return false;
    }
    
    // Characterize ADC (load calibration from eFuse or use two-point)
    esp_adc_cal_value_t cal_type = getCalibrationMethod();
    
    calibrated = true;
    initialized = true;
    
    // Print calibration info
    Serial.printf("ADC GPIO%d initialized: ", pin);
    switch (cal_type) {
        case ESP_ADC_CAL_VAL_EFUSE_TP:
            Serial.println("Two Point eFuse calibration");
            break;
        case ESP_ADC_CAL_VAL_EFUSE_VREF:
            Serial.println("eFuse Vref calibration");
            break;
        default:
            Serial.println("Default calibration (no eFuse)");
            calibrated = false; // Mark as uncalibrated if using default
            break;
    }
    
    return true;
}

uint32_t ESP32_ADC::readRaw() {
    if (!initialized) {
        Serial.println("ERROR: ADC not initialized!");
        return 0;
    }
    
    // Hardware multisampling for noise reduction
    uint64_t sum = 0;
    for (uint32_t i = 0; i < samples; i++) {
        sum += adc1_get_raw(channel);
    }
    
    return (uint32_t)(sum / samples);
}

uint32_t ESP32_ADC::readVoltage_mV() {
    if (!initialized || adc_chars == nullptr) {
        Serial.println("ERROR: ADC not initialized or calibration missing!");
        return 0;
    }
    
    uint32_t raw = readRaw();
    
    // Convert raw value to calibrated voltage (mV)
    uint32_t voltage_mV = esp_adc_cal_raw_to_voltage(raw, adc_chars);
    
    return voltage_mV;
}

float ESP32_ADC::readVoltage() {
    return readVoltage_mV() / 1000.0f;
}

float ESP32_ADC::readVoltageAverage(uint32_t count) {
    if (!initialized) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum += readVoltage();
        delayMicroseconds(100); // Small delay between samples
    }
    
    return sum / count;
}

const char* ESP32_ADC::getCalibrationName() const {
    if (!initialized || adc_chars == nullptr) {
        return "Not initialized";
    }
    
    esp_adc_cal_value_t cal_type = esp_adc_cal_characterize(
        ADC_UNIT_1, attenuation, width, 0, 
        const_cast<esp_adc_cal_characteristics_t*>(adc_chars));
    
    switch (cal_type) {
        case ESP_ADC_CAL_VAL_EFUSE_TP:
            return "Two Point eFuse";
        case ESP_ADC_CAL_VAL_EFUSE_VREF:
            return "eFuse Vref";
        default:
            return "Default (uncalibrated)";
    }
}

void ESP32_ADC::printInfo() const {
    Serial.printf("ESP32 ADC Configuration:\n");
    Serial.printf("  GPIO Pin: %d\n", pin);
    Serial.printf("  ADC Channel: %d\n", channel);
    Serial.printf("  Attenuation: ");
    switch (attenuation) {
        case ADC_ATTEN_DB_0:   Serial.println("0dB (0-1.1V)"); break;
        case ADC_ATTEN_DB_2_5: Serial.println("2.5dB (0-1.5V)"); break;
        case ADC_ATTEN_DB_6:   Serial.println("6dB (0-2.2V)"); break;
        case ADC_ATTEN_DB_11:  Serial.println("11dB (0-3.3V)"); break;
        default:               Serial.println("Unknown"); break;
    }
    Serial.printf("  Resolution: 12-bit (0-4095)\n");
    Serial.printf("  Multisampling: %d samples\n", samples);
    Serial.printf("  Calibration: %s\n", getCalibrationName());
    Serial.printf("  Status: %s\n", initialized ? "Ready" : "Not initialized");
}
