#include "PHSensor.h"

PHSensor::PHSensor(uint8_t analogPin) 
    : pin(analogPin), adc(nullptr), currentPH(7.0), readIndex(0), numReadings(0), total(0), 
      initialized(false), lastReadTime(0),
      acidVoltage(2.0), neutralVoltage(1.5), baseVoltage(1.0), calibrated(false),
      acidCalibrated(false), neutralCalibrated(false), baseCalibrated(false), numCalibrationPoints(0),
      acidCalTemp(25.0), neutralCalTemp(25.0), baseCalTemp(25.0),
      acidTrueRef(4.01), neutralTrueRef(7.00), baseTrueRef(10.01), refTemp(25.0),
      isCalibrating(false), lastCalibrationTime(0) {
    
    // Create ESP32 hardware ADC with 11dB attenuation (0-3.3V range) and 64-sample averaging
    adc = new ESP32_ADC(analogPin, ADC_ATTEN_DB_11, 64);
    prefs = new Preferences();
    
    // Initialize readings array
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 7.0;
    }
    total = 7.0 * numReadings;
}

PHSensor::~PHSensor() {
    if (adc) {
        delete adc;
    }
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

bool PHSensor::begin() {
    // Initialize ESP32 hardware ADC
    if (!adc->begin()) {
        Serial.println("ERROR: Failed to initialize pH sensor ADC");
        return false;
    }
    
    // Print ADC configuration
    Serial.println("pH Sensor ADC Configuration:");
    adc->printInfo();
    
    initialized = true;
    loadCalibration();
    Serial.println("pH sensor initialized");
    return true;
}

float PHSensor::readVoltage() {
    // Use ESP32 hardware ADC with calibration and multisampling
    // Returns calibrated voltage with hardware-based noise reduction
    if (!adc || !adc->isReady()) {
        Serial.println("ERROR: pH ADC not ready");
        return 1.65; // Default to neutral voltage
    }
    
    return adc->readVoltage();
}

float PHSensor::voltageTopH(float voltage, float currentTemp) {
    if (!calibrated) {
        // Default linear conversion if not calibrated
        // Typical pH probe: 3.3V = pH 0, 0V = pH 14
        float rawPH = 7.0 - ((voltage - 1.65) * 3.5);
        return applyTempCompensation(rawPH, currentTemp);
    }
    
    // Calibration curve based on available points
    float rawPH;
    
    if (numCalibrationPoints == 1) {
        // Single-point calibration (typically pH 7)
        if (neutralCalibrated) {
            // Linear offset from neutral point
            float voltageOffset = voltage - neutralVoltage;
            // Assume typical slope of ~59.16 mV/pH at 25°C (Nernstian slope)
            // With 3.3V ADC, that's approximately 0.165V per pH unit
            rawPH = 7.0 - (voltageOffset / 0.165);
        } else if (acidCalibrated) {
            rawPH = 4.0 - ((voltage - acidVoltage) / 0.165);
        } else {
            rawPH = 10.0 - ((voltage - baseVoltage) / 0.165);
        }
    } else if (numCalibrationPoints == 2) {
        // Two-point calibration - linear interpolation
        if (neutralCalibrated && acidCalibrated) {
            // pH 7 and pH 4
            float slope = (7.0 - 4.0) / (neutralVoltage - acidVoltage);
            rawPH = 4.0 + (voltage - acidVoltage) * slope;
        } else if (neutralCalibrated && baseCalibrated) {
            // pH 7 and pH 10
            float slope = (10.0 - 7.0) / (baseVoltage - neutralVoltage);
            rawPH = 7.0 + (voltage - neutralVoltage) * slope;
        } else {
            // pH 4 and pH 10 (wider range, less common)
            float slope = (10.0 - 4.0) / (baseVoltage - acidVoltage);
            rawPH = 4.0 + (voltage - acidVoltage) * slope;
        }
    } else {
        // Three-point calibration - piecewise linear
        if (voltage > neutralVoltage) {
            // Between acid (4.0) and neutral (7.0)
            float slope = (7.0 - 4.0) / (neutralVoltage - acidVoltage);
            rawPH = 4.0 + (voltage - acidVoltage) * slope;
        } else {
            // Between neutral (7.0) and base (10.0)
            float slope = (10.0 - 7.0) / (baseVoltage - neutralVoltage);
            rawPH = 7.0 + (voltage - neutralVoltage) * slope;
        }
    }
    
    // Apply temperature compensation
    return applyTempCompensation(rawPH, currentTemp);
}

float PHSensor::applyTempCompensation(float rawPH, float currentTemp) {
    // Temperature compensation based on Nernst equation
    // pH change = ~0.003 pH units per °C per pH unit away from 7
    // This is a simplified model; actual compensation depends on the buffer solution
    
    if (!calibrated) {
        // Generic temperature compensation
        float tempDiff = currentTemp - refTemp;
        float pHFromNeutral = rawPH - 7.0;
        float correction = -0.003 * tempDiff * pHFromNeutral;
        return rawPH + correction;
    }
    
    // Use calibration-specific temperature compensation
    // Determine which calibration point to use based on pH range
    float calTemp, trueRef;
    if (rawPH < 5.5) {
        // Use acid calibration data
        calTemp = acidCalTemp;
        trueRef = acidTrueRef;
    } else if (rawPH < 8.5) {
        // Use neutral calibration data
        calTemp = neutralCalTemp;
        trueRef = neutralTrueRef;
    } else {
        // Use base calibration data
        calTemp = baseCalTemp;
        trueRef = baseTrueRef;
    }
    
    // Calculate temperature offset for this solution
    // Most pH buffers shift ~0.003-0.02 pH/°C depending on the buffer
    // We'll use the Nernst-based approximation
    float tempDiff = currentTemp - calTemp;
    float pHFromNeutral = trueRef - 7.0;
    float correction = -0.003 * tempDiff * pHFromNeutral;
    
    return rawPH + correction;
}

float PHSensor::readPH(float waterTemp, float ambientTemp) {
    if (!initialized) return 7.0;
    
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime < READ_INTERVAL) {
        return currentPH;
    }
    
    float voltage = readVoltage();
    
    // Use ambient temp during calibration, water temp during normal operation
    float tempToUse = isCalibrating ? ambientTemp : waterTemp;
    float ph = voltageTopH(voltage, tempToUse);
    
    // Sanity check
    if (ph >= 0 && ph <= 14) {
        // Update moving average
        total = total - readings[readIndex];
        readings[readIndex] = ph;
        total = total + readings[readIndex];
        readIndex = (readIndex + 1) % numReadings;
        
        currentPH = total / numReadings;
        lastReadTime = currentTime;
    }
    
    return currentPH;
}

float PHSensor::getPH() {
    return currentPH;
}

bool PHSensor::isValid() {
    return initialized && currentPH >= 0 && currentPH <= 14;
}

bool PHSensor::isCalibrated() {
    return calibrated;
}

void PHSensor::startCalibration() {
    Serial.println("Starting pH calibration (using ambient air temperature)...");
    Serial.println("You can calibrate with 1, 2, or 3 points:");
    Serial.println("  1-point: pH 7.0 (neutral) - basic offset calibration");
    Serial.println("  2-point: pH 4.0 + pH 7.0 OR pH 7.0 + pH 10.0 - slope calibration");
    Serial.println("  3-point: pH 4.0 + pH 7.0 + pH 10.0 - full curve calibration (recommended)");
    calibrated = false;
    acidCalibrated = false;
    neutralCalibrated = false;
    baseCalibrated = false;
    numCalibrationPoints = 0;
    isCalibrating = true;
}

void PHSensor::endCalibration() {
    Serial.printf("Ending pH calibration mode - %d point(s) calibrated\n", numCalibrationPoints);
    if (numCalibrationPoints >= 1) {
        calibrated = true;
        Serial.println("✓ Calibration complete (switching to water temperature)");
        if (numCalibrationPoints == 1) {
            Serial.println("  Note: Single-point calibration provides basic accuracy");
        } else if (numCalibrationPoints == 2) {
            Serial.println("  Note: Two-point calibration provides good accuracy");
        } else {
            Serial.println("  Note: Three-point calibration provides best accuracy");
        }
    } else {
        Serial.println("⚠ Warning: No calibration points set - using defaults");
    }
    isCalibrating = false;
}

bool PHSensor::calibratePoint(float knownPH, float solutionTemp, float solutionRefPH) {
    // Take multiple readings and average
    float voltageSum = 0;
    int numSamples = 50;
    
    for (int i = 0; i < numSamples; i++) {
        voltageSum += readVoltage();
        delay(20);
    }
    
    float avgVoltage = voltageSum / numSamples;
    
    // Store calibration point based on pH
    if (abs(knownPH - 4.0) < 0.5) {
        acidVoltage = avgVoltage;
        acidCalTemp = solutionTemp;
        if (solutionRefPH > 0) {
            acidTrueRef = solutionRefPH;
        }
        if (!acidCalibrated) {
            numCalibrationPoints++;
            acidCalibrated = true;
        }
        Serial.printf("✓ Calibrated acid point: pH %.2f at %.1f°C = %.3fV (Ref pH: %.2f at %.1f°C)\n", 
                      knownPH, solutionTemp, avgVoltage, acidTrueRef, refTemp);
        Serial.printf("  Total points: %d/3\n", numCalibrationPoints);
        return true;
    } else if (abs(knownPH - 7.0) < 0.5) {
        neutralVoltage = avgVoltage;
        neutralCalTemp = solutionTemp;
        if (solutionRefPH > 0) {
            neutralTrueRef = solutionRefPH;
        }
        if (!neutralCalibrated) {
            numCalibrationPoints++;
            neutralCalibrated = true;
        }
        Serial.printf("✓ Calibrated neutral point: pH %.2f at %.1f°C = %.3fV (Ref pH: %.2f at %.1f°C)\n", 
                      knownPH, solutionTemp, avgVoltage, neutralTrueRef, refTemp);
        Serial.printf("  Total points: %d/3\n", numCalibrationPoints);
        return true;
    } else if (abs(knownPH - 10.0) < 0.5) {
        baseVoltage = avgVoltage;
        baseCalTemp = solutionTemp;
        if (solutionRefPH > 0) {
            baseTrueRef = solutionRefPH;
        }
        if (!baseCalibrated) {
            numCalibrationPoints++;
            baseCalibrated = true;
        }
        Serial.printf("✓ Calibrated base point: pH %.2f at %.1f°C = %.3fV (Ref pH: %.2f at %.1f°C)\n", 
                      knownPH, solutionTemp, avgVoltage, baseTrueRef, refTemp);
        Serial.printf("  Total points: %d/3\n", numCalibrationPoints);
        return true;
    }
    
    Serial.println("ERROR: Unknown calibration pH value. Use 4.0, 7.0, or 10.0");
    return false;
}

void PHSensor::saveCalibration() {
    if (!prefs->begin("ph-calibration", false)) {
        Serial.println("ERROR: Failed to open preferences for pH calibration");
        return;
    }
    
    // Update calibration timestamp
    lastCalibrationTime = millis() / 1000; // Store as seconds since boot
    // Note: For real-world use, you'd want to use NTP time or RTC
    // For now, we'll track relative time since last calibration
    
    prefs->putFloat("acidV", acidVoltage);
    prefs->putFloat("neutralV", neutralVoltage);
    prefs->putFloat("baseV", baseVoltage);
    prefs->putBool("calibrated", calibrated);
    prefs->putBool("acidCal", acidCalibrated);
    prefs->putBool("neutralCal", neutralCalibrated);
    prefs->putBool("baseCal", baseCalibrated);
    prefs->putInt("numPoints", numCalibrationPoints);
    
    // Save temperature compensation data
    prefs->putFloat("acidTemp", acidCalTemp);
    prefs->putFloat("neutralTemp", neutralCalTemp);
    prefs->putFloat("baseTemp", baseCalTemp);
    prefs->putFloat("acidRef", acidTrueRef);
    prefs->putFloat("neutralRef", neutralTrueRef);
    prefs->putFloat("baseRef", baseTrueRef);
    prefs->putFloat("refTemp", refTemp);
    
    // Save calibration timestamp
    prefs->putULong("calTime", lastCalibrationTime);
    
    prefs->end();
    
    int daysSince = getDaysSinceCalibration();
    Serial.printf("pH calibration with temperature compensation saved to NVS (age: %d days)\n", daysSince);
}

void PHSensor::loadCalibration() {
    if (!prefs->begin("ph-calibration", true)) {
        Serial.println("No pH calibration found, using defaults");
        return;
    }
    
    acidVoltage = prefs->getFloat("acidV", 2.0);
    neutralVoltage = prefs->getFloat("neutralV", 1.5);
    baseVoltage = prefs->getFloat("baseV", 1.0);
    calibrated = prefs->getBool("calibrated", false);
    acidCalibrated = prefs->getBool("acidCal", false);
    neutralCalibrated = prefs->getBool("neutralCal", false);
    baseCalibrated = prefs->getBool("baseCal", false);
    numCalibrationPoints = prefs->getInt("numPoints", 0);
    
    // Load temperature compensation data
    acidCalTemp = prefs->getFloat("acidTemp", 25.0);
    neutralCalTemp = prefs->getFloat("neutralTemp", 25.0);
    baseCalTemp = prefs->getFloat("baseTemp", 25.0);
    acidTrueRef = prefs->getFloat("acidRef", 4.01);
    neutralTrueRef = prefs->getFloat("neutralRef", 7.00);
    baseTrueRef = prefs->getFloat("baseRef", 10.01);
    refTemp = prefs->getFloat("refTemp", 25.0);
    
    // Load calibration timestamp
    lastCalibrationTime = prefs->getULong("calTime", 0);
    
    prefs->end();
    
    if (calibrated) {
        int daysSince = getDaysSinceCalibration();
        Serial.printf("pH calibration loaded (%d-point) with temp compensation:\n", numCalibrationPoints);
        if (acidCalibrated) {
            Serial.printf("  Acid: %.3fV at %.1f°C (ref pH %.2f at %.1f°C)\n", 
                      acidVoltage, acidCalTemp, acidTrueRef, refTemp);
        }
        if (neutralCalibrated) {
            Serial.printf("  Neutral: %.3fV at %.1f°C (ref pH %.2f at %.1f°C)\n", 
                      neutralVoltage, neutralCalTemp, neutralTrueRef, refTemp);
        }
        if (baseCalibrated) {
            Serial.printf("  Base: %.3fV at %.1f°C (ref pH %.2f at %.1f°C)\n", 
                      baseVoltage, baseCalTemp, baseTrueRef, refTemp);
        }
        Serial.printf("  Age: %d days since last calibration", daysSince);
        
        if (isCalibrationExpired()) {
            Serial.println(" ⚠️  EXPIRED - Recalibrate immediately!");
        } else if (needsCalibration()) {
            Serial.println(" ⚠️  WARNING - Recalibration recommended");
        } else {
            Serial.println(" ✓");
        }
    }
}

void PHSensor::resetCalibration() {
    acidVoltage = 2.0;
    neutralVoltage = 1.5;
    baseVoltage = 1.0;
    calibrated = false;
    acidCalibrated = false;
    neutralCalibrated = false;
    baseCalibrated = false;
    numCalibrationPoints = 0;
    
    // Reset temperature compensation to defaults
    acidCalTemp = 25.0;
    neutralCalTemp = 25.0;
    baseCalTemp = 25.0;
    acidTrueRef = 4.01;
    neutralTrueRef = 7.00;
    baseTrueRef = 10.01;
    refTemp = 25.0;
    lastCalibrationTime = 0;
    
    saveCalibration();
    Serial.println("pH calibration and temperature compensation reset");
}

int PHSensor::getDaysSinceCalibration() {
    if (lastCalibrationTime == 0) {
        return 999; // Never calibrated
    }
    
    unsigned long currentTime = millis() / 1000; // Convert to seconds
    unsigned long elapsedSeconds = currentTime - lastCalibrationTime;
    return elapsedSeconds / 86400; // Convert seconds to days
}

bool PHSensor::needsCalibration() {
    return getDaysSinceCalibration() >= CALIBRATION_WARNING_DAYS;
}

bool PHSensor::isCalibrationExpired() {
    return getDaysSinceCalibration() >= CALIBRATION_EXPIRED_DAYS;
}
