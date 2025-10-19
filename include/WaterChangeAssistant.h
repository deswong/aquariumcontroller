#ifndef WATER_CHANGE_ASSISTANT_H
#define WATER_CHANGE_ASSISTANT_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// Water change schedule types
enum WaterChangeSchedule {
    SCHEDULE_NONE = 0,
    SCHEDULE_DAILY = 1,
    SCHEDULE_WEEKLY = 7,
    SCHEDULE_BIWEEKLY = 14,
    SCHEDULE_MONTHLY = 30
};

// Water change phases
enum WaterChangePhase {
    PHASE_IDLE = 0,
    PHASE_PREPARE,      // Pause systems
    PHASE_DRAINING,     // Draining old water
    PHASE_DRAINED,      // Waiting for user to add new water
    PHASE_FILLING,      // Filling with new water
    PHASE_STABILIZING,  // Temperature/pH stabilization
    PHASE_COMPLETE      // Resume normal operation
};

// Water change record
struct WaterChangeRecord {
    unsigned long timestamp;    // Unix timestamp (seconds since epoch)
    float volumeChanged;        // Liters
    float tempBefore;
    float tempAfter;
    float phBefore;
    float phAfter;
    float tdsBefore;
    float tdsAfter;
    int durationMinutes;
    bool completedSuccessfully;
};

class WaterChangeAssistant {
private:
    Preferences* prefs;
    
    // Schedule settings
    WaterChangeSchedule schedule;
    unsigned long lastChangeTime;    // Seconds since boot
    float scheduledVolumePercent;    // Percentage of tank volume
    float tankVolumeLitres;
    
    // Current water change state
    WaterChangePhase currentPhase;
    unsigned long phaseStartTime;
    unsigned long waterChangeStartTimestamp; // Unix timestamp when water change started
    float currentChangeVolume;
    float tempBeforeChange;
    float phBeforeChange;
    float tdsBeforeChange;
    bool systemsPaused;
    
    // History tracking
    std::vector<WaterChangeRecord> history;
    static const int MAX_HISTORY = 50;
    
    // Safety parameters
    float maxTempDifference;      // Max safe temp difference (°C)
    float maxPhDifference;        // Max safe pH difference
    unsigned long maxDrainTime;   // Max time for drain phase (ms)
    unsigned long maxFillTime;    // Max time for fill phase (ms)
    unsigned long stabilizationTime; // Time to wait for stabilization (ms)
    
    // Deferred saving (dirty flag pattern)
    bool settingsDirty;
    bool historyDirty;
    unsigned long lastSaveTime;
    static const unsigned long SAVE_DELAY_MS = 5000; // 5 seconds
    
    void loadSettings();
    void saveSettings();
    void loadHistory();
    void saveHistory();
    void addToHistory(const WaterChangeRecord& record);
    
    void markSettingsDirty();
    void markHistoryDirty();

public:
    WaterChangeAssistant();
    ~WaterChangeAssistant();
    
    void begin();
    void update();
    
    // Schedule management
    void setSchedule(WaterChangeSchedule sched, float volumePercent);
    WaterChangeSchedule getSchedule() { return schedule; }
    int getDaysUntilNextChange();
    int getDaysSinceLastChange();
    bool isChangeOverdue();
    
    // Tank configuration
    void setTankVolume(float litres);
    float getTankVolume() { return tankVolumeLitres; }
    float getScheduledChangeVolume(); // Returns litres
    
    // Water change operations
    bool startWaterChange(float volumeLitres = 0); // 0 = use scheduled volume
    bool advancePhase();
    bool cancelWaterChange();
    bool completeWaterChange();
    
    // Deferred save management
    void forceSave(); // Immediate save for critical operations
    
    // Phase control
    WaterChangePhase getCurrentPhase() { return currentPhase; }
    const char* getPhaseDescription();
    unsigned long getPhaseElapsedTime(); // Returns seconds
    bool isInProgress() { return currentPhase != PHASE_IDLE && currentPhase != PHASE_COMPLETE; }
    
    // Safety checks
    bool isSafeToFill(float currentTemp, float currentPh);
    void setSafetyLimits(float maxTempDiff, float maxPhDiff);
    bool areSystemsPaused() { return systemsPaused; }
    
    // History and statistics
    int getHistoryCount() { return history.size(); }
    WaterChangeRecord getHistoryRecord(int index);
    std::vector<WaterChangeRecord> getRecentHistory(int count);
    float getAverageChangeVolume();
    int getTotalChangesThisMonth();
    float getTotalVolumeChangedThisMonth();
    
    // Status
    float getCurrentChangeVolume() { return currentChangeVolume; }
    float getTempBeforeChange() { return tempBeforeChange; }
    float getPhBeforeChange() { return phBeforeChange; }
    float getTdsBeforeChange() { return tdsBeforeChange; }
};

#endif
