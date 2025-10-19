#include "PatternLearner.h"
#include <math.h>
#include <time.h>

PatternLearner::PatternLearner() 
    : lastAnomalyCheck(0), totalAnomaliesDetected(0), 
      learningStartTime(0), totalSamples(0), patternsEstablished(false),
      ambientHistoryIndex(0), lastDetectedSeason("unknown") {
    
    prefs = new Preferences();
    
    // Initialize patterns to neutral values
    for (int i = 0; i < 24; i++) {
        hourlyTempPattern[i] = 25.0;
        hourlyTempStdDev[i] = 0.5;
        hourlyPHPattern[i] = 7.0;
        hourlyPHStdDev[i] = 0.2;
        hourlyTDSPattern[i] = 200.0;
        hourlyTDSStdDev[i] = 20.0;
        hourlyAmbientPattern[i] = 23.0;
        hourlySampleCounts[i] = 0;
    }
    
    for (int i = 0; i < 7; i++) {
        dailyTempPattern[i] = 25.0;
        dailyPHPattern[i] = 7.0;
        dailyTDSPattern[i] = 200.0;
        dailySampleCounts[i] = 0;
    }
    
    for (int i = 0; i < 168; i++) {
        ambientTempHistory[i] = 23.0;
    }
    
    // Default configuration
    config.enabled = true;
    config.minSamplesForAnomaly = 7; // Need at least a week of data
    config.tempAnomalyThreshold = 2.5; // 2.5 sigma
    config.phAnomalyThreshold = 2.0;   // 2.0 sigma
    config.tdsAnomalyThreshold = 2.5;  // 2.5 sigma
    config.autoSeasonalAdapt = true;
    config.alertOnAnomaly = true;
    config.maxAnomalyHistory = 100;
    
    // Initialize seasonal stats
    currentSeason.avgAmbientTemp = 23.0;
    currentSeason.avgWaterTemp = 25.0;
    currentSeason.avgPH = 7.0;
    currentSeason.avgTDS = 200.0;
    currentSeason.daysCollected = 0;
    currentSeason.season = "unknown";
}

PatternLearner::~PatternLearner() {
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

void PatternLearner::begin() {
    Serial.println("Initializing Pattern Learner...");
    
    learningStartTime = millis();
    loadConfig();
    loadPatterns();
    
    if (config.enabled) {
        Serial.println("Pattern learning ENABLED");
        Serial.printf("Anomaly thresholds: Temp=%.1fσ, pH=%.1fσ, TDS=%.1fσ\n",
                     config.tempAnomalyThreshold, config.phAnomalyThreshold, 
                     config.tdsAnomalyThreshold);
    } else {
        Serial.println("Pattern learning DISABLED");
    }
}

void PatternLearner::loadConfig() {
    if (!prefs->begin("pattern-cfg", false)) {
        Serial.println("Failed to open pattern config namespace");
        return;
    }
    
    config.enabled = prefs->getBool("enabled", true);
    config.minSamplesForAnomaly = prefs->getInt("minSamples", 7);
    config.tempAnomalyThreshold = prefs->getFloat("tempThresh", 2.5);
    config.phAnomalyThreshold = prefs->getFloat("phThresh", 2.0);
    config.tdsAnomalyThreshold = prefs->getFloat("tdsThresh", 2.5);
    config.autoSeasonalAdapt = prefs->getBool("autoSeason", true);
    config.alertOnAnomaly = prefs->getBool("alertAnomaly", true);
    config.maxAnomalyHistory = prefs->getInt("maxHistory", 100);
    
    prefs->end();
}

void PatternLearner::saveConfig() {
    if (!prefs->begin("pattern-cfg", false)) {
        Serial.println("Failed to save pattern config");
        return;
    }
    
    prefs->putBool("enabled", config.enabled);
    prefs->putInt("minSamples", config.minSamplesForAnomaly);
    prefs->putFloat("tempThresh", config.tempAnomalyThreshold);
    prefs->putFloat("phThresh", config.phAnomalyThreshold);
    prefs->putFloat("tdsThresh", config.tdsAnomalyThreshold);
    prefs->putBool("autoSeason", config.autoSeasonalAdapt);
    prefs->putBool("alertAnomaly", config.alertOnAnomaly);
    prefs->putInt("maxHistory", config.maxAnomalyHistory);
    
    prefs->end();
    Serial.println("Pattern config saved");
}

void PatternLearner::loadPatterns() {
    if (!prefs->begin("patterns", true)) {
        Serial.println("No saved patterns found - starting fresh");
        return;
    }
    
    // Load hourly patterns
    for (int i = 0; i < 24; i++) {
        String hourKey = "hr" + String(i);
        hourlyTempPattern[i] = prefs->getFloat((hourKey + "t").c_str(), 25.0);
        hourlyTempStdDev[i] = prefs->getFloat((hourKey + "ts").c_str(), 0.5);
        hourlyPHPattern[i] = prefs->getFloat((hourKey + "p").c_str(), 7.0);
        hourlyPHStdDev[i] = prefs->getFloat((hourKey + "ps").c_str(), 0.2);
        hourlyTDSPattern[i] = prefs->getFloat((hourKey + "d").c_str(), 200.0);
        hourlyTDSStdDev[i] = prefs->getFloat((hourKey + "ds").c_str(), 20.0);
        hourlyAmbientPattern[i] = prefs->getFloat((hourKey + "a").c_str(), 23.0);
        hourlySampleCounts[i] = prefs->getInt((hourKey + "c").c_str(), 0);
    }
    
    totalSamples = prefs->getInt("totalSmp", 0);
    patternsEstablished = prefs->getBool("established", false);
    
    prefs->end();
    
    Serial.printf("Loaded patterns: %d total samples\n", totalSamples);
    if (patternsEstablished) {
        Serial.println("Patterns are established - anomaly detection active");
    }
}

void PatternLearner::savePatterns() {
    if (!prefs->begin("patterns", false)) {
        Serial.println("Failed to save patterns");
        return;
    }
    
    // Save hourly patterns (compact storage)
    for (int i = 0; i < 24; i++) {
        String hourKey = "hr" + String(i);
        prefs->putFloat((hourKey + "t").c_str(), hourlyTempPattern[i]);
        prefs->putFloat((hourKey + "ts").c_str(), hourlyTempStdDev[i]);
        prefs->putFloat((hourKey + "p").c_str(), hourlyPHPattern[i]);
        prefs->putFloat((hourKey + "ps").c_str(), hourlyPHStdDev[i]);
        prefs->putFloat((hourKey + "d").c_str(), hourlyTDSPattern[i]);
        prefs->putFloat((hourKey + "ds").c_str(), hourlyTDSStdDev[i]);
        prefs->putFloat((hourKey + "a").c_str(), hourlyAmbientPattern[i]);
        prefs->putInt((hourKey + "c").c_str(), hourlySampleCounts[i]);
    }
    
    prefs->putInt("totalSmp", totalSamples);
    prefs->putBool("established", patternsEstablished);
    
    prefs->end();
    Serial.println("Patterns saved to NVS");
}

void PatternLearner::reset() {
    Serial.println("Resetting all learned patterns...");
    
    for (int i = 0; i < 24; i++) {
        hourlyTempPattern[i] = 25.0;
        hourlyTempStdDev[i] = 0.5;
        hourlyPHPattern[i] = 7.0;
        hourlyPHStdDev[i] = 0.2;
        hourlyTDSPattern[i] = 200.0;
        hourlyTDSStdDev[i] = 20.0;
        hourlyAmbientPattern[i] = 23.0;
        hourlySampleCounts[i] = 0;
    }
    
    totalSamples = 0;
    patternsEstablished = false;
    totalAnomaliesDetected = 0;
    anomalyHistory.clear();
    
    savePatterns();
}

void PatternLearner::update(float temp, float ph, float tds, float ambientTemp) {
    if (!config.enabled) return;
    
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    int hour = timeinfo.tm_hour;
    int dayOfWeek = timeinfo.tm_wday;
    
    // Learn patterns
    learnPattern(hour, dayOfWeek, temp, ph, tds, ambientTemp);
    
    // Check for anomalies (only if patterns are established)
    if (patternsEstablished && millis() - lastAnomalyCheck > 60000) { // Check every minute
        checkForAnomalies(temp, ph, tds, ambientTemp);
        lastAnomalyCheck = millis();
    }
    
    // Update seasonal stats
    updateSeasonalStats();
    
    // Save patterns periodically (every hour)
    static unsigned long lastSave = 0;
    if (millis() - lastSave > 3600000) {
        savePatterns();
        lastSave = millis();
    }
}

void PatternLearner::learnPattern(int hour, int dayOfWeek, float temp, float ph, float tds, float ambient) {
    // Validate inputs
    if (hour < 0 || hour >= 24) return;
    if (temp < 0 || temp > 40) return;
    if (ph < 0 || ph > 14) return;
    if (tds < 0 || tds > 2000) return;
    
    updateHourlyPattern(hour, temp, ph, tds, ambient);
    updateDailyPattern(dayOfWeek, temp, ph, tds);
    
    totalSamples++;
    
    // Consider patterns established after collecting data for all hours
    // with at least minSamplesForAnomaly samples each
    bool allHoursHaveSamples = true;
    for (int i = 0; i < 24; i++) {
        if (hourlySampleCounts[i] < config.minSamplesForAnomaly) {
            allHoursHaveSamples = false;
            break;
        }
    }
    
    if (allHoursHaveSamples && !patternsEstablished) {
        patternsEstablished = true;
        Serial.println("✓ Patterns established! Anomaly detection now active.");
    }
}

void PatternLearner::updateHourlyPattern(int hour, float temp, float ph, float tds, float ambient) {
    // Exponential moving average with adaptive learning rate
    // Higher learning rate early on, lower as more samples collected
    int count = hourlySampleCounts[hour];
    float alpha = (count < 30) ? 0.2 : 0.05; // Fast learning then slow refinement
    
    // Update means using exponential moving average
    if (count == 0) {
        hourlyTempPattern[hour] = temp;
        hourlyPHPattern[hour] = ph;
        hourlyTDSPattern[hour] = tds;
        hourlyAmbientPattern[hour] = ambient;
    } else {
        hourlyTempPattern[hour] = hourlyTempPattern[hour] * (1 - alpha) + temp * alpha;
        hourlyPHPattern[hour] = hourlyPHPattern[hour] * (1 - alpha) + ph * alpha;
        hourlyTDSPattern[hour] = hourlyTDSPattern[hour] * (1 - alpha) + tds * alpha;
        hourlyAmbientPattern[hour] = hourlyAmbientPattern[hour] * (1 - alpha) + ambient * alpha;
    }
    
    // Update standard deviations
    if (count > 1) {
        float tempDiff = abs(temp - hourlyTempPattern[hour]);
        float phDiff = abs(ph - hourlyPHPattern[hour]);
        float tdsDiff = abs(tds - hourlyTDSPattern[hour]);
        
        hourlyTempStdDev[hour] = hourlyTempStdDev[hour] * (1 - alpha) + tempDiff * alpha;
        hourlyPHStdDev[hour] = hourlyPHStdDev[hour] * (1 - alpha) + phDiff * alpha;
        hourlyTDSStdDev[hour] = hourlyTDSStdDev[hour] * (1 - alpha) + tdsDiff * alpha;
        
        // Ensure minimum std dev to avoid false positives
        if (hourlyTempStdDev[hour] < 0.2) hourlyTempStdDev[hour] = 0.2;
        if (hourlyPHStdDev[hour] < 0.1) hourlyPHStdDev[hour] = 0.1;
        if (hourlyTDSStdDev[hour] < 10.0) hourlyTDSStdDev[hour] = 10.0;
    }
    
    hourlySampleCounts[hour]++;
}

void PatternLearner::updateDailyPattern(int dayOfWeek, float temp, float ph, float tds) {
    if (dayOfWeek < 0 || dayOfWeek >= 7) return;
    
    int count = dailySampleCounts[dayOfWeek];
    float alpha = (count < 10) ? 0.3 : 0.1;
    
    if (count == 0) {
        dailyTempPattern[dayOfWeek] = temp;
        dailyPHPattern[dayOfWeek] = ph;
        dailyTDSPattern[dayOfWeek] = tds;
    } else {
        dailyTempPattern[dayOfWeek] = dailyTempPattern[dayOfWeek] * (1 - alpha) + temp * alpha;
        dailyPHPattern[dayOfWeek] = dailyPHPattern[dayOfWeek] * (1 - alpha) + ph * alpha;
        dailyTDSPattern[dayOfWeek] = dailyTDSPattern[dayOfWeek] * (1 - alpha) + tds * alpha;
    }
    
    dailySampleCounts[dayOfWeek]++;
}

void PatternLearner::updateSeasonalStats() {
    // Update rolling ambient temperature history
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    int hour = timeinfo.tm_hour;
    float currentAmbient = hourlyAmbientPattern[hour];
    
    ambientTempHistory[ambientHistoryIndex] = currentAmbient;
    ambientHistoryIndex = (ambientHistoryIndex + 1) % 168;
    
    // Calculate 7-day averages
    float tempSum = 0, phSum = 0, tdsSum = 0, ambientSum = 0;
    for (int i = 0; i < 24; i++) {
        tempSum += hourlyTempPattern[i];
        phSum += hourlyPHPattern[i];
        tdsSum += hourlyTDSPattern[i];
    }
    
    for (int i = 0; i < 168; i++) {
        ambientSum += ambientTempHistory[i];
    }
    
    currentSeason.avgWaterTemp = tempSum / 24.0;
    currentSeason.avgPH = phSum / 24.0;
    currentSeason.avgTDS = tdsSum / 24.0;
    currentSeason.avgAmbientTemp = ambientSum / 168.0;
    currentSeason.daysCollected = min(totalSamples / 24, 168);
    currentSeason.season = detectSeason(currentSeason.avgAmbientTemp);
    
    // Log season changes
    if (currentSeason.season != lastDetectedSeason && currentSeason.daysCollected >= 7) {
        Serial.printf("Season change detected: %s → %s (ambient: %.1f°C)\n",
                     lastDetectedSeason.c_str(), currentSeason.season.c_str(),
                     currentSeason.avgAmbientTemp);
        lastDetectedSeason = currentSeason.season;
    }
}

String PatternLearner::detectSeason(float avgAmbient) {
    // Simple season detection based on ambient temperature
    if (avgAmbient < 18.0) return "winter";
    else if (avgAmbient < 22.0) return "spring";
    else if (avgAmbient < 27.0) return "summer";
    else return "hot-summer";
}

bool PatternLearner::checkForAnomalies(float temp, float ph, float tds, float ambient) {
    if (!patternsEstablished) return false;
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    int hour = timeinfo.tm_hour;
    
    bool anomalyDetected = false;
    
    // Check temperature anomaly
    Anomaly tempAnomaly = detectTemperatureAnomaly(hour, temp);
    if (tempAnomaly.severity != "normal") {
        anomalyHistory.push_back(tempAnomaly);
        totalAnomaliesDetected++;
        anomalyDetected = true;
        
        Serial.printf("⚠️ TEMP ANOMALY [%s]: %.2f°C (expected %.2f±%.2f, %.1fσ deviation)\n",
                     tempAnomaly.severity.c_str(), temp, 
                     hourlyTempPattern[hour], hourlyTempStdDev[hour],
                     tempAnomaly.deviation);
    }
    
    // Check pH anomaly
    Anomaly phAnomaly = detectPHAnomaly(hour, ph);
    if (phAnomaly.severity != "normal") {
        anomalyHistory.push_back(phAnomaly);
        totalAnomaliesDetected++;
        anomalyDetected = true;
        
        Serial.printf("⚠️ pH ANOMALY [%s]: %.2f (expected %.2f±%.2f, %.1fσ deviation)\n",
                     phAnomaly.severity.c_str(), ph,
                     hourlyPHPattern[hour], hourlyPHStdDev[hour],
                     phAnomaly.deviation);
    }
    
    // Check TDS anomaly
    Anomaly tdsAnomaly = detectTDSAnomaly(hour, tds);
    if (tdsAnomaly.severity != "normal") {
        anomalyHistory.push_back(tdsAnomaly);
        totalAnomaliesDetected++;
        anomalyDetected = true;
        
        Serial.printf("⚠️ TDS ANOMALY [%s]: %.0f ppm (expected %.0f±%.0f, %.1fσ deviation)\n",
                     tdsAnomaly.severity.c_str(), tds,
                     hourlyTDSPattern[hour], hourlyTDSStdDev[hour],
                     tdsAnomaly.deviation);
    }
    
    pruneAnomalyHistory();
    return anomalyDetected;
}

Anomaly PatternLearner::detectTemperatureAnomaly(int hour, float temp) {
    Anomaly anomaly;
    anomaly.timestamp = time(nullptr);  // Use NTP time instead of millis
    anomaly.type = "temperature";
    anomaly.actualValue = temp;
    anomaly.expectedValue = hourlyTempPattern[hour];
    anomaly.deviation = getSigmaDeviation(temp, hourlyTempPattern[hour], hourlyTempStdDev[hour]);
    
    if (abs(anomaly.deviation) >= config.tempAnomalyThreshold) {
        anomaly.severity = getSeverity(abs(anomaly.deviation));
    } else {
        anomaly.severity = "normal";
    }
    
    return anomaly;
}

Anomaly PatternLearner::detectPHAnomaly(int hour, float ph) {
    Anomaly anomaly;
    anomaly.timestamp = time(nullptr);  // Use NTP time instead of millis
    anomaly.type = "ph";
    anomaly.actualValue = ph;
    anomaly.expectedValue = hourlyPHPattern[hour];
    anomaly.deviation = getSigmaDeviation(ph, hourlyPHPattern[hour], hourlyPHStdDev[hour]);
    
    if (abs(anomaly.deviation) >= config.phAnomalyThreshold) {
        anomaly.severity = getSeverity(abs(anomaly.deviation));
    } else {
        anomaly.severity = "normal";
    }
    
    return anomaly;
}

Anomaly PatternLearner::detectTDSAnomaly(int hour, float tds) {
    Anomaly anomaly;
    anomaly.timestamp = time(nullptr);  // Use NTP time instead of millis
    anomaly.type = "tds";
    anomaly.actualValue = tds;
    anomaly.expectedValue = hourlyTDSPattern[hour];
    anomaly.deviation = getSigmaDeviation(tds, hourlyTDSPattern[hour], hourlyTDSStdDev[hour]);
    
    if (abs(anomaly.deviation) >= config.tdsAnomalyThreshold) {
        anomaly.severity = getSeverity(abs(anomaly.deviation));
    } else {
        anomaly.severity = "normal";
    }
    
    return anomaly;
}

float PatternLearner::getSigmaDeviation(float actual, float expected, float stdDev) {
    if (stdDev < 0.01) return 0.0; // Avoid division by near-zero
    return (actual - expected) / stdDev;
}

String PatternLearner::getSeverity(float sigma) {
    if (sigma >= 4.0) return "critical";
    else if (sigma >= 3.0) return "high";
    else if (sigma >= 2.0) return "medium";
    else return "low";
}

void PatternLearner::pruneAnomalyHistory() {
    while (anomalyHistory.size() > (size_t)config.maxAnomalyHistory) {
        anomalyHistory.erase(anomalyHistory.begin());
    }
}

// Getters for expected values
float PatternLearner::getExpectedTemp(int hour) {
    if (hour < 0 || hour >= 24) return 25.0;
    return hourlyTempPattern[hour];
}

float PatternLearner::getExpectedPH(int hour) {
    if (hour < 0 || hour >= 24) return 7.0;
    return hourlyPHPattern[hour];
}

float PatternLearner::getExpectedTDS(int hour) {
    if (hour < 0 || hour >= 24) return 200.0;
    return hourlyTDSPattern[hour];
}

float PatternLearner::getExpectedAmbient(int hour) {
    if (hour < 0 || hour >= 24) return 23.0;
    return hourlyAmbientPattern[hour];
}

float PatternLearner::getTempStdDev(int hour) {
    if (hour < 0 || hour >= 24) return 0.5;
    return hourlyTempStdDev[hour];
}

float PatternLearner::getPHStdDev(int hour) {
    if (hour < 0 || hour >= 24) return 0.2;
    return hourlyPHStdDev[hour];
}

float PatternLearner::getTDSStdDev(int hour) {
    if (hour < 0 || hour >= 24) return 20.0;
    return hourlyTDSStdDev[hour];
}

SeasonalStats PatternLearner::getSeasonalStats() {
    return currentSeason;
}

String PatternLearner::getCurrentSeason() {
    return currentSeason.season;
}

bool PatternLearner::shouldAdaptForSeason() {
    return config.autoSeasonalAdapt && patternsEstablished && currentSeason.daysCollected >= 7;
}

void PatternLearner::getSeasonalPIDMultipliers(float& kpMult, float& kiMult, float& kdMult) {
    // Default: no adjustment
    kpMult = 1.0;
    kiMult = 1.0;
    kdMult = 1.0;
    
    if (!shouldAdaptForSeason()) return;
    
    String season = currentSeason.season;
    
    if (season == "winter") {
        // Winter: More aggressive heating needed
        kpMult = 1.2;
        kiMult = 1.3;
        kdMult = 1.1;
    } else if (season == "summer") {
        // Summer: Gentler control, less heating
        kpMult = 0.8;
        kiMult = 0.7;
        kdMult = 1.2;
    } else if (season == "hot-summer") {
        // Hot summer: Minimal heating
        kpMult = 0.6;
        kiMult = 0.5;
        kdMult = 1.3;
    }
    // Spring/Fall: default multipliers (1.0)
}

std::vector<Anomaly> PatternLearner::getRecentAnomalies(int count) {
    std::vector<Anomaly> recent;
    int start = max(0, (int)anomalyHistory.size() - count);
    
    for (size_t i = start; i < anomalyHistory.size(); i++) {
        recent.push_back(anomalyHistory[i]);
    }
    
    return recent;
}

int PatternLearner::getAnomalyCount() {
    return totalAnomaliesDetected;
}

void PatternLearner::clearAnomalyHistory() {
    anomalyHistory.clear();
    totalAnomaliesDetected = 0;
}

int PatternLearner::getSampleCount(int hour) {
    if (hour < 0 || hour >= 24) return 0;
    return hourlySampleCounts[hour];
}

int PatternLearner::getTotalSamples() {
    return totalSamples;
}

bool PatternLearner::arePatternsEstablished() {
    return patternsEstablished;
}

int PatternLearner::getDaysLearning() {
    return (millis() - learningStartTime) / (1000 * 60 * 60 * 24);
}

float PatternLearner::getPatternConfidence() {
    if (totalSamples == 0) return 0.0;
    
    // Confidence based on minimum samples per hour
    int minSamples = hourlySampleCounts[0];
    for (int i = 1; i < 24; i++) {
        if (hourlySampleCounts[i] < minSamples) {
            minSamples = hourlySampleCounts[i];
        }
    }
    
    // Full confidence after 30 samples per hour
    float confidence = min(1.0f, minSamples / 30.0f);
    return confidence;
}

void PatternLearner::setConfig(PatternConfig newConfig) {
    config = newConfig;
    saveConfig();
}

PatternConfig PatternLearner::getConfig() {
    return config;
}

void PatternLearner::setEnabled(bool enabled) {
    config.enabled = enabled;
    saveConfig();
}

bool PatternLearner::isEnabled() {
    return config.enabled;
}

void PatternLearner::setAnomalyThresholds(float temp, float ph, float tds) {
    config.tempAnomalyThreshold = temp;
    config.phAnomalyThreshold = ph;
    config.tdsAnomalyThreshold = tds;
    saveConfig();
}

void PatternLearner::setAutoSeasonalAdapt(bool enable) {
    config.autoSeasonalAdapt = enable;
    saveConfig();
}

float PatternLearner::predictTempChange(int hoursAhead) {
    if (!patternsEstablished || hoursAhead < 1 || hoursAhead > 24) return 0.0;
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    int currentHour = timeinfo.tm_hour;
    int futureHour = (currentHour + hoursAhead) % 24;
    
    return hourlyTempPattern[futureHour] - hourlyTempPattern[currentHour];
}

bool PatternLearner::predictAnomalyLikelihood(int hoursAhead) {
    // Simple prediction: check if recent anomalies show a trend
    if (anomalyHistory.size() < 3) return false;
    
    int recentAnomalies = 0;
    unsigned long currentTime = time(nullptr);  // Use NTP time instead of millis
    unsigned long lookback = 3600 * hoursAhead; // Look back same duration
    
    for (const auto& anomaly : anomalyHistory) {
        if (currentTime - anomaly.timestamp < lookback) {
            recentAnomalies++;
        }
    }
    
    return recentAnomalies >= 2; // 2+ anomalies recently suggests likelihood
}

void PatternLearner::printPatternSummary() {
    Serial.println("\n=== Pattern Learning Summary ===");
    Serial.printf("Status: %s\n", patternsEstablished ? "ESTABLISHED" : "LEARNING");
    Serial.printf("Total Samples: %d\n", totalSamples);
    Serial.printf("Days Learning: %d\n", getDaysLearning());
    Serial.printf("Confidence: %.1f%%\n", getPatternConfidence() * 100);
    Serial.printf("Season: %s (ambient: %.1f°C)\n", 
                 currentSeason.season.c_str(), currentSeason.avgAmbientTemp);
    Serial.printf("Total Anomalies: %d\n", totalAnomaliesDetected);
    
    Serial.println("\nHourly Temperature Pattern:");
    for (int i = 0; i < 24; i++) {
        Serial.printf("  %02d:00 - %.2f°C ± %.2f (n=%d)\n", 
                     i, hourlyTempPattern[i], hourlyTempStdDev[i], hourlySampleCounts[i]);
    }
}

void PatternLearner::printAnomalies() {
    Serial.println("\n=== Recent Anomalies ===");
    auto recent = getRecentAnomalies(10);
    
    if (recent.empty()) {
        Serial.println("No anomalies detected");
        return;
    }
    
    for (const auto& a : recent) {
        Serial.printf("[%s] %s: %.2f (expected %.2f, %.1fσ)\n",
                     a.severity.c_str(), a.type.c_str(), 
                     a.actualValue, a.expectedValue, a.deviation);
    }
}

String PatternLearner::getStatusJSON() {
    String json = "{";
    json += "\"enabled\":" + String(config.enabled ? "true" : "false") + ",";
    json += "\"established\":" + String(patternsEstablished ? "true" : "false") + ",";
    json += "\"totalSamples\":" + String(totalSamples) + ",";
    json += "\"confidence\":" + String(getPatternConfidence(), 2) + ",";
    json += "\"daysLearning\":" + String(getDaysLearning()) + ",";
    json += "\"season\":\"" + currentSeason.season + "\",";
    json += "\"avgAmbient\":" + String(currentSeason.avgAmbientTemp, 1) + ",";
    json += "\"totalAnomalies\":" + String(totalAnomaliesDetected) + ",";
    json += "\"recentAnomalies\":" + String(anomalyHistory.size());
    json += "}";
    return json;
}

