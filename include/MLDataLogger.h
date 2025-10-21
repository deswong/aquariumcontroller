#ifndef ML_DATA_LOGGER_H
#define ML_DATA_LOGGER_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include <map>

// ML training sample for PID optimization
struct PIDPerformanceSample {
    unsigned long timestamp;
    
    // Environmental features
    float currentValue;      // Current temperature/pH
    float targetValue;       // Setpoint
    float ambientTemp;       // Room temperature
    uint8_t hourOfDay;       // 0-23
    uint8_t season;          // 0-3 (0=winter, 1=spring, 2=summer, 3=fall)
    float tankVolume;        // Liters
    
    // PID parameters used
    float kp;
    float ki;
    float kd;
    
    // Performance metrics
    float errorMean;         // Average error over window
    float errorVariance;     // Oscillation indicator
    float settlingTime;      // Time to settle in seconds
    float overshoot;         // Maximum overshoot
    float steadyStateError;  // Final steady-state error
    
    // Control behavior
    float averageOutput;     // Average control output
    uint16_t cycleCount;     // Number of control cycles
    
    // Performance score (higher = better)
    float score;
};

// Lookup table entry for quick parameter retrieval
struct PIDGainEntry {
    float kp;
    float ki;
    float kd;
    float avgScore;
    uint16_t sampleCount;
};

// Feature bucket for discretization
struct FeatureBucket {
    float tempRange[2];      // [min, max] temperature
    float ambientRange[2];   // [min, max] ambient
    uint8_t hourRange[2];    // [min, max] hour
    uint8_t season;          // Specific season
};

class MLDataLogger {
private:
    String mlDataPath;
    String lookupTablePath;
    bool spiffsAvailable;
    
    // In-memory lookup table for fast parameter retrieval
    std::map<String, PIDGainEntry> lookupTable;
    
    // Statistics
    uint32_t totalSamples;
    uint32_t samplesThisSession;
    
    // Helper functions
    String generateBucketKey(float temp, float ambient, uint8_t hour, uint8_t season);
    void updateLookupTable(const PIDPerformanceSample& sample);
    float calculatePerformanceScore(const PIDPerformanceSample& sample);
    void saveLookupTableToFile();
    void loadLookupTableFromFile();
    FeatureBucket getBucketForFeatures(float temp, float ambient, uint8_t hour, uint8_t season);

public:
    MLDataLogger();
    ~MLDataLogger();
    
    bool begin();
    
    // Log a PID performance sample
    bool logSample(const PIDPerformanceSample& sample);
    
    // Get optimal PID gains for current conditions
    bool getOptimalGains(float temp, float ambient, uint8_t hour, uint8_t season, 
                         float& kp, float& ki, float& kd, float& confidence);
    
    // Get all samples for export/training
    std::vector<PIDPerformanceSample> getAllSamples(int maxSamples = 1000);
    
    // Export data as CSV for external training
    String exportAsCSV(int maxSamples = 1000);
    
    // Clear all ML data
    void clearData();
    
    // Get statistics
    uint32_t getTotalSamples() { return totalSamples; }
    uint32_t getSamplesThisSession() { return samplesThisSession; }
    size_t getDataFileSize();
    size_t getLookupTableSize() { return lookupTable.size(); }
    
    // Get best performing parameters globally
    bool getBestGlobalGains(float& kp, float& ki, float& kd, float& avgScore);
    
    // Check if enough data exists for ML recommendations
    bool hasMinimumData() { return totalSamples >= 50; }
};

#endif
