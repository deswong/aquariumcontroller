#include "WaterChangeAssistant.h"
#include "SystemTasks.h"
#include <SPIFFS.h>

WaterChangeAssistant::WaterChangeAssistant() 
    : schedule(SCHEDULE_NONE), lastChangeTime(0), scheduledVolumePercent(25.0),
      tankVolumeGallons(20.0), currentPhase(PHASE_IDLE), phaseStartTime(0),
      currentChangeVolume(0), tempBeforeChange(0), phBeforeChange(0), systemsPaused(false),
      maxTempDifference(2.0), maxPhDifference(0.5),
      maxDrainTime(600000), maxFillTime(600000), stabilizationTime(300000) {
    
    prefs = new Preferences();
}

WaterChangeAssistant::~WaterChangeAssistant() {
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

void WaterChangeAssistant::begin() {
    loadSettings();
    loadHistory();
    
    Serial.println("Water Change Assistant initialized");
    Serial.printf("Tank volume: %.1f gallons\n", tankVolumeGallons);
    Serial.printf("Schedule: %d days, %.1f%% volume\n", (int)schedule, scheduledVolumePercent);
    
    if (eventLogger) {
        eventLogger->info("waterchange", "Water change assistant initialized");
    }
}

void WaterChangeAssistant::update() {
    if (currentPhase == PHASE_IDLE || currentPhase == PHASE_COMPLETE) {
        return; // Nothing to do
    }
    
    unsigned long elapsed = millis() - phaseStartTime;
    
    // Check for timeout in drain/fill phases
    if (currentPhase == PHASE_DRAINING && elapsed > maxDrainTime) {
        Serial.println("WARNING: Drain phase timeout - please check drain system");
        if (eventLogger) {
            eventLogger->warning("waterchange", "Drain phase timeout exceeded");
        }
    }
    
    if (currentPhase == PHASE_FILLING && elapsed > maxFillTime) {
        Serial.println("WARNING: Fill phase timeout - please check fill system");
        if (eventLogger) {
            eventLogger->warning("waterchange", "Fill phase timeout exceeded");
        }
    }
    
    // Auto-advance from stabilizing phase
    if (currentPhase == PHASE_STABILIZING && elapsed > stabilizationTime) {
        Serial.println("Stabilization complete - water change finished");
        completeWaterChange();
    }
}

void WaterChangeAssistant::loadSettings() {
    if (!prefs->begin("waterchange", true)) {
        Serial.println("No saved water change settings, using defaults");
        return;
    }
    
    schedule = (WaterChangeSchedule)prefs->getInt("schedule", SCHEDULE_WEEKLY);
    lastChangeTime = prefs->getULong("lastChange", 0);
    scheduledVolumePercent = prefs->getFloat("volumePct", 25.0);
    tankVolumeGallons = prefs->getFloat("tankVol", 20.0);
    maxTempDifference = prefs->getFloat("maxTempDiff", 2.0);
    maxPhDifference = prefs->getFloat("maxPhDiff", 0.5);
    
    prefs->end();
    Serial.println("Water change settings loaded from NVS");
}

void WaterChangeAssistant::saveSettings() {
    if (!prefs->begin("waterchange", false)) {
        Serial.println("ERROR: Failed to save water change settings");
        return;
    }
    
    prefs->putInt("schedule", (int)schedule);
    prefs->putULong("lastChange", lastChangeTime);
    prefs->putFloat("volumePct", scheduledVolumePercent);
    prefs->putFloat("tankVol", tankVolumeGallons);
    prefs->putFloat("maxTempDiff", maxTempDifference);
    prefs->putFloat("maxPhDiff", maxPhDifference);
    
    prefs->end();
    Serial.println("Water change settings saved");
}

void WaterChangeAssistant::loadHistory() {
    if (!SPIFFS.exists(HISTORY_FILE)) {
        Serial.println("No water change history file found");
        return;
    }
    
    File file = SPIFFS.open(HISTORY_FILE, "r");
    if (!file) {
        Serial.println("ERROR: Failed to open water change history");
        return;
    }
    
    history.clear();
    while (file.available() && history.size() < MAX_HISTORY) {
        WaterChangeRecord record;
        file.read((uint8_t*)&record, sizeof(WaterChangeRecord));
        history.push_back(record);
    }
    
    file.close();
    Serial.printf("Loaded %d water change records\n", history.size());
}

void WaterChangeAssistant::saveHistory() {
    File file = SPIFFS.open(HISTORY_FILE, "w");
    if (!file) {
        Serial.println("ERROR: Failed to save water change history");
        return;
    }
    
    // Write all records
    for (const auto& record : history) {
        file.write((uint8_t*)&record, sizeof(WaterChangeRecord));
    }
    
    file.close();
    Serial.println("Water change history saved");
}

void WaterChangeAssistant::addToHistory(const WaterChangeRecord& record) {
    history.push_back(record);
    
    // Keep only most recent records
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin());
    }
    
    saveHistory();
}

void WaterChangeAssistant::setSchedule(WaterChangeSchedule sched, float volumePercent) {
    schedule = sched;
    scheduledVolumePercent = constrain(volumePercent, 10.0, 50.0); // 10-50% range
    saveSettings();
    
    Serial.printf("Water change schedule set: every %d days, %.1f%% volume\n", 
                  (int)schedule, scheduledVolumePercent);
    
    if (eventLogger) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Schedule updated: %d days, %.1f%% volume", 
                 (int)schedule, scheduledVolumePercent);
        eventLogger->info("waterchange", msg);
    }
}

int WaterChangeAssistant::getDaysUntilNextChange() {
    if (schedule == SCHEDULE_NONE || lastChangeTime == 0) {
        return -1; // No schedule set
    }
    
    unsigned long currentTime = millis() / 1000;
    unsigned long timeSinceLastChange = currentTime - lastChangeTime;
    int daysSinceChange = timeSinceLastChange / 86400;
    
    int daysUntilNext = (int)schedule - daysSinceChange;
    return max(0, daysUntilNext);
}

int WaterChangeAssistant::getDaysSinceLastChange() {
    if (lastChangeTime == 0) {
        return 999; // Never changed
    }
    
    unsigned long currentTime = millis() / 1000;
    unsigned long timeSinceLastChange = currentTime - lastChangeTime;
    return timeSinceLastChange / 86400;
}

bool WaterChangeAssistant::isChangeOverdue() {
    if (schedule == SCHEDULE_NONE) {
        return false;
    }
    
    return getDaysUntilNextChange() == 0;
}

void WaterChangeAssistant::setTankVolume(float gallons) {
    tankVolumeGallons = constrain(gallons, 5.0, 1000.0); // 5-1000 gallon range
    saveSettings();
    
    Serial.printf("Tank volume set to %.1f gallons\n", tankVolumeGallons);
}

float WaterChangeAssistant::getScheduledChangeVolume() {
    return tankVolumeGallons * (scheduledVolumePercent / 100.0);
}

bool WaterChangeAssistant::startWaterChange(float volumeGallons) {
    if (isInProgress()) {
        Serial.println("ERROR: Water change already in progress");
        return false;
    }
    
    // Use scheduled volume if not specified
    if (volumeGallons == 0) {
        volumeGallons = getScheduledChangeVolume();
    }
    
    // Validate volume
    if (volumeGallons > tankVolumeGallons * 0.5) {
        Serial.println("ERROR: Change volume exceeds 50% of tank capacity");
        if (eventLogger) {
            eventLogger->error("waterchange", "Change volume too large - safety limit");
        }
        return false;
    }
    
    currentChangeVolume = volumeGallons;
    currentPhase = PHASE_PREPARE;
    phaseStartTime = millis();
    
    // Capture pre-change values
    SensorData data = getSensorData();
    tempBeforeChange = data.temperature;
    phBeforeChange = data.ph;
    
    // Notify water change predictor
    if (wcPredictor) {
        wcPredictor->startWaterChange();
    }
    
    // Pause control systems
    systemsPaused = true;
    if (heaterRelay) heaterRelay->safetyDisable();
    if (co2Relay) co2Relay->safetyDisable();
    
    Serial.println("\n=== Water Change Started ===");
    Serial.printf("Volume: %.1f gallons (%.1f%%)\n", 
                  currentChangeVolume, (currentChangeVolume/tankVolumeGallons)*100);
    Serial.printf("Temperature before: %.1f°C\n", tempBeforeChange);
    Serial.printf("pH before: %.2f\n", phBeforeChange);
    Serial.println("Systems paused for safety");
    Serial.println("===========================\n");
    
    if (eventLogger) {
        char msg[128];
        snprintf(msg, sizeof(msg), 
                 "Water change started: %.1f gal (Temp: %.1f°C, pH: %.2f)", 
                 currentChangeVolume, tempBeforeChange, phBeforeChange);
        eventLogger->info("waterchange", msg);
    }
    
    sendMQTTAlert("waterchange", "Water change started - systems paused", false);
    
    return true;
}

bool WaterChangeAssistant::advancePhase() {
    if (currentPhase == PHASE_IDLE) {
        Serial.println("ERROR: No water change in progress");
        return false;
    }
    
    unsigned long phaseDuration = (millis() - phaseStartTime) / 1000; // seconds
    
    switch (currentPhase) {
        case PHASE_PREPARE:
            currentPhase = PHASE_DRAINING;
            phaseStartTime = millis();
            Serial.println("Phase: DRAINING - Remove old water");
            if (eventLogger) {
                eventLogger->info("waterchange", "Phase: Draining started");
            }
            break;
            
        case PHASE_DRAINING:
            currentPhase = PHASE_DRAINED;
            phaseStartTime = millis();
            Serial.println("Phase: DRAINED - Add new water when ready");
            if (eventLogger) {
                eventLogger->info("waterchange", "Phase: Drained, ready for new water");
            }
            break;
            
        case PHASE_DRAINED:
            currentPhase = PHASE_FILLING;
            phaseStartTime = millis();
            Serial.println("Phase: FILLING - Adding new water");
            if (eventLogger) {
                eventLogger->info("waterchange", "Phase: Filling with new water");
            }
            break;
            
        case PHASE_FILLING: {
            // Check if it's safe to proceed
            SensorData data = getSensorData();
            if (!isSafeToFill(data.temperature, data.ph)) {
                Serial.println("ERROR: Water parameters unsafe - cannot proceed");
                if (eventLogger) {
                    eventLogger->error("waterchange", "Unsafe water parameters detected");
                }
                return false;
            }
            
            currentPhase = PHASE_STABILIZING;
            phaseStartTime = millis();
            Serial.printf("Phase: STABILIZING - Waiting %lu seconds\n", stabilizationTime/1000);
            if (eventLogger) {
                eventLogger->info("waterchange", "Phase: Stabilizing");
            }
            break;
        }
        
        case PHASE_STABILIZING:
            completeWaterChange();
            break;
            
        default:
            break;
    }
    
    return true;
}

bool WaterChangeAssistant::cancelWaterChange() {
    if (!isInProgress()) {
        return false;
    }
    
    Serial.println("Water change CANCELLED by user");
    
    // Resume systems
    systemsPaused = false;
    
    // Reset state
    currentPhase = PHASE_IDLE;
    currentChangeVolume = 0;
    
    if (eventLogger) {
        eventLogger->warning("waterchange", "Water change cancelled by user");
    }
    
    sendMQTTAlert("waterchange", "Water change cancelled - systems resumed", false);
    
    return true;
}

bool WaterChangeAssistant::completeWaterChange() {
    if (currentPhase == PHASE_IDLE) {
        return false;
    }
    
    // Get final readings
    SensorData data = getSensorData();
    float tempAfter = data.temperature;
    float phAfter = data.ph;
    
    // Create history record
    WaterChangeRecord record;
    record.timestamp = millis() / 1000;
    record.volumeChanged = currentChangeVolume;
    record.tempBefore = tempBeforeChange;
    record.tempAfter = tempAfter;
    record.phBefore = phBeforeChange;
    record.phAfter = phAfter;
    record.durationMinutes = (millis() - phaseStartTime) / 60000;
    record.completedSuccessfully = true;
    
    addToHistory(record);
    
    // Update last change time
    lastChangeTime = millis() / 1000;
    saveSettings();
    
    // Resume systems
    systemsPaused = false;
    currentPhase = PHASE_COMPLETE;
    
    Serial.println("\n=== Water Change Complete ===");
    Serial.printf("Volume changed: %.1f gallons\n", currentChangeVolume);
    Serial.printf("Temperature: %.1f°C → %.1f°C (Δ%.2f°C)\n", 
                  tempBeforeChange, tempAfter, tempAfter - tempBeforeChange);
    Serial.printf("pH: %.2f → %.2f (Δ%.2f)\n", 
                  phBeforeChange, phAfter, phAfter - phBeforeChange);
    Serial.printf("Duration: %d minutes\n", record.durationMinutes);
    Serial.println("Systems resumed");
    Serial.println("============================\n");
    
    if (eventLogger) {
        char msg[128];
        snprintf(msg, sizeof(msg), 
                 "Water change completed: %.1f gal in %d min (Temp: %.1f→%.1f°C, pH: %.2f→%.2f)", 
                 currentChangeVolume, record.durationMinutes, 
                 tempBeforeChange, tempAfter, phBeforeChange, phAfter);
        eventLogger->info("waterchange", msg);
    }
    
    sendMQTTAlert("waterchange", "Water change completed successfully - systems resumed", false);
    
    // Notify water change predictor
    if (wcPredictor) {
        float volumePercent = (currentChangeVolume / tankVolumeGallons) * 100.0;
        wcPredictor->completeWaterChange(volumePercent);
    }
    
    // Reset for next change
    currentChangeVolume = 0;
    
    return true;
}

const char* WaterChangeAssistant::getPhaseDescription() {
    switch (currentPhase) {
        case PHASE_IDLE: return "Idle";
        case PHASE_PREPARE: return "Preparing - Systems paused";
        case PHASE_DRAINING: return "Draining old water";
        case PHASE_DRAINED: return "Ready for new water";
        case PHASE_FILLING: return "Filling with new water";
        case PHASE_STABILIZING: return "Stabilizing parameters";
        case PHASE_COMPLETE: return "Complete";
        default: return "Unknown";
    }
}

unsigned long WaterChangeAssistant::getPhaseElapsedTime() {
    if (currentPhase == PHASE_IDLE) {
        return 0;
    }
    return (millis() - phaseStartTime) / 1000;
}

bool WaterChangeAssistant::isSafeToFill(float currentTemp, float currentPh) {
    float tempDiff = abs(currentTemp - tempBeforeChange);
    float phDiff = abs(currentPh - phBeforeChange);
    
    if (tempDiff > maxTempDifference) {
        Serial.printf("WARNING: Temperature difference too large: %.2f°C (max: %.2f°C)\n", 
                      tempDiff, maxTempDifference);
        return false;
    }
    
    if (phDiff > maxPhDifference) {
        Serial.printf("WARNING: pH difference too large: %.2f (max: %.2f)\n", 
                      phDiff, maxPhDifference);
        return false;
    }
    
    return true;
}

void WaterChangeAssistant::setSafetyLimits(float maxTempDiff, float maxPhDiff) {
    maxTempDifference = maxTempDiff;
    maxPhDifference = maxPhDiff;
    saveSettings();
    
    Serial.printf("Safety limits updated: Temp ±%.1f°C, pH ±%.2f\n", 
                  maxTempDifference, maxPhDifference);
}

WaterChangeRecord WaterChangeAssistant::getHistoryRecord(int index) {
    if (index >= 0 && index < history.size()) {
        return history[index];
    }
    return WaterChangeRecord(); // Return empty record
}

std::vector<WaterChangeRecord> WaterChangeAssistant::getRecentHistory(int count) {
    std::vector<WaterChangeRecord> recent;
    
    int start = max(0, (int)history.size() - count);
    for (int i = start; i < history.size(); i++) {
        recent.push_back(history[i]);
    }
    
    return recent;
}

float WaterChangeAssistant::getAverageChangeVolume() {
    if (history.empty()) return 0;
    
    float total = 0;
    for (const auto& record : history) {
        total += record.volumeChanged;
    }
    
    return total / history.size();
}

int WaterChangeAssistant::getTotalChangesThisMonth() {
    unsigned long currentTime = millis() / 1000;
    unsigned long oneMonthAgo = currentTime - (30 * 86400);
    
    int count = 0;
    for (const auto& record : history) {
        if (record.timestamp >= oneMonthAgo) {
            count++;
        }
    }
    
    return count;
}

float WaterChangeAssistant::getTotalVolumeChangedThisMonth() {
    unsigned long currentTime = millis() / 1000;
    unsigned long oneMonthAgo = currentTime - (30 * 86400);
    
    float total = 0;
    for (const auto& record : history) {
        if (record.timestamp >= oneMonthAgo) {
            total += record.volumeChanged;
        }
    }
    
    return total;
}
