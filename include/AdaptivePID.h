#ifndef ADAPTIVE_PID_H
#define ADAPTIVE_PID_H

#include <Arduino.h>
#include <Preferences.h>

// Forward declaration
class MLDataLogger;

// Forward declaration for ISR
void IRAM_ATTR onControlTimerISR();

// ESP32-S3 Phase 1 Enhancements: PSRAM support
#ifdef BOARD_HAS_PSRAM
#define PID_USE_PSRAM 1
#define PID_HISTORY_SIZE 1000  // Large history buffer in PSRAM
#else
#define PID_USE_PSRAM 0
#define PID_HISTORY_SIZE 100   // Smaller history buffer in SRAM
#endif

class AdaptivePID {
    friend void onControlTimerISR();  // Allow ISR to access private members
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
    
    // Learning/adaptation - PHASE 1: PSRAM-based history buffers
    float* errorHistory;      // Dynamic allocation (PSRAM if available)
    float* outputHistory;     // Track outputs for ML analysis
    float* inputHistory;      // Track inputs for ML analysis
    uint16_t historySize;     // Configurable size based on available memory
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
    
    // PHASE 1: ML Parameter Cache (NVS-based)
    struct MLParamCache {
        float ambientTemp;
        uint8_t hour;
        uint8_t season;
        float kp_ml, ki_ml, kd_ml;
        float confidence;
        uint32_t lastUpdateTime;
        bool valid;
    };
    MLParamCache mlCache;
    uint32_t mlCacheValidityMs;     // Cache lifetime (default: 5 minutes)
    uint32_t mlCacheHits;           // Statistics
    uint32_t mlCacheMisses;
    
    // ML Model Versioning and Metadata
    struct MLModelMetadata {
        uint32_t version;              // Model version number
        char trainingDate[32];         // ISO8601 format
        uint32_t trainingSamples;      // Number of samples used for training
        float validationScore;         // Performance metric (RMSE, accuracy, etc.)
        float minExpectedError;        // Expected minimum error rate
        float maxExpectedError;        // Expected maximum error rate
        char checksum[33];             // MD5 checksum of model file
        bool isValid;                  // Model loaded and validated successfully
    };
    MLModelMetadata mlModelMeta;
    void loadMLModelMetadata();
    void saveMLModelMetadata();
    bool validateMLModel();
    
    // PHASE 1: Hardware Timer Support
    hw_timer_t* controlTimer;
    bool useHardwareTimer;
    uint32_t controlPeriodUs;       // Microseconds
    volatile bool computeReady;
    static void IRAM_ATTR onControlTimer(void* arg);
    
    bool enableProfiling;

public:
    // PHASE 1: Performance Profiling
    struct PerformanceProfile {
        uint32_t computeTimeUs;      // Current compute time (microseconds)
        uint32_t maxComputeTimeUs;
        uint32_t minComputeTimeUs;
        uint32_t avgComputeTimeUs;
        uint32_t computeCount;
        uint32_t overrunCount;       // Missed deadlines
        float cpuUsagePercent;
    };

private:
    PerformanceProfile profile;
    
    // PHASE 2: Dual-Core ML Processing
    TaskHandle_t mlTaskHandle;
    SemaphoreHandle_t paramMutex;          // Protect PID parameters
    SemaphoreHandle_t mlWakeupSemaphore;   // Signal ML task
    bool useDualCore;
    volatile bool mlTaskRunning;
    struct MLTaskData {
        float ambientTemp;
        uint8_t hour;
        uint8_t season;
        float tankVolume;
        float tds;  // Added for TDS influence
        bool needsUpdate;
    };
    MLTaskData mlTaskData;
    static void mlAdaptationTask(void* pvParameters);
    
    // PHASE 2: Kalman Filter for sensor smoothing
    struct KalmanFilter {
        float x;          // State estimate
        float p;          // Error covariance
        float q;          // Process noise
        float r;          // Measurement noise
        bool initialized;
    };
    KalmanFilter kalman;
    bool useKalman;
    float kalmanFilterInput(float measurement);
    void initKalmanFilter(float processNoise, float measurementNoise);
    
    // PHASE 3: Bumpless Transfer (smooth parameter transitions)
    struct ParameterTransition {
        float kp_target, ki_target, kd_target;
        float kp_start, ki_start, kd_start;
        uint32_t transitionStartTime;
        uint32_t transitionDuration;  // ms
        bool inTransition;
    };
    ParameterTransition paramTransition;
    void updateParameterTransition();
    
    bool useHealthMonitoring;
    void updateHealthMetrics();

public:
    // PHASE 3: Health Monitoring
    struct HealthMetrics {
        bool outputStuck;              // Output not changing despite error
        bool persistentHighError;      // Error remains high for extended period
        bool outputSaturation;         // Output at limits too often
        uint32_t lastHealthCheck;
        uint32_t stuckOutputCount;
        uint32_t saturationCount;
        float lastOutputValue;
        bool hasError;
        String errorMessage;
    };

private:
    HealthMetrics health;
    
    // PHASE 3: Predictive Feed-Forward based on TDS/sensor data
    struct FeedForwardModel {
        float tdsInfluence;            // How TDS affects target (cooling effect from water changes)
        float ambientTempInfluence;    // How ambient temp affects heat loss
        float phInfluence;             // How pH affects CO2 dissolution (for CO2 control)
        bool enabled;
        // Baseline values for computing deltas
        float baselineTDS;
        float baselineAmbientTemp;
        float baselinePH;
        // Last computed contributions (for API reporting)
        float lastTdsContribution;
        float lastAmbientContribution;
        float lastPhContribution;
        float lastTotalContribution;
    };
    FeedForwardModel feedForward;
    float computeFeedForward(float tds, float ambientTemp, float ph);
    
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
    
    // PHASE 1: Internal helpers
    void saveCacheToNVS();
    void loadCacheFromNVS();
    void allocateHistoryBuffers();
    void freeHistoryBuffers();

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
    
    // PHASE 1: Hardware Timer Control
    void enableHardwareTimer(uint32_t periodMs);
    void disableHardwareTimer();
    float computeIfReady(float input);  // Only compute if timer triggered
    bool isComputeReady() { return computeReady; }
    
    // PHASE 1: Performance Profiling
    void enablePerformanceProfiling(bool enable);
    String getProfileReport();
    PerformanceProfile getProfile() { return profile; }
    float getCPUUsage() { return profile.cpuUsagePercent; }
    
    // PHASE 1: ML Cache Statistics
    float getMLCacheHitRate() { 
        uint32_t total = mlCacheHits + mlCacheMisses;
        return (total > 0) ? ((float)mlCacheHits / (float)total * 100.0f) : 0.0f;
    }
    uint32_t getMLCacheHits() { return mlCacheHits; }
    uint32_t getMLCacheMisses() { return mlCacheMisses; }
    void resetMLCacheStats() { mlCacheHits = 0; mlCacheMisses = 0; }
    
    // History buffer access (for ML training)
    uint16_t getHistorySize() { return historySize; }
    float getErrorHistory(uint16_t index);
    float getOutputHistory(uint16_t index);
    float getInputHistory(uint16_t index);
    
    // PHASE 2: Dual-Core ML Control
    void enableDualCoreML(bool enable);
    bool isDualCoreEnabled() { return useDualCore; }
    void triggerMLAdaptation(float ambientTemp, uint8_t hour, uint8_t season, float tankVolume = 100.0f, float tds = 0.0f);
    
    // PHASE 2: Kalman Filter Control
    void enableKalmanFilter(bool enable, float processNoise = 0.001f, float measurementNoise = 0.1f);
    bool isKalmanEnabled() { return useKalman; }
    float getKalmanState() { return kalman.x; }
    float getKalmanCovariance() { return kalman.p; }
    bool isKalmanInitialized() { return kalman.initialized; }
    
    // PHASE 3: Bumpless Transfer
    void setParametersSmooth(float p, float i, float d, uint32_t durationMs = 30000);
    bool isInTransition() { return paramTransition.inTransition; }
    
    // PHASE 3: Health Monitoring
    void enableHealthMonitoring(bool enable);
    bool isHealthMonitoringEnabled() { return useHealthMonitoring; }
    HealthMetrics getHealthMetrics() { return health; }
    String getHealthReport();
    
    // PHASE 3: Predictive Feed-Forward with sensor data
    void enableFeedForwardModel(bool enable, float tdsInfluence = 0.1f, float ambientInfluence = 0.3f, float phInfluence = 0.2f);
    bool isFeedForwardEnabled() { return feedForward.enabled; }
    float getFeedForwardTDS() { return feedForward.lastTdsContribution; }
    float getFeedForwardAmbient() { return feedForward.lastAmbientContribution; }
    float getFeedForwardPH() { return feedForward.lastPhContribution; }
    float getFeedForwardTotal() { return feedForward.lastTotalContribution; }
    float computeWithSensorContext(float input, float dt, float ambientTemp, uint8_t hour, uint8_t season, 
                                    float tankVolume, float tds, float ph);
    
    // ML Model Metadata Access
    const MLModelMetadata& getMLModelMetadata() const { return mlModelMeta; }
    bool isMLModelValid() const { return mlModelMeta.isValid; }
    uint32_t getMLModelVersion() const { return mlModelMeta.version; }
    String getMLModelInfo() const;
};

#endif
