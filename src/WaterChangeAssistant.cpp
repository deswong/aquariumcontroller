#include "WaterChangeAssistant.h"
#include "ConfigManager.h"
#include "SystemTasks.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>

WaterChangeAssistant::WaterChangeAssistant() 
    : schedule(SCHEDULE_NONE), lastChangeTime(0), scheduledVolumePercent(25.0),
      currentPhase(PHASE_IDLE), waterChangeStartTimestamp(0), currentChangeVolume(0),
      tempBeforeChange(0), phBeforeChange(0), tdsBeforeChange(0),
      systemsPaused(false), settingsDirty(false), historyDirty(false), lastSaveTime(0),
      configMgr(nullptr) {
    
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
    loadFilterMaintenanceHistory();
    
    Serial.println("Water Change Assistant initialized (simplified mode)");
    Serial.printf("Tank volume: %.1f litres\n", getTankVolume());
    Serial.printf("Schedule: %d days, %.1f%% volume\n", (int)schedule, scheduledVolumePercent);
    
    if (eventLogger) {
        eventLogger->info("waterchange", "Water change assistant initialized");
    }
}

void WaterChangeAssistant::begin(ConfigManager* config) {
    configMgr = config;
    begin();
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
}

void WaterChangeAssistant::loadSettings() {
    if (!prefs->begin("waterchange", true)) {
        Serial.println("No saved water change settings, using defaults");
        return;
    }
    
    schedule = (WaterChangeSchedule)prefs->getInt("schedule", SCHEDULE_WEEKLY);
    lastChangeTime = prefs->getULong("lastChange", 0);
    scheduledVolumePercent = prefs->getFloat("volumePct", 25.0);
    
    prefs->end();
    Serial.println("Water change settings loaded from NVS");
}

void WaterChangeAssistant::saveSettings() {
    if (!prefs->begin("waterchange", false)) {
        Serial.println("Failed to save water change settings");
        return;
    }
    
    prefs->putInt("schedule", (int)schedule);
    prefs->putULong("lastChange", lastChangeTime);
    prefs->putFloat("volumePct", scheduledVolumePercent);
    
    prefs->end();
    settingsDirty = false;
    lastSaveTime = millis();
    Serial.println("Water change settings saved to NVS");
}

void WaterChangeAssistant::loadHistory() {
    if (!prefs->begin("wc-history", true)) {
        Serial.println("No water change history found");
        return;
    }
    
    int count = prefs->getInt("count", 0);
    count = min(count, MAX_HISTORY);
    
    history.clear();
    for (int i = 0; i < count; i++) {
        String key = "wc" + String(i);
        size_t len = prefs->getBytesLength(key.c_str());
        if (len == sizeof(WaterChangeRecord)) {
            WaterChangeRecord record;
            prefs->getBytes(key.c_str(), &record, sizeof(WaterChangeRecord));
            history.push_back(record);
        }
    }
    
    prefs->end();
    Serial.printf("Loaded %d water change records\n", history.size());
}

void WaterChangeAssistant::saveHistory() {
    if (!prefs->begin("wc-history", false)) {
        Serial.println("Failed to save water change history");
        return;
    }
    
    int count = min((int)history.size(), MAX_HISTORY);
    prefs->putInt("count", count);
    
    for (int i = 0; i < count; i++) {
        String key = "wc" + String(i);
        prefs->putBytes(key.c_str(), &history[i], sizeof(WaterChangeRecord));
    }
    
    prefs->end();
    historyDirty = false;
    lastSaveTime = millis();
    Serial.printf("Saved %d water change records\n", count);
}

void WaterChangeAssistant::loadFilterMaintenanceHistory() {
    if (!prefs->begin("wc-filter", true)) {
        Serial.println("No filter maintenance history found");
        return;
    }
    
    int count = prefs->getInt("count", 0);
    count = min(count, MAX_FILTER_HISTORY);
    
    filterMaintenanceHistory.clear();
    for (int i = 0; i < count; i++) {
        String tsKey = "fm_ts" + String(i);
        String noteKey = "fm_note" + String(i);
        
        FilterMaintenanceRecord record;
        record.timestamp = prefs->getULong(tsKey.c_str(), 0);
        record.notes = prefs->getString(noteKey.c_str(), "");
        
        if (record.timestamp > 0) {
            filterMaintenanceHistory.push_back(record);
        }
    }
    
    prefs->end();
    Serial.printf("Loaded %d filter maintenance records\n", filterMaintenanceHistory.size());
}

void WaterChangeAssistant::saveFilterMaintenanceHistory() {
    if (!prefs->begin("wc-filter", false)) {
        Serial.println("Failed to save filter maintenance history");
        return;
    }
    
    int count = min((int)filterMaintenanceHistory.size(), MAX_FILTER_HISTORY);
    prefs->putInt("count", count);
    
    for (int i = 0; i < count; i++) {
        String tsKey = "fm_ts" + String(i);
        String noteKey = "fm_note" + String(i);
        
        prefs->putULong(tsKey.c_str(), filterMaintenanceHistory[i].timestamp);
        prefs->putString(noteKey.c_str(), filterMaintenanceHistory[i].notes.c_str());
    }
    
    prefs->end();
    Serial.printf("Saved %d filter maintenance records\n", count);
}

void WaterChangeAssistant::addToHistory(const WaterChangeRecord& record) {
    history.insert(history.begin(), record); // Add to front (most recent first)
    
    // Keep only MAX_HISTORY records
    if (history.size() > MAX_HISTORY) {
        history.resize(MAX_HISTORY);
    }
    
    markHistoryDirty();
}

void WaterChangeAssistant::addFilterMaintenance(const FilterMaintenanceRecord& record) {
    filterMaintenanceHistory.insert(filterMaintenanceHistory.begin(), record);
    
    if (filterMaintenanceHistory.size() > MAX_FILTER_HISTORY) {
        filterMaintenanceHistory.resize(MAX_FILTER_HISTORY);
    }
    
    markHistoryDirty(); // Will trigger save of filter history too
    saveFilterMaintenanceHistory(); // Save immediately
}

void WaterChangeAssistant::markSettingsDirty() {
    settingsDirty = true;
}

void WaterChangeAssistant::markHistoryDirty() {
    historyDirty = true;
}

void WaterChangeAssistant::forceSave() {
    if (settingsDirty) {
        saveSettings();
    }
    if (historyDirty) {
        saveHistory();
    }
}

void WaterChangeAssistant::setConfigManager(ConfigManager* config) {
    configMgr = config;
}

float WaterChangeAssistant::getTankVolume() {
    if (configMgr) {
        const SystemConfig& cfg = configMgr->getConfig();
        // Calculate volume in litres from dimensions in cm
        float volumeLitres = (cfg.tankLength * cfg.tankWidth * cfg.tankHeight) / 1000.0f;
        return volumeLitres > 0 ? volumeLitres : 100.0f; // Fallback if dimensions not set
    }
    return 100.0f; // Default fallback
}

float WaterChangeAssistant::getScheduledChangeVolume() {
    return getTankVolume() * (scheduledVolumePercent / 100.0f);
}

void WaterChangeAssistant::setSchedule(WaterChangeSchedule sched, float volumePercent) {
    schedule = sched;
    scheduledVolumePercent = constrain(volumePercent, 10.0f, 50.0f);
    markSettingsDirty();
    
    Serial.printf("Water change schedule updated: %d days, %.1f%% (%.1f litres)\n",
                 (int)schedule, scheduledVolumePercent, getScheduledChangeVolume());
    
    if (eventLogger) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Schedule: every %d days, %.1f%% volume",
                (int)schedule, scheduledVolumePercent);
        eventLogger->info("waterchange", msg);
    }
}

int WaterChangeAssistant::getDaysSinceLastChange() {
    if (lastChangeTime == 0) return -1;
    
    time_t now;
    time(&now);
    return (now - lastChangeTime) / 86400;
}

int WaterChangeAssistant::getDaysUntilNextChange() {
    if (schedule == SCHEDULE_NONE || lastChangeTime == 0) return -1;
    
    int daysSince = getDaysSinceLastChange();
    int daysUntil = (int)schedule - daysSince;
    return max(daysUntil, 0);
}

bool WaterChangeAssistant::isChangeOverdue() {
    if (schedule == SCHEDULE_NONE) return false;
    
    int daysUntil = getDaysUntilNextChange();
    return daysUntil == 0 || getDaysSinceLastChange() > (int)schedule;
}

bool WaterChangeAssistant::startWaterChange(float volumeLitres, float temp, float ph, float tds) {
    if (currentPhase != PHASE_IDLE) {
        Serial.println("ERROR: Water change already in progress");
        return false;
    }
    
    // Get current time
    time_t now;
    time(&now);
    waterChangeStartTimestamp = now;
    
    // Store current readings (volume will be set at the end)
    currentChangeVolume = 0;  // Volume unknown until end of water change
    tempBeforeChange = temp;
    phBeforeChange = ph;
    tdsBeforeChange = tds;
    
    currentPhase = PHASE_IN_PROGRESS;
    
    // Pause heater and CO2 systems for safety
    systemsPaused = true;
    if (heaterRelay) heaterRelay->safetyDisable();
    if (co2Relay) co2Relay->safetyDisable();
    
    Serial.printf("Water change started at %lu\n", now);
    Serial.printf("Initial readings: Temp=%.1f°C, pH=%.2f, TDS=%.0f ppm\n", temp, ph, tds);
    Serial.println("Volume will be recorded at completion");
    
    if (eventLogger) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Started: T=%.1f°C pH=%.2f TDS=%.0f (systems paused, volume TBD)", 
                temp, ph, tds);
        eventLogger->info("waterchange", msg);
    }
    
    return true;
}

bool WaterChangeAssistant::endWaterChange(float temp, float ph, float tds, float volumeLitres) {
    if (currentPhase != PHASE_IN_PROGRESS) {
        Serial.println("ERROR: No water change in progress");
        return false;
    }
    
    // Set the actual volume changed (now known at completion)
    currentChangeVolume = volumeLitres;
    
    // Get end time
    time_t now;
    time(&now);
    
    // Calculate duration
    int durationMinutes = (now - waterChangeStartTimestamp) / 60;
    
    // Create record with before and after readings
    WaterChangeRecord record;
    record.startTimestamp = waterChangeStartTimestamp;
    record.endTimestamp = now;
    record.volumeChanged = currentChangeVolume;
    record.tempBefore = tempBeforeChange;
    record.tempAfter = temp;
    record.phBefore = phBeforeChange;
    record.phAfter = ph;
    record.tdsBefore = tdsBeforeChange;
    record.tdsAfter = tds;
    record.durationMinutes = durationMinutes;
    record.completedSuccessfully = true;
    
    addToHistory(record);
    
    // Update last change time
    lastChangeTime = now;
    markSettingsDirty();
    
    // Resume systems
    systemsPaused = false;
    if (heaterRelay) heaterRelay->safetyEnable();
    if (co2Relay) co2Relay->safetyEnable();
    
    currentPhase = PHASE_COMPLETE;
    
    // Calculate changes for logging
    float tempChange = temp - tempBeforeChange;
    float phChange = ph - phBeforeChange;
    float tdsChange = tds - tdsBeforeChange;
    
    Serial.printf("Water change completed: %.1f litres in %d minutes\n",
                 record.volumeChanged, durationMinutes);
    Serial.printf("Final readings: Temp=%.1f°C (Δ%.1f), pH=%.2f (Δ%.2f), TDS=%.0f (Δ%.0f)\n",
                 temp, tempChange, ph, phChange, tds, tdsChange);
    
    if (eventLogger) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Completed: %.1f L in %d min, T=%.1f°C pH=%.2f TDS=%.0f (systems resumed)",
                record.volumeChanged, durationMinutes, temp, ph, tds);
        eventLogger->info("waterchange", msg);
    }
    
    // Publish water change event to MQTT for ML service
    if (mqttClient && mqttClient->connected() && configMgr) {
        StaticJsonDocument<512> doc;
        doc["startTime"] = record.startTimestamp;
        doc["endTime"] = record.endTimestamp;
        doc["volume"] = record.volumeChanged;
        doc["tempBefore"] = record.tempBefore;
        doc["tempAfter"] = record.tempAfter;
        doc["phBefore"] = record.phBefore;
        doc["phAfter"] = record.phAfter;
        doc["tdsBefore"] = record.tdsBefore;
        doc["tdsAfter"] = record.tdsAfter;
        doc["duration"] = record.durationMinutes;
        doc["successful"] = record.completedSuccessfully;
        
        char buffer[512];
        serializeJson(doc, buffer);
        
        String topic = String(configMgr->getConfig().mqttTopicPrefix) + "/waterchange/event";
        if (mqttClient->publish(topic.c_str(), buffer)) {
            Serial.println("[WC] Water change event published to MQTT");
        } else {
            Serial.println("[WC] Failed to publish water change to MQTT");
        }
    }
    
    // Reset state
    currentChangeVolume = 0;
    waterChangeStartTimestamp = 0;
    tempBeforeChange = 0;
    phBeforeChange = 0;
    tdsBeforeChange = 0;
    
    // Reset to idle after logging
    currentPhase = PHASE_IDLE;
    
    return true;
}

bool WaterChangeAssistant::cancelWaterChange() {
    if (currentPhase != PHASE_IN_PROGRESS) {
        return false;
    }
    
    Serial.println("Water change cancelled");
    
    // Resume systems
    systemsPaused = false;
    if (heaterRelay) heaterRelay->safetyEnable();
    if (co2Relay) co2Relay->safetyEnable();
    
    currentPhase = PHASE_IDLE;
    currentChangeVolume = 0;
    waterChangeStartTimestamp = 0;
    
    if (eventLogger) {
        eventLogger->warning("waterchange", "Cancelled (systems resumed)");
    }
    
    return true;
}

bool WaterChangeAssistant::recordFilterMaintenance(const char* notes) {
    time_t now;
    time(&now);
    
    FilterMaintenanceRecord record;
    record.timestamp = now;
    record.notes = String(notes);
    
    addFilterMaintenance(record);
    
    Serial.printf("Filter maintenance recorded at %lu: %s\n", now, notes);
    
    if (eventLogger) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Filter maintenance: %s", notes);
        eventLogger->info("maintenance", msg);
    }
    
    // Publish filter maintenance event to MQTT for ML service
    if (mqttClient && mqttClient->connected() && configMgr) {
        StaticJsonDocument<384> doc;
        doc["timestamp"] = now;
        doc["filter_type"] = "mechanical";
        doc["days_since_last"] = (int)getDaysSinceLastFilterMaintenance();
        doc["notes"] = notes;
        
        // Try to get current TDS reading before/after (if available)
        // Note: This is simplified - ideally capture TDS before maintenance starts
        if (tdsBeforeChange > 0) {
            doc["tds_before"] = tdsBeforeChange;
        }
        
        char buffer[384];
        serializeJson(doc, buffer);
        
        String topic = String(configMgr->getConfig().mqttTopicPrefix) + "/filter/maintenance";
        if (mqttClient->publish(topic.c_str(), buffer)) {
            Serial.println("[WC] Filter maintenance event published to MQTT");
        } else {
            Serial.println("[WC] Failed to publish filter maintenance to MQTT");
        }
    }
    
    return true;
}

unsigned long WaterChangeAssistant::getDaysSinceLastFilterMaintenance() {
    if (filterMaintenanceHistory.empty()) {
        return 999; // Large number indicating never maintained
    }
    
    time_t now;
    time(&now);
    
    unsigned long lastMaintenance = filterMaintenanceHistory[0].timestamp;
    return (now - lastMaintenance) / 86400;
}

std::vector<FilterMaintenanceRecord> WaterChangeAssistant::getRecentFilterMaintenance(int count) {
    std::vector<FilterMaintenanceRecord> recent;
    int n = min(count, (int)filterMaintenanceHistory.size());
    
    for (int i = 0; i < n; i++) {
        recent.push_back(filterMaintenanceHistory[i]);
    }
    
    return recent;
}

const char* WaterChangeAssistant::getPhaseDescription() {
    switch (currentPhase) {
        case PHASE_IDLE: return "Idle - Ready for water change";
        case PHASE_IN_PROGRESS: return "Water change in progress (systems paused)";
        case PHASE_COMPLETE: return "Water change complete";
        default: return "Unknown";
    }
}

unsigned long WaterChangeAssistant::getElapsedTime() {
    if (currentPhase == PHASE_IDLE) return 0;
    
    time_t now;
    time(&now);
    
    if (waterChangeStartTimestamp > 0) {
        return now - waterChangeStartTimestamp;
    }
    
    return 0;
}

std::vector<WaterChangeRecord> WaterChangeAssistant::getRecentHistory(int count) {
    std::vector<WaterChangeRecord> recent;
    int n = min(count, (int)history.size());
    
    for (int i = 0; i < n; i++) {
        recent.push_back(history[i]);
    }
    
    return recent;
}

WaterChangeRecord WaterChangeAssistant::getHistoryRecord(int index) {
    if (index >= 0 && index < history.size()) {
        return history[index];
    }
    WaterChangeRecord empty = {0};
    return empty;
}

float WaterChangeAssistant::getAverageChangeVolume() {
    if (history.empty()) return 0.0f;
    
    float total = 0.0f;
    for (const auto& record : history) {
        total += record.volumeChanged;
    }
    
    return total / history.size();
}

int WaterChangeAssistant::getTotalChangesThisMonth() {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    int currentMonth = timeinfo->tm_mon;
    int currentYear = timeinfo->tm_year;
    
    int count = 0;
    for (const auto& record : history) {
        time_t recordTime = record.endTimestamp;
        struct tm* recordInfo = localtime(&recordTime);
        
        if (recordInfo->tm_mon == currentMonth && recordInfo->tm_year == currentYear) {
            count++;
        }
    }
    
    return count;
}

float WaterChangeAssistant::getTotalVolumeChangedThisMonth() {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    int currentMonth = timeinfo->tm_mon;
    int currentYear = timeinfo->tm_year;
    
    float total = 0.0f;
    for (const auto& record : history) {
        time_t recordTime = record.endTimestamp;
        struct tm* recordInfo = localtime(&recordTime);
        
        if (recordInfo->tm_mon == currentMonth && recordInfo->tm_year == currentYear) {
            total += record.volumeChanged;
        }
    }
    
    return total;
}
