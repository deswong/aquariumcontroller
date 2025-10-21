#ifndef ADAPTIVE_PID_H
#define ADAPTIVE_PID_H

#include <Arduino.h>
#include <Preferences.h>

// Forward declaration
class MLDataLogger;

class AdaptivePID {
private:
    float kp, ki, kd;
    float target;
    float outputMin, outputMax;
    float integral;
    float lastError;
    float lastOutput;
    float lastInput;
    unsigned long lastTime;
    
    // Safety limits
    float absoluteMax;
    float safetyThreshold;
    bool emergencyStop;
    unsigned long emergencyStopTime;
    
    // Advanced features
    bool useDerivativeFilter;
    float derivativeFilterCoeff; // 0.0 to 1.0, higher = more filtering
    float filteredDerivative;
    
    bool useSetpointRamping;
    float rampRate; // Units per second
    float effectiveTarget;
    
    bool useIntegralWindupPrevention;
    float integralMax;
    
    bool useFeedForward;
    float feedForwardGain;
    
    // Auto-tuning parameters
    bool autoTuning;
    float relayAmplitude;
    unsigned long tuneStartTime;
    float oscillationAmplitude;
    unsigned long oscillationPeriod;
    int oscillationCount;
    float lastPeakValue;
    unsigned long lastPeakTime;
    bool peakDetected;
    
    // Learning/adaptation
    float errorHistory[100];
    int errorIndex;
    float performanceMetric;
    bool useMLAdaptation;
    
    // Performance monitoring for ML
    unsigned long performanceWindowStart;
    float performanceWindowErrorSum;
    float performanceWindowErrorSqSum;
    int performanceWindowSamples;
    float performanceWindowOutputSum;
    
    // Performance monitoring
    unsigned long settlingStartTime;
    bool isSettled;
    float maxOvershoot;
    float steadyStateError;
    unsigned long settlingTime;
    int controlActions;
    
    // ML integration
    MLDataLogger* mlLogger;
    bool mlEnabled;
    float mlConfidence;
    
    // Storage
    Preferences* prefs;
    String namespace_name;
    
    void adaptParameters();
    void adaptParametersWithML(float ambientTemp, uint8_t hour, uint8_t season);
    void logPerformanceToML(float ambientTemp, uint8_t hour, uint8_t season, float tankVolume);
    void detectOscillation(float error);
    void calculateAutoTuneParams();
    void updatePerformanceMetrics(float error, float input);
    float computeDerivative(float input, float dt);
    void updateSetpointRamp(float dt);

public:
    AdaptivePID(String namespaceName, float initialKp = 2.0, float initialKi = 0.1, float initialKd = 1.0);
    ~AdaptivePID();
    
    void begin();
    void setMLLogger(MLDataLogger* logger);
    void setTarget(float setpoint);
    void setSafetyLimits(float maxValue, float safetyMargin);
    void setOutputLimits(float min, float max);
    
    // Main compute function with ML support
    float compute(float input, float dt);
    float computeWithContext(float input, float dt, float ambientTemp, uint8_t hour, uint8_t season, float tankVolume = 0);
    
    void reset();
    
    // Advanced PID features
    void enableDerivativeFilter(bool enable, float coefficient = 0.7);
    void enableSetpointRamping(bool enable, float rampRate = 1.0);
    void enableIntegralWindupPrevention(bool enable, float maxIntegral = 100.0);
    void enableFeedForward(bool enable, float gain = 0.5);
    
    // ML features
    void enableMLAdaptation(bool enable);
    bool isMLEnabled() { return mlEnabled && mlLogger != nullptr; }
    float getMLConfidence() { return mlConfidence; }
    
    // Auto-tuning
    void startAutoTune(float amplitude = 1.0);
    void stopAutoTune();
    bool isAutoTuning() { return autoTuning; }
    
    // Learning
    void enableAdaptation(bool enable);
    
    // Safety
    bool isEmergencyStopped() { return emergencyStop; }
    void clearEmergencyStop();
    
    // Parameter management
    void setParameters(float p, float i, float d);
    void getParameters(float& p, float& i, float& d);
    void saveParameters();
    void loadParameters();
    
    // Performance metrics
    float getSettlingTime() { return settlingTime / 1000.0; } // seconds
    float getMaxOvershoot() { return maxOvershoot; }
    float getSteadyStateError() { return steadyStateError; }
    bool isSystemSettled() { return isSettled; }
    int getControlActions() { return controlActions; }
    
    // Status
    float getTarget() { return target; }
    float getEffectiveTarget() { return effectiveTarget; }
    float getError() { return lastError; }
    float getOutput() { return lastOutput; }
    float getIntegral() { return integral; }
    float getPerformanceMetric() { return performanceMetric; }
};

#endif
