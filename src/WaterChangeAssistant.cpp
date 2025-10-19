#include "WaterChangeAssistant.h"
#include "SystemTasks.h"

WaterChangeAssistant::WaterChangeAssistant() 
    : schedule(SCHEDULE_NONE), lastChangeTime(0), scheduledVolumePercent(25.0),
      tankVolumeLitres(75.0), currentPhase(PHASE_IDLE), phaseStartTime(0),
      waterChangeStartTimestamp(0), currentChangeVolume(0), tempBeforeChange(0), 
      phBeforeChange(0), tdsBeforeChange(0), systemsPaused(false),
      maxTempDifference(2.0), maxPhDifference(0.5),
      maxDrainTime(600000), maxFillTime(600000), stabilizationTime(300000),
      settingsDirty(false), historyDirty(false), lastSaveTime(0) {
    
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
    Serial.printf("Tank volume: %.1f litres\n", tankVolumeLitres);
    Serial.printf("Schedule: %d days, %.1f%% volume\n", (int)schedule, scheduledVolumePercent);
    
    if (eventLogger) {
        eventLogger->info("waterchange", "Water change assistant initialized");
    }
}

void WaterChangeAssistant::update() {
    // Deferred save check
    if ((settingsDirty || historyDirty) && (millis() - lastSaveTime > SAVE_DELAY_MS)) {
        if (settingsDirty) {
            Serial.println("Auto-saving water change settings (deferred)...");
            saveSettings();
        }
        if (historyDirty) {
            Serial.println("Auto-saving water change history (deferred)...");
            saveHistory();
        }
    }
    
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
    tankVolumeLitres = prefs->getFloat("tankVol", 75.0);
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
    prefs->putFloat("tankVol", tankVolumeLitres);
    prefs->putFloat("maxTempDiff", maxTempDifference);
    prefs->putFloat("maxPhDiff", maxPhDifference);
    
    prefs->end();
    Serial.println("Water change settings saved");
    
    settingsDirty = false;
    lastSaveTime = millis();
}

void WaterChangeAssistant::markSettingsDirty() {
    settingsDirty = true;
}

void WaterChangeAssistant::markHistoryDirty() {
    historyDirty = true;
}

void WaterChangeAssistant::forceSave() {
    // Immediate save for critical operations
    if (settingsDirty) {
        Serial.println("Force-saving water change settings...");
        saveSettings();
    }
    if (historyDirty) {
        Serial.println("Force-saving water change history...");
        saveHistory();
    }
}

void WaterChangeAssistant::loadHistory() {
    // Load history from NVS
    prefs->begin("wc_history", false);
    
    int count = prefs->getInt("count", 0);
    Serial.printf("Loading %d water change records from NVS\n", count);
    
    history.clear();
    for (int i = 0; i < count && i < MAX_HISTORY; i++) {
        char key[16];
        WaterChangeRecord record;
        
        // Load each field separately since NVS can't store structs directly
        snprintf(key, sizeof(key), "ts_%d", i);
        record.timestamp = prefs->getULong(key, 0);
        
        snprintf(key, sizeof(key), "vol_%d", i);
        record.volumeChanged = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "tb_%d", i);
        record.tempBefore = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "ta_%d", i);
        record.tempAfter = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "pb_%d", i);
        record.phBefore = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "pa_%d", i);
        record.phAfter = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "tdsb_%d", i);
        record.tdsBefore = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "tdsa_%d", i);
        record.tdsAfter = prefs->getFloat(key, 0.0f);
        
        snprintf(key, sizeof(key), "dur_%d", i);
        record.durationMinutes = prefs->getInt(key, 0);
        
        snprintf(key, sizeof(key), "ok_%d", i);
        record.completedSuccessfully = prefs->getBool(key, false);
        
        if (record.timestamp > 0) {
            history.push_back(record);
        }
    }
    
    prefs->end();
    Serial.printf("Loaded %d water change records from NVS\n", history.size());
}

void WaterChangeAssistant::saveHistory() {
    // Save history to NVS
    prefs->begin("wc_history", false);
    
    // Store count
    prefs->putInt("count", history.size());
    
    // Store each record
    for (int i = 0; i < history.size(); i++) {
        char key[16];
        const WaterChangeRecord& record = history[i];
        
        snprintf(key, sizeof(key), "ts_%d", i);
        prefs->putULong(key, record.timestamp);
        
        snprintf(key, sizeof(key), "vol_%d", i);
        prefs->putFloat(key, record.volumeChanged);
        
        snprintf(key, sizeof(key), "tb_%d", i);
        prefs->putFloat(key, record.tempBefore);
        
        snprintf(key, sizeof(key), "ta_%d", i);
        prefs->putFloat(key, record.tempAfter);
        
        snprintf(key, sizeof(key), "pb_%d", i);
        prefs->putFloat(key, record.phBefore);
        
        snprintf(key, sizeof(key), "pa_%d", i);
        prefs->putFloat(key, record.phAfter);
        
        snprintf(key, sizeof(key), "tdsb_%d", i);
        prefs->putFloat(key, record.tdsBefore);
        
        snprintf(key, sizeof(key), "tdsa_%d", i);
        prefs->putFloat(key, record.tdsAfter);
        
        snprintf(key, sizeof(key), "dur_%d", i);
        prefs->putInt(key, record.durationMinutes);
        
        snprintf(key, sizeof(key), "ok_%d", i);
        prefs->putBool(key, record.completedSuccessfully);
    }
    
    prefs->end();
    Serial.println("Water change history saved to NVS");
    
    historyDirty = false;
    lastSaveTime = millis();
}

void WaterChangeAssistant::addToHistory(const WaterChangeRecord& record) {
    history.push_back(record);
    
    // Keep only most recent records
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin());
    }
    
    markHistoryDirty(); // Mark for deferred save
}

void WaterChangeAssistant::setSchedule(WaterChangeSchedule sched, float volumePercent) {
    schedule = sched;
    scheduledVolumePercent = constrain(volumePercent, 10.0, 50.0); // 10-50% range
    markSettingsDirty(); // Deferred save
    
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
    
    // Get current Unix timestamp
    time_t now;
    time(&now);
    
    // Check if we have a valid time (NTP synced)
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year <= (2020 - 1900)) {
        // NTP not synced, can't calculate days accurately
        return -1;
    }
    
    // Calculate using Unix timestamps
    unsigned long timeSinceLastChange = now - lastChangeTime;
    int daysSinceChange = timeSinceLastChange / 86400;
    
    int daysUntilNext = (int)schedule - daysSinceChange;
    return max(0, daysUntilNext);
}

int WaterChangeAssistant::getDaysSinceLastChange() {
    if (lastChangeTime == 0) {
        return 999; // Never changed
    }
    
    // Get current Unix timestamp
    time_t now;
    time(&now);
    
    // Check if we have a valid time (NTP synced)
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year <= (2020 - 1900)) {
        // NTP not synced, can't calculate days accurately
        return 999;
    }
    
    // Calculate days since last change using Unix timestamps
    unsigned long timeSinceLastChange = now - lastChangeTime;
    return timeSinceLastChange / 86400;
}

bool WaterChangeAssistant::isChangeOverdue() {
    if (schedule == SCHEDULE_NONE) {
        return false;
    }
    
    return getDaysUntilNextChange() == 0;
}

void WaterChangeAssistant::setTankVolume(float litres) {
    tankVolumeLitres = constrain(litres, 20.0, 4000.0); // 20-4000 litre range
    markSettingsDirty(); // Deferred save
    
    Serial.printf("Tank volume set to %.1f litres\n", tankVolumeLitres);
}

float WaterChangeAssistant::getScheduledChangeVolume() {
    return tankVolumeLitres * (scheduledVolumePercent / 100.0);
}

bool WaterChangeAssistant::startWaterChange(float volumeLitres) {
    if (isInProgress()) {
        Serial.println("ERROR: Water change already in progress");
        return false;
    }
    
    // Use scheduled volume if not specified
    if (volumeLitres == 0) {
        volumeLitres = getScheduledChangeVolume();
    }
    
    // Validate volume
    if (volumeLitres > tankVolumeLitres * 0.5) {
        Serial.println("ERROR: Change volume exceeds 50% of tank capacity");
        if (eventLogger) {
            eventLogger->error("waterchange", "Change volume too large - safety limit");
        }
        return false;
    }
    
    currentChangeVolume = volumeLitres;
    currentPhase = PHASE_PREPARE;
    phaseStartTime = millis();
    
    // Capture Unix timestamp when water change starts
    time_t now;
    time(&now);
    waterChangeStartTimestamp = (unsigned long)now;
    
    // Capture pre-change values
    SensorData data = getSensorData();
    tempBeforeChange = data.temperature;
    phBeforeChange = data.ph;
    tdsBeforeChange = data.tds;
    
    // Notify water change predictor
    if (wcPredictor) {
        wcPredictor->startWaterChange();
    }
    
    // Pause control systems
    systemsPaused = true;
    if (heaterRelay) heaterRelay->safetyDisable();
    if (co2Relay) co2Relay->safetyDisable();
    
    Serial.println("\n=== Water Change Started ===");
    Serial.printf("Volume: %.1f litres (%.1f%%)\n", 
                  currentChangeVolume, (currentChangeVolume/tankVolumeLitres)*100);
    Serial.printf("Temperature before: %.1f°C\n", tempBeforeChange);
    Serial.printf("pH before: %.2f\n", phBeforeChange);
    Serial.println("Systems paused for safety");
    Serial.println("===========================\n");
    
    if (eventLogger) {
        char msg[128];
        snprintf(msg, sizeof(msg), 
                 "Water change started: %.1f L (Temp: %.1f°C, pH: %.2f)", 
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
    
    // Check if we have a valid timestamp (NTP synced)
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // Check if time is valid (year > 2020) - this means NTP is synced
    if (timeinfo.tm_year <= (2020 - 1900)) {
        Serial.println("WARNING: NTP not synced - water change will not be logged");
        Serial.println("Please ensure NTP time synchronization is working");
        
        // Still resume systems, but don't log the change
        systemsPaused = false;
        currentPhase = PHASE_COMPLETE;
        currentChangeVolume = 0;
        
        if (eventLogger) {
            eventLogger->warning("waterchange", "Water change completed but not logged - NTP not synced");
        }
        
        sendMQTTAlert("waterchange", "Water change completed - not logged (NTP not synced)", true);
        return false;
    }
    
    // Get final readings
    SensorData data = getSensorData();
    float tempAfter = data.temperature;
    float phAfter = data.ph;
    float tdsAfter = data.tds;
    
    // Create history record
    WaterChangeRecord record;
    record.timestamp = waterChangeStartTimestamp; // Use the timestamp from when water change started
    record.volumeChanged = currentChangeVolume;
    record.tempBefore = tempBeforeChange;
    record.tempAfter = tempAfter;
    record.phBefore = phBeforeChange;
    record.phAfter = phAfter;
    record.tdsBefore = tdsBeforeChange;
    record.tdsAfter = tdsAfter;
    record.durationMinutes = (millis() - phaseStartTime) / 60000;
    record.completedSuccessfully = true;
    
    addToHistory(record);
    
    // Update last change time
    lastChangeTime = waterChangeStartTimestamp;
    markSettingsDirty(); // Deferred save for settings
    forceSave(); // But force save immediately for completed water change (critical)
    
    // Resume systems
    systemsPaused = false;
    currentPhase = PHASE_COMPLETE;
    
    Serial.println("\n=== Water Change Complete ===");
    Serial.printf("Volume changed: %.1f litres\n", currentChangeVolume);
    Serial.printf("Temperature: %.1f°C → %.1f°C (Δ%.2f°C)\n", 
                  tempBeforeChange, tempAfter, tempAfter - tempBeforeChange);
    Serial.printf("pH: %.2f → %.2f (Δ%.2f)\n", 
                  phBeforeChange, phAfter, phAfter - phBeforeChange);
    Serial.printf("TDS: %.0f ppm → %.0f ppm (Δ%.0f ppm)\n", 
                  tdsBeforeChange, tdsAfter, tdsAfter - tdsBeforeChange);
    Serial.printf("Duration: %d minutes\n", record.durationMinutes);
    Serial.println("Systems resumed");
    Serial.println("============================\n");
    
    if (eventLogger) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
                 "Water change completed: %.1f L in %d min (Temp: %.1f→%.1f°C, pH: %.2f→%.2f, TDS: %.0f→%.0f ppm)", 
                 currentChangeVolume, record.durationMinutes, 
                 tempBeforeChange, tempAfter, phBeforeChange, phAfter,
                 tdsBeforeChange, tdsAfter);
        eventLogger->info("waterchange", msg);
    }
    
    sendMQTTAlert("waterchange", "Water change completed successfully - systems resumed", false);
    
    // Notify water change predictor
    if (wcPredictor) {
        float volumePercent = (currentChangeVolume / tankVolumeLitres) * 100.0;
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
