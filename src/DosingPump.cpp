#include "DosingPump.h"
#include <time.h>

DosingPump::DosingPump(uint8_t in1, uint8_t in2, uint8_t pwmCh)
    : in1Pin(in1), in2Pin(in2), pwmChannel(pwmCh),
      mlPerSecond(0.0), calibrated(false), lastCalibrationTime(0),
      currentState(PUMP_IDLE), stateStartTime(0), currentSpeed(0),
      targetVolume(0), volumePumped(0), maxHistoryRecords(100),
      safetyEnabled(true), maxDoseVolume(50), maxDailyVolume(200),
      dailyVolumeDosed(0), lastDayReset(0), totalRuntime(0),
      lastMaintenanceTime(0), totalDoses(0),
      configDirty(false), historyDirty(false), lastSaveTime(0) {
    
    prefs = new Preferences();
    
    // Initialize schedule config
    scheduleConfig.enabled = false;
    scheduleConfig.schedule = DOSE_MANUAL;
    scheduleConfig.customDays = 7;
    scheduleConfig.hour = 9;
    scheduleConfig.minute = 0;
    scheduleConfig.doseVolume = 5.0;
    scheduleConfig.lastDoseTime = 0;
    scheduleConfig.nextDoseTime = 0;
}

DosingPump::~DosingPump() {
    setMotorCoast();
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

void DosingPump::begin() {
    Serial.println("Initializing Dosing Pump...");
    
    // Configure pins
    pinMode(in1Pin, OUTPUT);
    pinMode(in2Pin, OUTPUT);
    
    // Setup PWM channel (ESP32 LEDC)
    ledcSetup(pwmChannel, 1000, 8); // 1kHz, 8-bit resolution
    ledcAttachPin(in1Pin, pwmChannel);
    
    // Start in coast mode (stopped)
    setMotorCoast();
    
    // Load saved configuration
    loadConfig();
    loadCalibration();
    loadHistory();
    
    // Reset daily volume tracking
    lastDayReset = millis();
    
    Serial.printf("Dosing Pump initialized on pins IN1=%d, IN2=%d\n", in1Pin, in2Pin);
    if (calibrated) {
        Serial.printf("Calibration loaded: %.3f mL/sec\n", mlPerSecond);
    } else {
        Serial.println("⚠️ Pump not calibrated - calibration required");
    }
}

void DosingPump::loadConfig() {
    if (!prefs->begin("dosepump-cfg", true)) {
        Serial.println("Using default dosing pump config");
        return;
    }
    
    scheduleConfig.enabled = prefs->getBool("schedEnabled", false);
    scheduleConfig.schedule = (DosingSchedule)prefs->getInt("schedule", DOSE_MANUAL);
    scheduleConfig.customDays = prefs->getInt("customDays", 7);
    scheduleConfig.hour = prefs->getInt("hour", 9);
    scheduleConfig.minute = prefs->getInt("minute", 0);
    scheduleConfig.doseVolume = prefs->getFloat("doseVolume", 5.0);
    scheduleConfig.lastDoseTime = prefs->getULong("lastDose", 0);
    
    maxDoseVolume = prefs->getInt("maxDose", 50);
    maxDailyVolume = prefs->getInt("maxDaily", 200);
    safetyEnabled = prefs->getBool("safetyOn", true);
    
    totalRuntime = prefs->getULong("totalRuntime", 0);
    totalDoses = prefs->getInt("totalDoses", 0);
    lastMaintenanceTime = prefs->getULong("lastMaint", 0);
    
    prefs->end();
    
    // Calculate next dose time
    if (scheduleConfig.enabled) {
        calculateNextDoseTime();
    }
}

void DosingPump::saveConfig() {
    if (!prefs->begin("dosepump-cfg", false)) {
        Serial.println("Failed to save dosing pump config");
        return;
    }
    
    prefs->putBool("schedEnabled", scheduleConfig.enabled);
    prefs->putInt("schedule", scheduleConfig.schedule);
    prefs->putInt("customDays", scheduleConfig.customDays);
    prefs->putInt("hour", scheduleConfig.hour);
    prefs->putInt("minute", scheduleConfig.minute);
    prefs->putFloat("doseVolume", scheduleConfig.doseVolume);
    prefs->putULong("lastDose", scheduleConfig.lastDoseTime);
    
    prefs->putInt("maxDose", maxDoseVolume);
    prefs->putInt("maxDaily", maxDailyVolume);
    prefs->putBool("safetyOn", safetyEnabled);
    
    prefs->putULong("totalRuntime", totalRuntime);
    prefs->putInt("totalDoses", totalDoses);
    prefs->putULong("lastMaint", lastMaintenanceTime);
    
    prefs->end();
    Serial.println("Dosing pump config saved");
    
    configDirty = false;
    lastSaveTime = millis();
}

void DosingPump::markConfigDirty() {
    configDirty = true;
}

void DosingPump::markHistoryDirty() {
    historyDirty = true;
}

void DosingPump::forceSave() {
    // Immediate save for critical operations
    if (configDirty) {
        Serial.println("Force-saving dosing pump config...");
        saveConfig();
    }
    if (historyDirty) {
        Serial.println("Force-saving dosing pump history...");
        saveHistory();
    }
}

void DosingPump::loadCalibration() {
    if (!prefs->begin("dosepump-cal", true)) {
        Serial.println("No calibration data found");
        return;
    }
    
    mlPerSecond = prefs->getFloat("mlPerSec", 0.0);
    lastCalibrationTime = prefs->getULong("calTime", 0);
    calibrated = (mlPerSecond > 0.0);
    
    prefs->end();
    
    if (calibrated) {
        int daysSince = getDaysSinceCalibration();
        Serial.printf("Calibration loaded: %.3f mL/sec (%d days old)\n", 
                     mlPerSecond, daysSince);
    }
}

void DosingPump::saveCalibration() {
    if (!prefs->begin("dosepump-cal", false)) {
        Serial.println("Failed to save calibration");
        return;
    }
    
    prefs->putFloat("mlPerSec", mlPerSecond);
    prefs->putULong("calTime", millis() / 1000);
    
    prefs->end();
    Serial.printf("Calibration saved: %.3f mL/sec\n", mlPerSecond);
}

void DosingPump::loadHistory() {
    if (!prefs->begin("dosepump-hist", true)) {
        Serial.println("No dosing history found");
        return;
    }
    
    int count = prefs->getInt("count", 0);
    Serial.printf("Loading %d dose records from NVS\n", count);
    
    history.clear();
    for (int i = 0; i < count && i < maxHistoryRecords; i++) {
        DosingRecord record;
        char key[16];
        
        snprintf(key, sizeof(key), "ts_%d", i);
        record.timestamp = prefs->getULong(key, 0);
        
        snprintf(key, sizeof(key), "vol_%d", i);
        record.volumeDosed = prefs->getFloat(key, 0.0);
        
        snprintf(key, sizeof(key), "dur_%d", i);
        record.durationMs = prefs->getInt(key, 0);
        
        snprintf(key, sizeof(key), "ok_%d", i);
        record.success = prefs->getBool(key, true);
        
        snprintf(key, sizeof(key), "type_%d", i);
        record.type = prefs->getString(key, "manual");
        
        history.push_back(record);
    }
    
    prefs->end();
    Serial.printf("Loaded %d dose records\n", history.size());
    
    // Clear dirty flag after load
    historyDirty = false;
}

void DosingPump::saveHistory() {
    if (!prefs->begin("dosepump-hist", false)) {
        Serial.println("Failed to save dosing history");
        return;
    }
    
    // Store count
    prefs->putInt("count", history.size());
    
    // Store each record
    for (int i = 0; i < history.size(); i++) {
        char key[16];
        const DosingRecord& record = history[i];
        
        snprintf(key, sizeof(key), "ts_%d", i);
        prefs->putULong(key, record.timestamp);
        
        snprintf(key, sizeof(key), "vol_%d", i);
        prefs->putFloat(key, record.volumeDosed);
        
        snprintf(key, sizeof(key), "dur_%d", i);
        prefs->putInt(key, record.durationMs);
        
        snprintf(key, sizeof(key), "ok_%d", i);
        prefs->putBool(key, record.success);
        
        snprintf(key, sizeof(key), "type_%d", i);
        prefs->putString(key, record.type.c_str());
    }
    
    prefs->end();
    Serial.printf("Saved %d dose records to NVS\n", history.size());
    
    // Clear dirty flag and update timestamp after successful save
    historyDirty = false;
    lastSaveTime = millis();
}

// ============================================================================
// Motor Control (DRV8871)
// ============================================================================

void DosingPump::setMotorForward(int speed) {
    // Constrain speed 0-100%
    speed = constrain(speed, 0, 100);
    currentSpeed = speed;
    
    if (speed == 0) {
        setMotorCoast();
        return;
    }
    
    // Convert to PWM value (0-255)
    int pwmValue = map(speed, 0, 100, 0, 255);
    
    // IN1 = PWM (forward), IN2 = LOW
    ledcWrite(pwmChannel, pwmValue);
    digitalWrite(in2Pin, LOW);
}

void DosingPump::setMotorReverse(int speed) {
    // Constrain speed 0-100%
    speed = constrain(speed, 0, 100);
    currentSpeed = speed;
    
    if (speed == 0) {
        setMotorCoast();
        return;
    }
    
    // Convert to PWM value (0-255)
    int pwmValue = map(speed, 0, 100, 0, 255);
    
    // IN1 = LOW, IN2 = PWM (reverse)
    ledcWrite(pwmChannel, 0);
    digitalWrite(in2Pin, HIGH);
    analogWrite(in2Pin, pwmValue); // If using software PWM for IN2
}

void DosingPump::setMotorBrake() {
    // Both HIGH = brake
    ledcWrite(pwmChannel, 255);
    digitalWrite(in2Pin, HIGH);
    currentSpeed = 0;
}

void DosingPump::setMotorCoast() {
    // Both LOW = coast (free-wheeling stop)
    ledcWrite(pwmChannel, 0);
    digitalWrite(in2Pin, LOW);
    currentSpeed = 0;
}

// ============================================================================
// Basic Pump Control
// ============================================================================

void DosingPump::start(float volumeML, int speedPercent) {
    if (!calibrated) {
        Serial.println("ERROR: Pump not calibrated");
        currentState = PUMP_ERROR;
        return;
    }
    
    if (currentState == PUMP_DOSING || currentState == PUMP_PRIMING) {
        Serial.println("ERROR: Pump already running");
        return;
    }
    
    // Check safety limits
    if (safetyEnabled) {
        if (volumeML > maxDoseVolume) {
            Serial.printf("ERROR: Volume %.1fmL exceeds max dose %dmL\n", 
                         volumeML, maxDoseVolume);
            currentState = PUMP_ERROR;
            return;
        }
        
        if (dailyVolumeDosed + volumeML > maxDailyVolume) {
            Serial.printf("ERROR: Would exceed daily limit (%.1f/%.1fmL used)\n",
                         dailyVolumeDosed, (float)maxDailyVolume);
            currentState = PUMP_ERROR;
            return;
        }
    }
    
    targetVolume = volumeML;
    volumePumped = 0;
    currentState = PUMP_DOSING;
    stateStartTime = millis();
    
    setMotorForward(speedPercent);
    
    Serial.printf("Dosing started: %.1fmL at %d%% speed\n", volumeML, speedPercent);
}

void DosingPump::stop() {
    if (currentState == PUMP_IDLE) return;
    
    setMotorCoast();
    
    // Record if this was a dose
    if (currentState == PUMP_DOSING && volumePumped > 0) {
        DosingRecord record;
        record.timestamp = millis() / 1000;
        record.volumeDosed = volumePumped;
        record.durationMs = millis() - stateStartTime;
        record.success = (volumePumped >= targetVolume * 0.95); // Within 5%
        record.type = "manual";
        
        addToHistory(record);
        
        dailyVolumeDosed += volumePumped;
        totalDoses++;
        totalRuntime += record.durationMs;
        
        markConfigDirty(); // Deferred save for stats
    }
    
    Serial.printf("Pump stopped. Dosed: %.2fmL in %dms\n", 
                 volumePumped, (int)(millis() - stateStartTime));
    
    currentState = PUMP_IDLE;
    currentSpeed = 0;
    targetVolume = 0;
    volumePumped = 0;
}

void DosingPump::pause() {
    if (currentState != PUMP_DOSING && currentState != PUMP_PRIMING) return;
    
    setMotorCoast();
    currentState = PUMP_PAUSED;
    Serial.println("Pump paused");
}

void DosingPump::resume() {
    if (currentState != PUMP_PAUSED) return;
    
    currentState = PUMP_DOSING;
    setMotorForward(currentSpeed);
    Serial.println("Pump resumed");
}

void DosingPump::emergencyStop() {
    setMotorBrake();
    currentState = PUMP_IDLE;
    currentSpeed = 0;
    Serial.println("🛑 EMERGENCY STOP - Pump braked");
}

// ============================================================================
// Maintenance Functions
// ============================================================================

void DosingPump::prime(int durationSeconds, int speedPercent) {
    if (currentState != PUMP_IDLE) {
        Serial.println("ERROR: Pump busy");
        return;
    }
    
    currentState = PUMP_PRIMING;
    stateStartTime = millis();
    targetVolume = durationSeconds; // Store duration in targetVolume for priming
    volumePumped = 0;
    
    setMotorForward(speedPercent);
    
    Serial.printf("Priming pump for %d seconds at %d%% speed\n", 
                 durationSeconds, speedPercent);
}

void DosingPump::backflush(int durationSeconds, int speedPercent) {
    if (currentState != PUMP_IDLE) {
        Serial.println("ERROR: Pump busy");
        return;
    }
    
    currentState = PUMP_BACKFLUSHING;
    stateStartTime = millis();
    targetVolume = durationSeconds; // Store duration
    
    setMotorReverse(speedPercent);
    
    Serial.printf("Backflushing for %d seconds at %d%% speed\n",
                 durationSeconds, speedPercent);
}

void DosingPump::purge(int durationSeconds, int speedPercent) {
    // Full speed forward to clear lines
    prime(durationSeconds, speedPercent);
    Serial.println("Purging pump lines...");
}

void DosingPump::runCleaning(int cycles) {
    Serial.printf("Running cleaning cycle (%d cycles)...\n", cycles);
    
    for (int i = 0; i < cycles; i++) {
        Serial.printf("Cycle %d/%d\n", i + 1, cycles);
        
        // Forward
        prime(3, 50);
        while (currentState == PUMP_PRIMING) {
            update();
            delay(10);
        }
        delay(500);
        
        // Reverse
        backflush(3, 30);
        while (currentState == PUMP_BACKFLUSHING) {
            update();
            delay(10);
        }
        delay(500);
    }
    
    lastMaintenanceTime = millis() / 1000;
    forceSave(); // Force save after maintenance (important)
    
    Serial.println("Cleaning cycle complete");
}

// ============================================================================
// Calibration
// ============================================================================

void DosingPump::startCalibration(int speedPercent) {
    if (currentState != PUMP_IDLE) {
        Serial.println("ERROR: Pump busy");
        return;
    }
    
    currentState = PUMP_CALIBRATING;
    stateStartTime = millis();
    volumePumped = 0;
    
    setMotorForward(speedPercent);
    
    Serial.println("========================================");
    Serial.println("CALIBRATION MODE");
    Serial.println("========================================");
    Serial.printf("Pump running at %d%% speed\n", speedPercent);
    Serial.println("Collect the output in a measuring cylinder");
    Serial.println("Call finishCalibration(measuredML, seconds) when done");
    Serial.println("========================================");
}

void DosingPump::finishCalibration(float measuredVolumeML, int durationSeconds) {
    if (currentState != PUMP_CALIBRATING) {
        Serial.println("ERROR: Not in calibration mode");
        return;
    }
    
    setMotorCoast();
    
    // Calculate flow rate
    mlPerSecond = measuredVolumeML / (float)durationSeconds;
    calibrated = true;
    lastCalibrationTime = millis() / 1000;
    
    saveCalibration();
    
    Serial.println("========================================");
    Serial.println("CALIBRATION COMPLETE");
    Serial.println("========================================");
    Serial.printf("Measured: %.2f mL in %d seconds\n", measuredVolumeML, durationSeconds);
    Serial.printf("Flow rate: %.3f mL/sec\n", mlPerSecond);
    Serial.printf("Estimated max: %.1f mL/min\n", mlPerSecond * 60);
    Serial.println("========================================");
    
    currentState = PUMP_IDLE;
}

void DosingPump::cancelCalibration() {
    if (currentState == PUMP_CALIBRATING) {
        setMotorCoast();
        currentState = PUMP_IDLE;
        Serial.println("Calibration cancelled");
    }
}

bool DosingPump::isCalibrated() {
    return calibrated;
}

int DosingPump::getDaysSinceCalibration() {
    if (!calibrated) return -1;
    unsigned long currentTime = millis() / 1000;
    return (currentTime - lastCalibrationTime) / (60 * 60 * 24);
}

// ============================================================================
// Dosing Control
// ============================================================================

void DosingPump::doseManual(float volumeML, int speedPercent) {
    start(volumeML, speedPercent);
}

void DosingPump::doseScheduled() {
    if (!calibrated) {
        Serial.println("Cannot dose: Not calibrated");
        return;
    }
    
    start(scheduleConfig.doseVolume, 100);
    
    scheduleConfig.lastDoseTime = millis() / 1000;
    calculateNextDoseTime();
    markConfigDirty(); // Deferred save
    
    // Update history with scheduled type
    DosingRecord record;
    record.timestamp = scheduleConfig.lastDoseTime;
    record.volumeDosed = scheduleConfig.doseVolume;
    record.type = "scheduled";
    record.success = true;
    // Will be updated when dose completes
}

bool DosingPump::isDosing() {
    return (currentState == PUMP_DOSING);
}

float DosingPump::getProgress() {
    if (targetVolume <= 0) return 0.0;
    return constrain(volumePumped / targetVolume, 0.0, 1.0);
}

// ============================================================================
// Schedule Management
// ============================================================================

void DosingPump::setSchedule(DosingSchedule schedule, int hour, int minute, float volumeML) {
    scheduleConfig.schedule = schedule;
    scheduleConfig.hour = hour;
    scheduleConfig.minute = minute;
    scheduleConfig.doseVolume = volumeML;
    
    calculateNextDoseTime();
    markConfigDirty(); // Deferred save
    
    Serial.printf("Schedule set: %s at %02d:%02d, %.1fmL\n",
                 schedule == DOSE_DAILY ? "DAILY" : 
                 schedule == DOSE_WEEKLY ? "WEEKLY" : "CUSTOM",
                 hour, minute, volumeML);
}

void DosingPump::setCustomSchedule(int days, int hour, int minute, float volumeML) {
    scheduleConfig.schedule = DOSE_CUSTOM;
    scheduleConfig.customDays = days;
    scheduleConfig.hour = hour;
    scheduleConfig.minute = minute;
    scheduleConfig.doseVolume = volumeML;
    
    calculateNextDoseTime();
    markConfigDirty(); // Deferred save
    
    Serial.printf("Custom schedule set: Every %d days at %02d:%02d, %.1fmL\n",
                 days, hour, minute, volumeML);
}

void DosingPump::enableSchedule(bool enable) {
    scheduleConfig.enabled = enable;
    if (enable) {
        calculateNextDoseTime();
    }
    markConfigDirty(); // Deferred save
    Serial.printf("Schedule %s\n", enable ? "ENABLED" : "DISABLED");
}

bool DosingPump::isScheduleEnabled() {
    return scheduleConfig.enabled;
}

DosingScheduleConfig DosingPump::getScheduleConfig() {
    return scheduleConfig;
}

int DosingPump::calculateScheduleInterval() {
    switch (scheduleConfig.schedule) {
        case DOSE_DAILY:
            return 1;
        case DOSE_WEEKLY:
            return 7;
        case DOSE_CUSTOM:
            return scheduleConfig.customDays;
        default:
            return 0;
    }
}

void DosingPump::calculateNextDoseTime() {
    if (!scheduleConfig.enabled || scheduleConfig.schedule == DOSE_MANUAL) {
        scheduleConfig.nextDoseTime = 0;
        return;
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Calculate next dose time
    int intervalDays = calculateScheduleInterval();
    if (intervalDays == 0) return;
    
    // Start from last dose or now
    time_t baseTime = (scheduleConfig.lastDoseTime > 0) ? 
                      scheduleConfig.lastDoseTime : now;
    
    // Add interval days
    baseTime += intervalDays * 24 * 60 * 60;
    
    // Set to scheduled hour/minute
    localtime_r(&baseTime, &timeinfo);
    timeinfo.tm_hour = scheduleConfig.hour;
    timeinfo.tm_min = scheduleConfig.minute;
    timeinfo.tm_sec = 0;
    
    scheduleConfig.nextDoseTime = mktime(&timeinfo);
}

int DosingPump::getHoursUntilNextDose() {
    if (scheduleConfig.nextDoseTime == 0) return -1;
    
    time_t now;
    time(&now);
    
    int seconds = scheduleConfig.nextDoseTime - now;
    if (seconds < 0) return 0;
    
    return seconds / 3600;
}

bool DosingPump::isDoseOverdue() {
    if (!scheduleConfig.enabled || scheduleConfig.nextDoseTime == 0) return false;
    
    time_t now;
    time(&now);
    
    return now > scheduleConfig.nextDoseTime;
}

// ============================================================================
// Safety
// ============================================================================

void DosingPump::setSafetyLimits(int maxDoseML, int maxDailyML) {
    maxDoseVolume = maxDoseML;
    maxDailyVolume = maxDailyML;
    markConfigDirty(); // Deferred save
    Serial.printf("Safety limits set: %dmL per dose, %dmL per day\n",
                 maxDoseML, maxDailyML);
}

void DosingPump::setSafetyEnabled(bool enable) {
    safetyEnabled = enable;
    markConfigDirty(); // Deferred save
    Serial.printf("Safety limits %s\n", enable ? "ENABLED" : "DISABLED");
}

bool DosingPump::isSafetyEnabled() {
    return safetyEnabled;
}

float DosingPump::getDailyVolumeDosed() {
    return dailyVolumeDosed;
}

float DosingPump::getRemainingDailyVolume() {
    return max(0.0f, (float)maxDailyVolume - dailyVolumeDosed);
}

void DosingPump::resetDailyVolume() {
    dailyVolumeDosed = 0;
    lastDayReset = millis();
    Serial.println("Daily volume counter reset");
}

// ============================================================================
// State & Status
// ============================================================================

PumpState DosingPump::getState() {
    return currentState;
}

String DosingPump::getStateString() {
    switch (currentState) {
        case PUMP_IDLE: return "Idle";
        case PUMP_DOSING: return "Dosing";
        case PUMP_PRIMING: return "Priming";
        case PUMP_BACKFLUSHING: return "Backflushing";
        case PUMP_CALIBRATING: return "Calibrating";
        case PUMP_PAUSED: return "Paused";
        case PUMP_ERROR: return "Error";
        default: return "Unknown";
    }
}

float DosingPump::getFlowRate() {
    return mlPerSecond;
}

int DosingPump::getCurrentSpeed() {
    return currentSpeed;
}

float DosingPump::getTargetVolume() {
    return targetVolume;
}

float DosingPump::getVolumePumped() {
    return volumePumped;
}

int DosingPump::getElapsedTime() {
    if (currentState == PUMP_IDLE) return 0;
    return millis() - stateStartTime;
}

// ============================================================================
// History
// ============================================================================

void DosingPump::addToHistory(DosingRecord record) {
    // Add to history immediately
    history.push_back(record);
    
    // Limit history size
    while (history.size() > (size_t)maxHistoryRecords) {
        history.erase(history.begin());
    }
    
    // Save immediately to prevent data loss (only 1 dose/day expected)
    Serial.println("Saving dose history immediately...");
    saveHistory();
}

std::vector<DosingRecord> DosingPump::getHistory(int count) {
    std::vector<DosingRecord> recent;
    
    int start = max(0, (int)history.size() - count);
    
    for (size_t i = start; i < history.size(); i++) {
        recent.push_back(history[i]);
    }
    
    return recent;
}

int DosingPump::getHistoryCount() {
    return history.size();
}

void DosingPump::clearHistory() {
    history.clear();
    Serial.println("Dosing history cleared");
}

// ============================================================================
// Statistics
// ============================================================================

unsigned long DosingPump::getTotalRuntime() {
    return totalRuntime;
}

int DosingPump::getTotalDoses() {
    return totalDoses;
}

float DosingPump::getAverageDoseVolume() {
    if (history.empty()) return 0.0;
    
    float total = 0;
    for (const auto& record : history) {
        total += record.volumeDosed;
    }
    
    return total / history.size();
}

float DosingPump::getTotalVolumeDosed() {
    float total = 0;
    for (const auto& record : history) {
        total += record.volumeDosed;
    }
    return total;
}

unsigned long DosingPump::getLastMaintenanceTime() {
    return lastMaintenanceTime;
}

void DosingPump::resetMaintenanceCounter() {
    lastMaintenanceTime = millis() / 1000;
    totalRuntime = 0;
    saveConfig();
    Serial.println("Maintenance counter reset");
}

// ============================================================================
// Update Loop
// ============================================================================

void DosingPump::update() {
    // Deferred save check
    if ((configDirty || historyDirty) && 
        (millis() - lastSaveTime > SAVE_DELAY_MS)) {
        if (configDirty) {
            Serial.println("Auto-saving dosing pump config (deferred)...");
            saveConfig();
        }
        if (historyDirty) {
            Serial.println("Auto-saving dosing pump history (deferred)...");
            saveHistory();
        }
    }
    
    // Reset daily volume at midnight
    if (millis() - lastDayReset > 24 * 60 * 60 * 1000) {
        resetDailyVolume();
    }
    
    // Check scheduled doses
    if (scheduleConfig.enabled && isDoseOverdue() && currentState == PUMP_IDLE) {
        Serial.println("Scheduled dose triggered");
        doseScheduled();
    }
    
    // Update volume tracking when dosing
    if (currentState == PUMP_DOSING && calibrated) {
        updateVolumeTracking();
        
        // Stop when target reached
        if (volumePumped >= targetVolume) {
            stop();
        }
    }
    
    // Handle timed operations (priming, backflushing)
    if ((currentState == PUMP_PRIMING || currentState == PUMP_BACKFLUSHING) && 
        targetVolume > 0) {
        int elapsed = (millis() - stateStartTime) / 1000;
        if (elapsed >= (int)targetVolume) {
            setMotorCoast();
            currentState = PUMP_IDLE;
            Serial.printf("%s complete\n", 
                         currentState == PUMP_PRIMING ? "Priming" : "Backflushing");
        }
    }
}

void DosingPump::updateVolumeTracking() {
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    if (lastUpdate == 0) {
        lastUpdate = now;
        return;
    }
    
    float deltaSeconds = (now - lastUpdate) / 1000.0;
    float speedFactor = currentSpeed / 100.0;
    float volumeAdded = mlPerSecond * speedFactor * deltaSeconds;
    
    volumePumped += volumeAdded;
    lastUpdate = now;
}

// ============================================================================
// JSON Status
// ============================================================================

String DosingPump::getStatusJSON() {
    String json = "{";
    json += "\"state\":\"" + getStateString() + "\",";
    json += "\"calibrated\":" + String(calibrated ? "true" : "false") + ",";
    json += "\"flowRate\":" + String(mlPerSecond, 3) + ",";
    json += "\"currentSpeed\":" + String(currentSpeed) + ",";
    json += "\"targetVolume\":" + String(targetVolume, 2) + ",";
    json += "\"volumePumped\":" + String(volumePumped, 2) + ",";
    json += "\"progress\":" + String(getProgress(), 2) + ",";
    json += "\"elapsedTime\":" + String(getElapsedTime()) + ",";
    json += "\"scheduleEnabled\":" + String(scheduleConfig.enabled ? "true" : "false") + ",";
    json += "\"hoursUntilNext\":" + String(getHoursUntilNextDose()) + ",";
    json += "\"dailyVolume\":" + String(dailyVolumeDosed, 2) + ",";
    json += "\"remainingDaily\":" + String(getRemainingDailyVolume(), 2) + ",";
    json += "\"totalDoses\":" + String(totalDoses);
    json += "}";
    return json;
}
