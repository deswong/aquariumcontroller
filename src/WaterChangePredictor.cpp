#include "WaterChangePredictor.h"
#include <cmath>
#include <time.h>

WaterChangePredictor::WaterChangePredictor() {
    prefs = new Preferences();
    currentTDS = 0;
    tdsAtLastWaterChange = 0;
    lastWaterChangeTime = 0;
    waterChangeInProgress = false;
    tdsBeforeWaterChange = 0;
    averageTDSIncreasePerDay = 5.0;  // Default estimate
    tdsDegradationRate = 0.0;
    targetTDSThreshold = 400.0;       // Default 400 ppm
    confidenceLevel = 0;
    predictedDaysUntilChange = 0;
    lastPredictionUpdate = 0;
}

WaterChangePredictor::~WaterChangePredictor() {
    delete prefs;
}

void WaterChangePredictor::begin() {
    loadHistory();
    
    // Load last water change time and TDS
    if (prefs->begin("wc-predictor", true)) {
        lastWaterChangeTime = prefs->getULong64("lastWCTime", 0);
        tdsAtLastWaterChange = prefs->getFloat("lastWCTDS", 0);
        targetTDSThreshold = prefs->getFloat("tdsThreshold", 400.0);
        prefs->end();
    }
    
    // Calculate initial degradation if we have history
    if (history.size() >= 2) {
        calculateTDSDegradation();
    }
    
    Serial.println("Water Change Predictor initialized");
    Serial.printf("  History events: %d\n", history.size());
    Serial.printf("  Last water change: %lu days ago\n", getDaysSinceLastChange());
    Serial.printf("  TDS degradation rate: %.2f ppm/day\n", averageTDSIncreasePerDay);
}

void WaterChangePredictor::loadHistory() {
    history.clear();
    
    if (!prefs->begin("wc-history", true)) {
        Serial.println("No water change history found");
        return;
    }
    
    int count = prefs->getInt("count", 0);
    for (int i = 0; i < count && i < MAX_HISTORY; i++) {
        WaterChangeEvent event;
        char key[20];
        
        snprintf(key, sizeof(key), "ts_%d", i);
        event.timestamp = prefs->getULong64(key, 0);
        
        snprintf(key, sizeof(key), "tdsB_%d", i);
        event.tdsBeforeChange = prefs->getFloat(key, 0);
        
        snprintf(key, sizeof(key), "tdsA_%d", i);
        event.tdsAfterChange = prefs->getFloat(key, 0);
        
        snprintf(key, sizeof(key), "vol_%d", i);
        event.volumeChangedPercent = prefs->getFloat(key, 0);
        
        snprintf(key, sizeof(key), "days_%d", i);
        event.daysSinceLastChange = prefs->getFloat(key, 0);
        
        if (event.timestamp > 0) {
            history.push_back(event);
        }
    }
    
    prefs->end();
    Serial.printf("Loaded %d water change events from history\n", history.size());
}

void WaterChangePredictor::saveHistory() {
    if (!prefs->begin("wc-history", false)) {
        Serial.println("ERROR: Failed to save water change history");
        return;
    }
    
    int count = min((int)history.size(), MAX_HISTORY);
    prefs->putInt("count", count);
    
    for (int i = 0; i < count; i++) {
        char key[20];
        WaterChangeEvent& event = history[i];
        
        snprintf(key, sizeof(key), "ts_%d", i);
        prefs->putULong64(key, event.timestamp);
        
        snprintf(key, sizeof(key), "tdsB_%d", i);
        prefs->putFloat(key, event.tdsBeforeChange);
        
        snprintf(key, sizeof(key), "tdsA_%d", i);
        prefs->putFloat(key, event.tdsAfterChange);
        
        snprintf(key, sizeof(key), "vol_%d", i);
        prefs->putFloat(key, event.volumeChangedPercent);
        
        snprintf(key, sizeof(key), "days_%d", i);
        prefs->putFloat(key, event.daysSinceLastChange);
    }
    
    prefs->end();
    Serial.println("Water change history saved");
}

void WaterChangePredictor::startWaterChange() {
    waterChangeInProgress = true;
    tdsBeforeWaterChange = currentTDS;
    Serial.printf("Water change started. TDS before: %.1f ppm\n", tdsBeforeWaterChange);
}

void WaterChangePredictor::completeWaterChange(float volumePercent) {
    if (!waterChangeInProgress) {
        Serial.println("WARNING: completeWaterChange called but no water change in progress");
        return;
    }
    
    waterChangeInProgress = false;
    
    // Create new event
    WaterChangeEvent event;
    event.timestamp = time(nullptr);  // Use NTP time instead of millis
    event.tdsBeforeChange = tdsBeforeWaterChange;
    event.tdsAfterChange = currentTDS;
    event.volumeChangedPercent = volumePercent;
    
    // Calculate days since last change
    if (lastWaterChangeTime > 0) {
        event.daysSinceLastChange = (event.timestamp - lastWaterChangeTime) / 86400.0;
    } else {
        event.daysSinceLastChange = 0;
    }
    
    // Add to history
    history.push_back(event);
    
    // Keep only MAX_HISTORY events
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin());
    }
    
    // Update state
    lastWaterChangeTime = event.timestamp;
    tdsAtLastWaterChange = currentTDS;
    
    // Save to NVS
    saveHistory();
    
    if (prefs->begin("wc-predictor", false)) {
        prefs->putULong64("lastWCTime", lastWaterChangeTime);
        prefs->putFloat("lastWCTDS", tdsAtLastWaterChange);
        prefs->end();
    }
    
    // Recalculate degradation rate
    calculateTDSDegradation();
    
    Serial.printf("Water change completed: %.1f%% changed, TDS: %.1f → %.1f ppm\n",
                  volumePercent, tdsBeforeWaterChange, currentTDS);
    Serial.printf("Days since last change: %.1f, TDS increase: %.1f ppm\n",
                  event.daysSinceLastChange, tdsBeforeWaterChange - tdsAtLastWaterChange);
}

void WaterChangePredictor::cancelWaterChange() {
    waterChangeInProgress = false;
    Serial.println("Water change cancelled");
}

void WaterChangePredictor::updateTDS(float tds) {
    currentTDS = tds;
    
    // Update prediction periodically
    unsigned long now = millis();
    if (now - lastPredictionUpdate >= PREDICTION_UPDATE_INTERVAL) {
        updatePrediction();
        lastPredictionUpdate = now;
    }
}

void WaterChangePredictor::calculateTDSDegradation() {
    if (history.size() < 2) {
        confidenceLevel = 0;
        Serial.println("Not enough history for TDS degradation calculation");
        return;
    }
    
    // Calculate average TDS increase per day using linear regression
    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = 0;
    
    for (const auto& event : history) {
        if (event.daysSinceLastChange > 0) {
            float tdsDelta = event.tdsBeforeChange - event.tdsAfterChange;
            float days = event.daysSinceLastChange;
            float tdsPerDay = tdsDelta / days;
            
            sumX += days;
            sumY += tdsPerDay;
            sumXY += days * tdsPerDay;
            sumX2 += days * days;
            n++;
        }
    }
    
    if (n > 1) {
        // Linear regression slope
        averageTDSIncreasePerDay = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
        
        // Ensure positive and reasonable
        averageTDSIncreasePerDay = constrain(averageTDSIncreasePerDay, 0.5, 50.0);
        
        // Calculate confidence based on consistency of data
        float variance = 0;
        for (const auto& event : history) {
            if (event.daysSinceLastChange > 0) {
                float tdsDelta = event.tdsBeforeChange - event.tdsAfterChange;
                float tdsPerDay = tdsDelta / event.daysSinceLastChange;
                float diff = tdsPerDay - averageTDSIncreasePerDay;
                variance += diff * diff;
            }
        }
        variance /= n;
        
        // Higher confidence if variance is low
        float stdDev = sqrt(variance);
        confidenceLevel = constrain(100 - (int)(stdDev * 10), 10, 95);
    } else {
        averageTDSIncreasePerDay = 5.0;  // Default fallback
        confidenceLevel = 10;
    }
    
    Serial.printf("TDS degradation calculated: %.2f ppm/day (confidence: %d%%)\n",
                  averageTDSIncreasePerDay, confidenceLevel);
}

void WaterChangePredictor::updatePrediction() {
    if (averageTDSIncreasePerDay <= 0 || targetTDSThreshold <= 0) {
        predictedDaysUntilChange = 0;
        return;
    }
    
    // Calculate TDS delta from last water change
    float tdsSinceLastChange = currentTDS - tdsAtLastWaterChange;
    
    // Calculate how much TDS increase until threshold
    float tdsUntilThreshold = targetTDSThreshold - currentTDS;
    
    if (tdsUntilThreshold <= 0) {
        predictedDaysUntilChange = 0;  // Already exceeded
        return;
    }
    
    // Predict days until threshold based on current rate
    predictedDaysUntilChange = tdsUntilThreshold / averageTDSIncreasePerDay;
    
    // Ensure reasonable prediction
    predictedDaysUntilChange = constrain(predictedDaysUntilChange, 0, 90);
}

float WaterChangePredictor::getPredictedDaysUntilChange() {
    return predictedDaysUntilChange;
}

float WaterChangePredictor::getCurrentTDSIncreaseRate() {
    return averageTDSIncreasePerDay;
}

int WaterChangePredictor::getConfidenceLevel() {
    return confidenceLevel;
}

bool WaterChangePredictor::needsWaterChange() {
    return currentTDS >= targetTDSThreshold;
}

int WaterChangePredictor::getHistoryCount() {
    return history.size();
}

WaterChangeEvent WaterChangePredictor::getHistoryEvent(int index) {
    if (index >= 0 && index < history.size()) {
        return history[index];
    }
    return WaterChangeEvent{0, 0, 0, 0, 0};
}

void WaterChangePredictor::clearHistory() {
    history.clear();
    saveHistory();
    confidenceLevel = 0;
    Serial.println("Water change history cleared");
}

void WaterChangePredictor::setTargetTDSThreshold(float threshold) {
    targetTDSThreshold = threshold;
    
    if (prefs->begin("wc-predictor", false)) {
        prefs->putFloat("tdsThreshold", threshold);
        prefs->end();
    }
    
    updatePrediction();
    Serial.printf("TDS threshold set to %.1f ppm\n", threshold);
}

float WaterChangePredictor::getTargetTDSThreshold() {
    return targetTDSThreshold;
}

float WaterChangePredictor::getAverageDaysBetweenChanges() {
    if (history.size() < 2) return 0;
    
    float total = 0;
    int count = 0;
    for (const auto& event : history) {
        if (event.daysSinceLastChange > 0) {
            total += event.daysSinceLastChange;
            count++;
        }
    }
    
    return count > 0 ? total / count : 0;
}

float WaterChangePredictor::getAverageTDSIncrease() {
    if (history.size() < 1) return 0;
    
    float total = 0;
    int count = 0;
    for (const auto& event : history) {
        total += event.tdsBeforeChange - event.tdsAfterChange;
        count++;
    }
    
    return count > 0 ? total / count : 0;
}

float WaterChangePredictor::getTDSAtLastChange() {
    return tdsAtLastWaterChange;
}

unsigned long WaterChangePredictor::getDaysSinceLastChange() {
    if (lastWaterChangeTime == 0) return 0;
    unsigned long now = time(nullptr);  // Use NTP time instead of millis
    return (now - lastWaterChangeTime) / 86400;
}

String WaterChangePredictor::getStatusJSON() {
    String json = "{";
    json += "\"predicted_days\":" + String(predictedDaysUntilChange, 1) + ",";
    json += "\"tds_increase_rate\":" + String(averageTDSIncreasePerDay, 2) + ",";
    json += "\"confidence\":" + String(confidenceLevel) + ",";
    json += "\"needs_change\":" + String(needsWaterChange() ? "true" : "false") + ",";
    json += "\"threshold\":" + String(targetTDSThreshold, 1) + ",";
    json += "\"days_since_last\":" + String(getDaysSinceLastChange()) + ",";
    json += "\"avg_days_between\":" + String(getAverageDaysBetweenChanges(), 1) + ",";
    json += "\"history_count\":" + String(getHistoryCount());
    json += "}";
    return json;
}
