# NVS Write Optimization Guide

## Overview
NVS (Non-Volatile Storage) flash memory has limited write endurance (~100,000 write cycles per sector). Reducing unnecessary writes extends the ESP32's lifespan and improves performance.

## Current NVS Write Analysis

### High-Frequency Write Operations

#### 1. **ConfigManager** - Every setter calls `save()`
**Location**: `src/ConfigManager.cpp`

**Problem**: Each configuration change triggers a full save of all ~20 settings:
```cpp
void ConfigManager::setWiFi(const char* ssid, const char* password) {
    strncpy(config.wifiSSID, ssid, sizeof(config.wifiSSID) - 1);
    strncpy(config.wifiPassword, password, sizeof(config.wifiPassword) - 1);
    save();  // ❌ Immediate write
}

void ConfigManager::setTemperatureTarget(float target, float safetyMax) {
    config.tempTarget = target;
    config.tempSafetyMax = safetyMax;
    save();  // ❌ Immediate write
}

void ConfigManager::setTankDimensions(float length, float width, float height) {
    config.tankLength = length;
    config.tankWidth = width;
    config.tankHeight = height;
    save();  // ❌ Immediate write
}
```

**Impact**: 
- Web UI settings page saves ~10 values = 10 NVS writes
- User adjusting multiple settings = Multiple writes in quick succession
- Each save writes ALL settings (not just changed ones)

#### 2. **WaterChangeAssistant** - Multiple save points
**Location**: `src/WaterChangeAssistant.cpp`

**Problem**: Settings saved multiple times during operations:
```cpp
void WaterChangeAssistant::setSchedule(ScheduleType type) {
    schedule = type;
    saveSettings();  // ❌ Line 211
}

void WaterChangeAssistant::setTankVolume(float litres) {
    tankVolumeLitres = litres;
    saveSettings();  // ❌ Line 281
}

void WaterChangeAssistant::completeWaterChange() {
    // ... logic ...
    saveSettings();  // ❌ Line 498
}

void WaterChangeAssistant::updateProgress(float volume, float temp, float ph, float tds) {
    // ... logic ...
    saveSettings();  // ❌ Line 582 (in some cases)
}
```

**Impact**:
- Water change configuration = 2-3 NVS writes
- Each completed water change = 1 settings + 1 history write

#### 3. **DosingPump** - 8 save operations
**Location**: `src/DosingPump.cpp`

**Problem**: Multiple functions call `saveConfig()`:
```cpp
void DosingPump::stop() {
    // ... complete dose ...
    saveConfig();  // ❌ Line 269 - Every dose completion
}

void DosingPump::calibrate(...) {
    // ... calibration ...
    saveConfig();  // ❌ Line 371 - Every calibration
}

void DosingPump::setSchedule(...) {
    // ... set schedule ...
    saveConfig();  // ❌ Line 494 - Schedule changes
}

void DosingPump::setSafetyLimits(...) {
    maxSingleDoseML = maxDoseML;
    maxDailyVolumeML = maxDailyML;
    saveConfig();  // ❌ Lines 605, 612
}
```

**Impact**:
- Each dose completion = 1 NVS write
- Multiple doses per day = Multiple writes
- Configuration changes during setup = Multiple writes

#### 4. **PatternLearner** - 4 save operations
**Location**: `src/PatternLearner.cpp`

**Problem**: Pattern updates trigger saves:
```cpp
void PatternLearner::setConfig(PatternConfig newConfig) {
    config = newConfig;
    saveConfig();  // ❌ Line 614
}

void PatternLearner::setEnabled(bool enabled) {
    config.enabled = enabled;
    saveConfig();  // ❌ Line 623
}

void PatternLearner::setAnomalyThresholds(...) {
    // ... set thresholds ...
    saveConfig();  // ❌ Line 634
}

void PatternLearner::setAutoSeasonalAdapt(bool enable) {
    config.autoSeasonalAdapt = enable;
    saveConfig();  // ❌ Line 639
}
```

**Impact**:
- Pattern learning configuration = Multiple writes
- Each setting change = Immediate NVS write

## Optimization Strategies

### Strategy 1: Deferred Saving (Dirty Flag Pattern)

**Benefit**: Batch multiple changes into a single NVS write

**Implementation**:
```cpp
class ConfigManager {
private:
    bool isDirty = false;
    unsigned long lastSaveTime = 0;
    const unsigned long SAVE_DELAY_MS = 5000; // 5 seconds

public:
    void markDirty() {
        isDirty = true;
    }
    
    void update() {
        // Call this in main loop
        if (isDirty && (millis() - lastSaveTime > SAVE_DELAY_MS)) {
            save();
            isDirty = false;
            lastSaveTime = millis();
        }
    }
    
    void setWiFi(const char* ssid, const char* password) {
        strncpy(config.wifiSSID, ssid, sizeof(config.wifiSSID) - 1);
        strncpy(config.wifiPassword, password, sizeof(config.wifiPassword) - 1);
        markDirty();  // ✅ Deferred save
    }
    
    void forceSave() {
        // For critical operations that need immediate save
        if (isDirty) {
            save();
            isDirty = false;
            lastSaveTime = millis();
        }
    }
};
```

**Result**: 10 setting changes = 1 NVS write (instead of 10)

### Strategy 2: Separate Critical vs Non-Critical Settings

**Benefit**: Only save changed values, not entire config

**Implementation**:
```cpp
class ConfigManager {
public:
    void savePartial(const char* namespace, const char* key, float value) {
        prefs->begin(namespace, false);
        prefs->putFloat(key, value);
        prefs->end();
    }
    
    void setTemperatureTarget(float target, float safetyMax) {
        config.tempTarget = target;
        config.tempSafetyMax = safetyMax;
        // ✅ Save only these 2 values
        savePartial("system-config", "tempTarget", target);
        savePartial("system-config", "tempSafetyMax", safetyMax);
    }
};
```

**Result**: Smaller NVS writes, faster saves

### Strategy 3: Session-Based Saving

**Benefit**: Only save when user explicitly saves or on reboot

**Implementation**:
```cpp
// In main.cpp
void onSaveSettings() {
    // Web UI "Save Settings" button triggers this
    config->forceSave();
    waterChangeAssistant->forceSave();
    dosingPump->forceSave();
    patternLearner->forceSave();
}

void onReboot() {
    // Before reboot, save everything
    config->forceSave();
    waterChangeAssistant->forceSave();
    dosingPump->forceSave();
    patternLearner->forceSave();
}
```

**Result**: User controls when saves happen

### Strategy 4: Reduce DosingPump History Writes

**Problem**: Every dose completion writes history

**Solution**: Batch history records
```cpp
class DosingPump {
private:
    std::vector<DoseRecord> pendingHistory;
    
public:
    void stop() {
        // ... dose completion ...
        pendingHistory.push_back(record);
        
        // Only save history every 5 doses or on shutdown
        if (pendingHistory.size() >= 5) {
            flushHistory();
        }
    }
    
    void flushHistory() {
        for (auto& record : pendingHistory) {
            addToHistory(record);
        }
        saveHistory();
        pendingHistory.clear();
    }
};
```

**Result**: 5 doses = 1 NVS write (instead of 5)

### Strategy 5: Use RAM for Volatile Data

**Problem**: Saving runtime stats that don't need persistence

**Solution**: Separate volatile from persistent data
```cpp
class DosingPump {
private:
    // Persistent (saved to NVS)
    float mlPerSecondCalibration;
    DosingSchedule schedule;
    
    // Volatile (RAM only, reset on boot)
    unsigned long dailyVolumeDosed;  // Reset daily
    unsigned long totalRuntime;      // Reset on boot
    
public:
    void saveConfig() {
        // ✅ Only save persistent data
        prefs->putFloat("mlPerSec", mlPerSecondCalibration);
        prefs->putInt("schedule", (int)schedule);
        // ❌ Don't save dailyVolumeDosed or totalRuntime
    }
};
```

**Result**: Fewer writes, faster saves

## Recommended Implementation Plan

### Phase 1: Immediate Wins (Low Risk)
1. ✅ Add dirty flag to ConfigManager
2. ✅ Add `update()` method with 5-second save delay
3. ✅ Keep `forceSave()` for critical operations (reboot, manual save)

**Estimated Reduction**: 70-80% fewer writes

### Phase 2: Batch Operations (Medium Risk)
1. ✅ Batch DosingPump history (save every 5 doses)
2. ✅ Batch WaterChangeAssistant history
3. ✅ Add explicit "Save" button in Web UI

**Estimated Reduction**: 50-60% fewer writes

### Phase 3: Selective Saving (Higher Risk)
1. ✅ Track which settings changed
2. ✅ Only write changed values to NVS
3. ✅ Separate volatile from persistent data

**Estimated Reduction**: 40-50% fewer writes

## Expected Results

### Current Estimated Daily Writes
- DosingPump: 10 doses/day × 1 write = **10 writes**
- WaterChangeAssistant: 1 change/week = **0.14 writes/day**
- PatternLearner: 24 updates/day × 0 (read-only) = **0 writes**
- ConfigManager: 2 setting changes/day = **2 writes**
- **Total: ~12 writes/day**

### After Optimization
- DosingPump: 10 doses ÷ 5 batches = **2 writes/day**
- WaterChangeAssistant: 1 change/week = **0.14 writes/day**
- ConfigManager: 2 changes batched = **0.4 writes/day**
- **Total: ~2.5 writes/day** (80% reduction)

### Lifespan Calculation
- NVS endurance: ~100,000 write cycles
- Current rate: 12 writes/day = **22 years lifespan**
- Optimized rate: 2.5 writes/day = **109 years lifespan** ✅

## Implementation Code

### ConfigManager with Dirty Flag

```cpp
// Add to ConfigManager.h
class ConfigManager {
private:
    bool isDirty;
    unsigned long lastSaveTime;
    static const unsigned long SAVE_DELAY_MS = 5000;

public:
    void markDirty();
    void update();
    void forceSave();
};

// Add to ConfigManager.cpp
void ConfigManager::markDirty() {
    isDirty = true;
}

void ConfigManager::update() {
    if (isDirty && (millis() - lastSaveTime > SAVE_DELAY_MS)) {
        save();
        isDirty = false;
        lastSaveTime = millis();
    }
}

void ConfigManager::forceSave() {
    if (isDirty) {
        save();
        isDirty = false;
        lastSaveTime = millis();
    }
}

// Modify existing setters
void ConfigManager::setWiFi(const char* ssid, const char* password) {
    strncpy(config.wifiSSID, ssid, sizeof(config.wifiSSID) - 1);
    strncpy(config.wifiPassword, password, sizeof(config.wifiPassword) - 1);
    markDirty();  // Changed from save()
}
```

### Main Loop Update

```cpp
// In main.cpp loop()
void loop() {
    config->update();  // Periodic save if dirty
    // ... rest of loop ...
}
```

### Web UI Explicit Save

```javascript
// In index.html
async function saveSettings() {
    // Collect all settings
    const settings = { /* ... */ };
    
    // Send to backend
    await fetch('/api/config', {
        method: 'POST',
        body: JSON.stringify(settings)
    });
    
    // Force immediate save
    await fetch('/api/config/save', { method: 'POST' });
    
    showAlert('✅ Settings saved to flash memory', 'success');
}
```

## Monitoring NVS Health

Add diagnostics endpoint:
```cpp
void WebServer::handleNVSStats() {
    StaticJsonDocument<512> doc;
    
    size_t used = nvs_get_used_entry_count("nvs");
    size_t total = nvs_get_free_entry_count("nvs") + used;
    
    doc["used"] = used;
    doc["total"] = total;
    doc["percent"] = (used * 100) / total;
    doc["writes_today"] = nvsWriteCounter;
    
    String json;
    serializeJson(doc, json);
    server->send(200, "application/json", json);
}
```

## Summary

✅ **Dirty Flag Pattern**: Batch multiple changes = 70-80% reduction  
✅ **History Batching**: Save every N records = 50-60% reduction  
✅ **Selective Saves**: Only write changed values = 40-50% reduction  
✅ **Explicit Save Button**: User control = Better UX  
✅ **Lifespan Impact**: 22 years → 109 years  

**Recommendation**: Implement Phase 1 (dirty flag) immediately as it's low-risk and high-reward.
