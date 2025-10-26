#ifndef PATTERN_LEARNER_H
#define PATTERN_LEARNER_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// Represents a detected anomaly
struct Anomaly {
    unsigned long timestamp;
    String type;              // "temperature", "ph", "tds", "combined"
    float actualValue;
    float expectedValue;
    float deviation;          // Sigma deviation (how many std devs away)
    String severity;          // "low", "medium", "high", "critical"
};

// Daily/seasonal statistics
struct SeasonalStats {
    float avgAmbientTemp;     // Last 7 days
    float avgWaterTemp;
    float avgPH;
    float avgTDS;
    int daysCollected;
    String season;            // "winter", "spring", "summer", "fall"
};

// Pattern learning configuration
struct PatternConfig {
    bool enabled;
    int minSamplesForAnomaly; // Minimum samples before detecting anomalies
    float tempAnomalyThreshold; // Sigma multiplier (2.0 = 2 std devs)
    float phAnomalyThreshold;
    float tdsAnomalyThreshold;
    bool autoSeasonalAdapt;   // Auto-adjust PID based on season
    bool alertOnAnomaly;      // Send MQTT alerts
    int maxAnomalyHistory;    // Max anomalies to store
};

class PatternLearner {
private:
    // Hourly patterns (24 hours)
    float hourlyTempPattern[24];
    float hourlyTempStdDev[24];
    float hourlyPHPattern[24];
    float hourlyPHStdDev[24];
    float hourlyTDSPattern[24];
    float hourlyTDSStdDev[24];
    float hourlyAmbientPattern[24];
    int hourlySampleCounts[24];
    
    // Weekly patterns (7 days)
    float dailyTempPattern[7];
    float dailyPHPattern[7];
    float dailyTDSPattern[7];
    int dailySampleCounts[7];
    
    // Seasonal tracking (rolling 7-day averages)
    float ambientTempHistory[168]; // 7 days * 24 hours
    int ambientHistoryIndex;
    
    // Anomaly detection
    std::vector<Anomaly> anomalyHistory;
    unsigned long lastAnomalyCheck;
    int totalAnomaliesDetected;
    
    // Learning state
    unsigned long learningStartTime;
    int totalSamples;
    bool patternsEstablished;
    
    // Seasonal adaptation
    SeasonalStats currentSeason;
    String lastDetectedSeason;
    
    // Configuration
    PatternConfig config;
    
    // Storage
    Preferences* prefs;
    
    // Helper methods
    void updateHourlyPattern(int hour, float temp, float ph, float tds, float ambient);
    void updateDailyPattern(int dayOfWeek, float temp, float ph, float tds);
    void updateSeasonalStats();
    float calculateStdDev(float values[], int count, float mean);
    float calculateMean(float values[], int count);
    String detectSeason(float avgAmbient);
    float getSigmaDeviation(float actual, float expected, float stdDev);
    String getSeverity(float sigma);
    void pruneAnomalyHistory();
    
public:
    PatternLearner();
    ~PatternLearner();
    
    // Initialization
    void begin();
    void loadConfig();
    void saveConfig();
    void loadPatterns();
    void savePatterns();
    void reset();
    
    // Learning
    void update(float temp, float ph, float tds, float ambientTemp);
    void learnPattern(int hour, int dayOfWeek, float temp, float ph, float tds, float ambient);
    
    // Anomaly detection
    bool checkForAnomalies(float temp, float ph, float tds, float ambient);
    Anomaly detectTemperatureAnomaly(int hour, float temp);
    Anomaly detectPHAnomaly(int hour, float ph);
    Anomaly detectTDSAnomaly(int hour, float tds);
    bool isAnomaly(float actual, float expected, float stdDev, float threshold);
    
    // Pattern queries
    float getExpectedTemp(int hour);
    float getExpectedPH(int hour);
    float getExpectedTDS(int hour);
    float getExpectedAmbient(int hour);
    float getTempStdDev(int hour);
    float getPHStdDev(int hour);
    float getTDSStdDev(int hour);
    
    // Seasonal adaptation
    SeasonalStats getSeasonalStats();
    String getCurrentSeason();
    void setCurrentSeason(uint8_t seasonNum); // Set meteorological season (0=Spring, 1=Summer, 2=Autumn, 3=Winter)
    bool shouldAdaptForSeason();
    void getSeasonalPIDMultipliers(float& kpMult, float& kiMult, float& kdMult);
    
    // Anomaly history
    std::vector<Anomaly> getRecentAnomalies(int count);
    int getAnomalyCount();
    void clearAnomalyHistory();
    
    // Statistics
    int getSampleCount(int hour);
    int getTotalSamples();
    bool arePatternsEstablished();
    int getDaysLearning();
    float getPatternConfidence(); // 0.0 to 1.0
    
    // Configuration
    void setConfig(PatternConfig newConfig);
    PatternConfig getConfig();
    void setEnabled(bool enabled);
    bool isEnabled();
    void setAnomalyThresholds(float temp, float ph, float tds);
    void setAutoSeasonalAdapt(bool enable);
    
    // Predictions
    float predictTempChange(int hoursAhead);
    bool predictAnomalyLikelihood(int hoursAhead);
    
    // Debug/Status
    void printPatternSummary();
    void printAnomalies();
    String getStatusJSON();
};

#endif // PATTERN_LEARNER_H
