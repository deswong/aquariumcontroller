#include "MLDataLogger.h"
#include <time.h>

#define ML_DATA_FILE "/ml/pid_samples.dat"
#define LOOKUP_TABLE_FILE "/ml/pid_lookup.dat"

MLDataLogger::MLDataLogger() 
    : mlDataPath(ML_DATA_FILE), lookupTablePath(LOOKUP_TABLE_FILE),
      spiffsAvailable(false), totalSamples(0), samplesThisSession(0) {
}

MLDataLogger::~MLDataLogger() {
}

bool MLDataLogger::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[MLDataLogger] SPIFFS not available");
        return false;
    }
    
    spiffsAvailable = true;
    
    // Create ML directory if it doesn't exist
    if (!SPIFFS.exists("/ml")) {
        SPIFFS.mkdir("/ml");
        Serial.println("[MLDataLogger] Created /ml directory");
    }
    
    // Load existing lookup table
    loadLookupTableFromFile();
    
    // Count existing samples
    if (SPIFFS.exists(mlDataPath)) {
        File file = SPIFFS.open(mlDataPath, "r");
        if (file) {
            totalSamples = file.size() / sizeof(PIDPerformanceSample);
            file.close();
            Serial.printf("[MLDataLogger] Found %d existing samples\n", totalSamples);
        }
    }
    
    Serial.printf("[MLDataLogger] Initialized - %d samples, %d lookup entries\n", 
                  totalSamples, lookupTable.size());
    
    return true;
}

float MLDataLogger::calculatePerformanceScore(const PIDPerformanceSample& sample) {
    // Performance score: lower is better for these metrics
    // Normalize to 0-100 scale where 100 is perfect
    
    float settlingPenalty = min(sample.settlingTime / 300.0f, 1.0f); // 5 min max
    float overshootPenalty = min(sample.overshoot * 10.0f, 1.0f);    // 0.1 = 100% penalty
    float steadyStatePenalty = min(abs(sample.steadyStateError) * 20.0f, 1.0f); // 0.05 = 100%
    float variancePenalty = min(sample.errorVariance / 5.0f, 1.0f);  // variance of 5 = 100%
    
    // Weighted score (higher is better)
    float score = 100.0f * (
        1.0f - (0.3f * settlingPenalty +      // 30% weight on settling time
                0.3f * overshootPenalty +      // 30% weight on overshoot
                0.2f * steadyStatePenalty +    // 20% weight on steady-state error
                0.2f * variancePenalty)        // 20% weight on oscillation
    );
    
    return max(0.0f, score);
}

String MLDataLogger::generateBucketKey(float temp, float ambient, uint8_t hour, uint8_t season) {
    // Discretize features into buckets for lookup table
    // Temperature: 2°C buckets (e.g., 24-26°C)
    int tempBucket = (int)(temp / 2.0f);
    
    // Ambient: 3°C buckets
    int ambientBucket = (int)(ambient / 3.0f);
    
    // Hour: 6-hour blocks (night, morning, afternoon, evening)
    int hourBucket = hour / 6;
    
    // Generate unique key
    char key[32];
    snprintf(key, sizeof(key), "T%d_A%d_H%d_S%d", tempBucket, ambientBucket, hourBucket, season);
    return String(key);
}

bool MLDataLogger::logSample(const PIDPerformanceSample& sample) {
    if (!spiffsAvailable) {
        return false;
    }
    
    // Calculate performance score
    PIDPerformanceSample scoredSample = sample;
    scoredSample.score = calculatePerformanceScore(sample);
    
    // Append sample to file
    File file = SPIFFS.open(mlDataPath, "a");
    if (!file) {
        Serial.println("[MLDataLogger] Failed to open data file");
        return false;
    }
    
    size_t written = file.write((uint8_t*)&scoredSample, sizeof(PIDPerformanceSample));
    file.close();
    
    if (written != sizeof(PIDPerformanceSample)) {
        Serial.println("[MLDataLogger] Failed to write sample");
        return false;
    }
    
    totalSamples++;
    samplesThisSession++;
    
    // Update lookup table with this sample
    updateLookupTable(scoredSample);
    
    // Save lookup table periodically
    if (samplesThisSession % 10 == 0) {
        saveLookupTableToFile();
    }
    
    Serial.printf("[MLDataLogger] Logged sample #%d (score: %.1f) - Kp=%.3f, Ki=%.3f, Kd=%.3f\n",
                  totalSamples, scoredSample.score, scoredSample.kp, scoredSample.ki, scoredSample.kd);
    
    return true;
}

void MLDataLogger::updateLookupTable(const PIDPerformanceSample& sample) {
    String key = generateBucketKey(sample.currentValue, sample.ambientTemp, 
                                   sample.hourOfDay, sample.season);
    
    if (lookupTable.find(key) != lookupTable.end()) {
        // Update existing entry with exponential moving average
        PIDGainEntry& entry = lookupTable[key];
        
        // If new score is better, weight it more heavily
        float weight = (sample.score > entry.avgScore) ? 0.7f : 0.3f;
        
        entry.kp = entry.kp * (1.0f - weight) + sample.kp * weight;
        entry.ki = entry.ki * (1.0f - weight) + sample.ki * weight;
        entry.kd = entry.kd * (1.0f - weight) + sample.kd * weight;
        entry.avgScore = entry.avgScore * 0.9f + sample.score * 0.1f; // Slower average
        entry.sampleCount++;
    } else {
        // Create new entry
        PIDGainEntry entry;
        entry.kp = sample.kp;
        entry.ki = sample.ki;
        entry.kd = sample.kd;
        entry.avgScore = sample.score;
        entry.sampleCount = 1;
        lookupTable[key] = entry;
    }
}

bool MLDataLogger::getOptimalGains(float temp, float ambient, uint8_t hour, uint8_t season,
                                   float& kp, float& ki, float& kd, float& confidence) {
    String key = generateBucketKey(temp, ambient, hour, season);
    
    if (lookupTable.find(key) != lookupTable.end()) {
        const PIDGainEntry& entry = lookupTable[key];
        
        // Only use if we have sufficient samples and good score
        if (entry.sampleCount >= 3 && entry.avgScore > 50.0f) {
            kp = entry.kp;
            ki = entry.ki;
            kd = entry.kd;
            
            // Confidence based on sample count and score
            confidence = min(1.0f, (entry.sampleCount / 20.0f) * (entry.avgScore / 100.0f));
            
            return true;
        }
    }
    
    // Fallback: try nearby buckets or global best
    return getBestGlobalGains(kp, ki, kd, confidence);
}

bool MLDataLogger::getBestGlobalGains(float& kp, float& ki, float& kd, float& avgScore) {
    if (lookupTable.empty()) {
        return false;
    }
    
    // Find entry with highest average score
    float bestScore = -1.0f;
    const PIDGainEntry* bestEntry = nullptr;
    
    for (const auto& pair : lookupTable) {
        if (pair.second.avgScore > bestScore && pair.second.sampleCount >= 5) {
            bestScore = pair.second.avgScore;
            bestEntry = &pair.second;
        }
    }
    
    if (bestEntry) {
        kp = bestEntry->kp;
        ki = bestEntry->ki;
        kd = bestEntry->kd;
        avgScore = bestEntry->avgScore;
        return true;
    }
    
    return false;
}

std::vector<PIDPerformanceSample> MLDataLogger::getAllSamples(int maxSamples) {
    std::vector<PIDPerformanceSample> samples;
    
    if (!spiffsAvailable || !SPIFFS.exists(mlDataPath)) {
        return samples;
    }
    
    File file = SPIFFS.open(mlDataPath, "r");
    if (!file) {
        return samples;
    }
    
    // Read samples from file
    PIDPerformanceSample sample;
    int count = 0;
    
    while (file.available() && count < maxSamples) {
        size_t read = file.read((uint8_t*)&sample, sizeof(PIDPerformanceSample));
        if (read == sizeof(PIDPerformanceSample)) {
            samples.push_back(sample);
            count++;
        } else {
            break;
        }
    }
    
    file.close();
    return samples;
}

String MLDataLogger::exportAsCSV(int maxSamples) {
    String csv = "timestamp,currentValue,targetValue,ambientTemp,hourOfDay,season,tankVolume,";
    csv += "kp,ki,kd,errorMean,errorVariance,settlingTime,overshoot,steadyStateError,";
    csv += "averageOutput,cycleCount,score\n";
    
    std::vector<PIDPerformanceSample> samples = getAllSamples(maxSamples);
    
    for (const auto& s : samples) {
        csv += String(s.timestamp) + ",";
        csv += String(s.currentValue, 3) + ",";
        csv += String(s.targetValue, 3) + ",";
        csv += String(s.ambientTemp, 3) + ",";
        csv += String(s.hourOfDay) + ",";
        csv += String(s.season) + ",";
        csv += String(s.tankVolume, 1) + ",";
        csv += String(s.kp, 4) + ",";
        csv += String(s.ki, 4) + ",";
        csv += String(s.kd, 4) + ",";
        csv += String(s.errorMean, 4) + ",";
        csv += String(s.errorVariance, 4) + ",";
        csv += String(s.settlingTime, 1) + ",";
        csv += String(s.overshoot, 4) + ",";
        csv += String(s.steadyStateError, 4) + ",";
        csv += String(s.averageOutput, 2) + ",";
        csv += String(s.cycleCount) + ",";
        csv += String(s.score, 2) + "\n";
    }
    
    return csv;
}

void MLDataLogger::saveLookupTableToFile() {
    if (!spiffsAvailable) return;
    
    File file = SPIFFS.open(lookupTablePath, "w");
    if (!file) {
        Serial.println("[MLDataLogger] Failed to save lookup table");
        return;
    }
    
    // Write number of entries
    uint32_t count = lookupTable.size();
    file.write((uint8_t*)&count, sizeof(uint32_t));
    
    // Write each entry
    for (const auto& pair : lookupTable) {
        // Write key length and key
        uint8_t keyLen = pair.first.length();
        file.write(&keyLen, 1);
        file.write((uint8_t*)pair.first.c_str(), keyLen);
        
        // Write entry data
        file.write((uint8_t*)&pair.second, sizeof(PIDGainEntry));
    }
    
    file.close();
    Serial.printf("[MLDataLogger] Saved %d lookup entries\n", count);
}

void MLDataLogger::loadLookupTableFromFile() {
    if (!spiffsAvailable || !SPIFFS.exists(lookupTablePath)) {
        return;
    }
    
    File file = SPIFFS.open(lookupTablePath, "r");
    if (!file) {
        return;
    }
    
    // Read number of entries
    uint32_t count;
    if (file.read((uint8_t*)&count, sizeof(uint32_t)) != sizeof(uint32_t)) {
        file.close();
        return;
    }
    
    // Read each entry
    for (uint32_t i = 0; i < count; i++) {
        // Read key
        uint8_t keyLen;
        if (file.read(&keyLen, 1) != 1) break;
        
        char keyBuf[64];
        if (file.read((uint8_t*)keyBuf, keyLen) != keyLen) break;
        keyBuf[keyLen] = '\0';
        String key(keyBuf);
        
        // Read entry data
        PIDGainEntry entry;
        if (file.read((uint8_t*)&entry, sizeof(PIDGainEntry)) != sizeof(PIDGainEntry)) break;
        
        lookupTable[key] = entry;
    }
    
    file.close();
    Serial.printf("[MLDataLogger] Loaded %d lookup entries\n", lookupTable.size());
}

void MLDataLogger::clearData() {
    if (!spiffsAvailable) return;
    
    if (SPIFFS.exists(mlDataPath)) {
        SPIFFS.remove(mlDataPath);
    }
    if (SPIFFS.exists(lookupTablePath)) {
        SPIFFS.remove(lookupTablePath);
    }
    
    lookupTable.clear();
    totalSamples = 0;
    samplesThisSession = 0;
    
    Serial.println("[MLDataLogger] All ML data cleared");
}

size_t MLDataLogger::getDataFileSize() {
    if (!spiffsAvailable || !SPIFFS.exists(mlDataPath)) {
        return 0;
    }
    
    File file = SPIFFS.open(mlDataPath, "r");
    if (!file) {
        return 0;
    }
    
    size_t size = file.size();
    file.close();
    return size;
}
