#ifndef WATER_CHANGE_PREDICTOR_H
#define WATER_CHANGE_PREDICTOR_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// Structure to store water change history
struct WaterChangeEvent {
    unsigned long timestamp;      // Time of water change (epoch seconds)
    float tdsBeforeChange;        // TDS before water change
    float tdsAfterChange;         // TDS after water change
    float volumeChangedPercent;   // % of water changed
    float daysSinceLastChange;    // Days since previous water change
};

class WaterChangePredictor {
private:
    Preferences* prefs;
    
    // Current state tracking
    float currentTDS;
    float tdsAtLastWaterChange;
    unsigned long lastWaterChangeTime;  // Epoch seconds
    bool waterChangeInProgress;
    float tdsBeforeWaterChange;
    
    // Historical data
    std::vector<WaterChangeEvent> history;
    static const int MAX_HISTORY = 20;  // Keep last 20 water changes
    
    // Learning parameters
    float averageTDSIncreasePerDay;     // ppm/day
    float tdsDegradationRate;            // Exponential decay rate
    float targetTDSThreshold;            // TDS level that triggers water change
    int confidenceLevel;                 // 0-100% confidence in prediction
    
    // Prediction parameters
    float predictedDaysUntilChange;
    unsigned long lastPredictionUpdate;
    static const unsigned long PREDICTION_UPDATE_INTERVAL = 3600000; // 1 hour
    
    void loadHistory();
    void saveHistory();
    void calculateTDSDegradation();
    void updatePrediction();
    float calculateLinearRegression();
    float calculateExponentialFit();
    
public:
    WaterChangePredictor();
    ~WaterChangePredictor();
    
    void begin();
    
    // Water change tracking
    void startWaterChange();                    // Call when water change begins
    void completeWaterChange(float volumePercent); // Call when water change completes
    void cancelWaterChange();                   // Call if water change cancelled
    
    // TDS monitoring
    void updateTDS(float tds);                  // Call regularly with current TDS
    
    // Predictions
    float getPredictedDaysUntilChange();        // Get prediction
    float getCurrentTDSIncreaseRate();          // ppm/day
    int getConfidenceLevel();                   // 0-100%
    bool needsWaterChange();                    // True if TDS exceeded threshold
    
    // History
    int getHistoryCount();
    WaterChangeEvent getHistoryEvent(int index);
    void clearHistory();
    
    // Configuration
    void setTargetTDSThreshold(float threshold);
    float getTargetTDSThreshold();
    
    // Statistics
    float getAverageDaysBetweenChanges();
    float getAverageTDSIncrease();
    float getTDSAtLastChange();
    unsigned long getDaysSinceLastChange();
    
    // JSON status
    String getStatusJSON();
};

#endif // WATER_CHANGE_PREDICTOR_H
