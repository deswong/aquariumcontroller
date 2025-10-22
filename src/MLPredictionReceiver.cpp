#include "MLPredictionReceiver.h"

MLPredictionReceiver::MLPredictionReceiver() {
    mqttClient = nullptr;
    predictedDaysRemaining = 0.0f;
    predictedTotalCycleDays = 0.0f;
    daysSinceLastChange = 0.0f;
    confidence = 0.0f;
    modelName = "unknown";
    currentTDS = 0.0f;
    tdsIncreaseRate = 0.0f;
    needsChangeSoon = false;
    lastUpdateTime = 0;
    predictionValid = false;
}

void MLPredictionReceiver::begin(PubSubClient* client) {
    mqttClient = client;
    Serial.println("[ML] Prediction receiver initialized");
}

void MLPredictionReceiver::processMessage(const char* payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.printf("[ML] Failed to parse prediction JSON: %s\n", error.c_str());
        return;
    }
    
    // Extract prediction data
    predictedDaysRemaining = doc["predicted_days_remaining"] | 0.0f;
    predictedTotalCycleDays = doc["predicted_total_cycle_days"] | 0.0f;
    daysSinceLastChange = doc["days_since_last_change"] | 0.0f;
    confidence = doc["confidence"] | 0.0f;
    modelName = doc["model"] | "unknown";
    currentTDS = doc["current_tds"] | 0.0f;
    tdsIncreaseRate = doc["tds_increase_rate"] | 0.0f;
    needsChangeSoon = doc["needs_change_soon"] | false;
    
    lastUpdateTime = millis();
    predictionValid = true;
    
    Serial.printf("[ML] Prediction received: %.1f days remaining (confidence: %.0f%%)\n", 
                 predictedDaysRemaining, confidence * 100);
    Serial.printf("[ML] Model: %s, TDS rate: %.2f ppm/day\n", 
                 modelName.c_str(), tdsIncreaseRate);
}

bool MLPredictionReceiver::isPredictionValid() const {
    return predictionValid && !isPredictionStale();
}

bool MLPredictionReceiver::isPredictionStale() const {
    if (!predictionValid) return true;
    return (millis() - lastUpdateTime) > PREDICTION_TIMEOUT;
}

String MLPredictionReceiver::getStatusJSON() const {
    StaticJsonDocument<384> doc;
    
    doc["predicted_days_remaining"] = predictedDaysRemaining;
    doc["predicted_total_cycle_days"] = predictedTotalCycleDays;
    doc["days_since_last_change"] = daysSinceLastChange;
    doc["confidence"] = confidence;
    doc["confidence_percent"] = getConfidencePercent();
    doc["model"] = modelName;
    doc["current_tds"] = currentTDS;
    doc["tds_increase_rate"] = tdsIncreaseRate;
    doc["needs_change_soon"] = needsChangeSoon;
    doc["valid"] = isPredictionValid();
    doc["stale"] = isPredictionStale();
    doc["last_update"] = lastUpdateTime / 1000;
    
    String output;
    serializeJson(doc, output);
    return output;
}

void MLPredictionReceiver::printStatus() const {
    Serial.println("\n===== ML Prediction Status =====");
    Serial.printf("Days Remaining: %.1f\n", predictedDaysRemaining);
    Serial.printf("Total Cycle Days: %.1f\n", predictedTotalCycleDays);
    Serial.printf("Days Since Last Change: %.1f\n", daysSinceLastChange);
    Serial.printf("Confidence: %.0f%%\n", confidence * 100);
    Serial.printf("Model: %s\n", modelName.c_str());
    Serial.printf("Current TDS: %.0f ppm\n", currentTDS);
    Serial.printf("TDS Increase Rate: %.2f ppm/day\n", tdsIncreaseRate);
    Serial.printf("Needs Change Soon: %s\n", needsChangeSoon ? "YES" : "NO");
    Serial.printf("Valid: %s\n", isPredictionValid() ? "YES" : "NO");
    Serial.printf("Stale: %s\n", isPredictionStale() ? "YES" : "NO");
    Serial.printf("Last Update: %lu seconds ago\n", (millis() - lastUpdateTime) / 1000);
    Serial.println("================================\n");
}
