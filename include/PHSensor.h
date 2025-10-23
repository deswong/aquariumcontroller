#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include <Arduino.h>
#include <Preferences.h>

class PHSensor {
private:
    uint8_t pin;
    float currentPH;
    float readings[20];
    int readIndex;
    int numReadings;
    float total;
    bool initialized;
    unsigned long lastReadTime;
    static const unsigned long READ_INTERVAL = 500; // 0.5 seconds
    
    // Calibration data
    float acidVoltage;    // pH 4.0 at calibration temp
    float neutralVoltage; // pH 7.0 at calibration temp
    float baseVoltage;    // pH 10.0 at calibration temp
    bool calibrated;
    
    // Track which points have been calibrated
    bool acidCalibrated;
    bool neutralCalibrated;
    bool baseCalibrated;
    int numCalibrationPoints;
    
    // Temperature compensation data
    float acidCalTemp;      // Temperature when pH 4.0 was calibrated
    float neutralCalTemp;   // Temperature when pH 7.0 was calibrated
    float baseCalTemp;      // Temperature when pH 10.0 was calibrated
    float acidTrueRef;      // True pH of acid solution at reference temp (usually 4.01)
    float neutralTrueRef;   // True pH of neutral solution at reference temp (usually 7.00)
    float baseTrueRef;      // True pH of base solution at reference temp (usually 10.01)
    float refTemp;          // Reference temperature (usually 25°C)
    bool isCalibrating;     // Flag to indicate calibration mode
    
    // Calibration age tracking
    unsigned long lastCalibrationTime; // Unix timestamp of last calibration
    static const int CALIBRATION_WARNING_DAYS = 30;
    static const int CALIBRATION_EXPIRED_DAYS = 60;
    
    Preferences* prefs;
    
    float voltageTopH(float voltage, float currentTemp);
    float readVoltage();
    float applyTempCompensation(float rawPH, float currentTemp);

public:
    PHSensor(uint8_t analogPin);
    ~PHSensor();
    
    bool begin();
    float readPH(float waterTemp = 25.0, float ambientTemp = 25.0); // Temperature-compensated reading
    float getPH();
    bool isValid();
    bool isCalibrated();
    bool inCalibrationMode() { return isCalibrating; }
    
    // Calibration methods with temperature compensation
    void startCalibration();
    void endCalibration();
    bool calibratePoint(float knownPH, float solutionTemp, float solutionRefPH = 0);
    int getCalibrationPointCount() { return numCalibrationPoints; }
    bool hasAcidPoint() { return acidCalibrated; }
    bool hasNeutralPoint() { return neutralCalibrated; }
    bool hasBasePoint() { return baseCalibrated; } 
    // knownPH: nominal pH (e.g., 4.0, 7.0, 10.0)
    // solutionTemp: current temperature of calibration solution
    // solutionRefPH: true pH at reference temp (0 = use defaults)
    
    void saveCalibration();
    void loadCalibration();
    void resetCalibration();
    
    // Calibration age tracking
    int getDaysSinceCalibration();
    bool needsCalibration();      // Returns true if >30 days
    bool isCalibrationExpired();  // Returns true if >60 days
    unsigned long getLastCalibrationTime() { return lastCalibrationTime; }
    
    // Set reference temperature and solution pH values
    void setReferenceTemp(float temp) { refTemp = temp; }
    void setSolutionReference(float acidRef, float neutralRef, float baseRef) {
        acidTrueRef = acidRef;
        neutralTrueRef = neutralRef;
        baseTrueRef = baseRef;
    }
    
    // Get calibration data for display
    float getAcidVoltage() { return acidVoltage; }
    float getNeutralVoltage() { return neutralVoltage; }
    float getBaseVoltage() { return baseVoltage; }
};

#endif
