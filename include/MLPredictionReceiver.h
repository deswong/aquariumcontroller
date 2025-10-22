#ifndef ML_PREDICTION_RECEIVER_H
#define ML_PREDICTION_RECEIVER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/**
 * ML Prediction Receiver
 * 
 * Receives water change predictions from external ML service via MQTT
 * Replaces on-device WaterChangePredictor for reduced ESP32 load
 */
class MLPredictionReceiver {
private:
    PubSubClient* mqttClient;
    
    // Prediction data
    float predictedDaysRemaining;
    float predictedTotalCycleDays;
    float daysSinceLastChange;
    float confidence;
    String modelName;
    float currentTDS;
    float tdsIncreaseRate;
    bool needsChangeSoon;
    unsigned long lastUpdateTime;
    
    // Validity tracking
    bool predictionValid;
    static const unsigned long PREDICTION_TIMEOUT = 86400000; // 24 hours
    
public:
    MLPredictionReceiver();
    
    void begin(PubSubClient* client);
    void processMessage(const char* payload);
    
    // Getters
    float getPredictedDaysRemaining() const { return predictedDaysRemaining; }
    float getPredictedTotalCycleDays() const { return predictedTotalCycleDays; }
    float getDaysSinceLastChange() const { return daysSinceLastChange; }
    float getConfidence() const { return confidence; }
    String getModelName() const { return modelName; }
    float getCurrentTDS() const { return currentTDS; }
    float getTDSIncreaseRate() const { return tdsIncreaseRate; }
    bool needsWaterChangeSoon() const { return needsChangeSoon; }
    unsigned long getLastUpdateTime() const { return lastUpdateTime; }
    
    // Validity checks
    bool isPredictionValid() const;
    bool isPredictionStale() const;
    int getConfidencePercent() const { return (int)(confidence * 100); }
    
    // Status
    String getStatusJSON() const;
    void printStatus() const;
};

#endif // ML_PREDICTION_RECEIVER_H
