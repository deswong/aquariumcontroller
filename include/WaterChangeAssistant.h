#ifndef WATER_CHANGE_ASSISTANT_H
#define WATER_CHANGE_ASSISTANT_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// Forward declaration
class ConfigManager;

// Water change schedule types
enum WaterChangeSchedule {
    SCHEDULE_NONE = 0,
    SCHEDULE_DAILY = 1,
    SCHEDULE_WEEKLY = 7,
    SCHEDULE_BIWEEKLY = 14,
    SCHEDULE_MONTHLY = 30
};

// Water change phases (simplified)
enum WaterChangePhase {
    PHASE_IDLE = 0,
    PHASE_IN_PROGRESS,  // Water change in progress (systems paused)
    PHASE_COMPLETE      // Resume normal operation
};

// Water change record (simplified)
struct WaterChangeRecord {
    unsigned long startTimestamp;   // Unix timestamp when started (seconds since epoch)
    unsigned long endTimestamp;     // Unix timestamp when ended
    float volumeChanged;            // Liters
    float tempBefore;               // Temperature before change (°C)
    float tempAfter;                // Temperature after change (°C)
    float phBefore;                 // pH before change
    float phAfter;                  // pH after change
    float tdsBefore;                // TDS before change (ppm)
    float tdsAfter;                 // TDS after change (ppm)
    int durationMinutes;
    bool completedSuccessfully;
};

// Filter maintenance record
struct FilterMaintenanceRecord {
    unsigned long timestamp;        // Unix timestamp when maintenance performed
    String notes;                   // Optional notes (e.g., "Replaced filter media", "Cleaned impeller")
};

class WaterChangeAssistant {
private:
    Preferences* prefs;
    ConfigManager* configMgr;  // Pointer to config for tank dimensions
    
    // Schedule settings
    WaterChangeSchedule schedule;
    unsigned long lastChangeTime;    // Seconds since boot
    float scheduledVolumePercent;    // Percentage of tank volume
    
    // Current water change state
    WaterChangePhase currentPhase;
    unsigned long waterChangeStartTimestamp; // Unix timestamp when water change started
    float currentChangeVolume;
    float tempBeforeChange;         // Captured at start
    float phBeforeChange;           // Captured at start
    float tdsBeforeChange;          // Captured at start
    bool systemsPaused;
    
    // History tracking
    std::vector<WaterChangeRecord> history;
    std::vector<FilterMaintenanceRecord> filterMaintenanceHistory;
    static const int MAX_HISTORY = 50;
    static const int MAX_FILTER_HISTORY = 100;
    
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
    void loadFilterMaintenanceHistory();
    void saveFilterMaintenanceHistory();
    void addFilterMaintenance(const FilterMaintenanceRecord& record);
    
    void markSettingsDirty();
    void markHistoryDirty();

public:
    WaterChangeAssistant();
    ~WaterChangeAssistant();
    
    void begin();
    void begin(ConfigManager* config); // Preferred: pass config manager
    void update();
    
    // Schedule management
    void setSchedule(WaterChangeSchedule sched, float volumePercent);
    WaterChangeSchedule getSchedule() { return schedule; }
    float getScheduledVolumePercent() { return scheduledVolumePercent; }
    int getDaysUntilNextChange();
    int getDaysSinceLastChange();
    bool isChangeOverdue();
    
    // Tank configuration
    void setConfigManager(ConfigManager* config); // Set config manager reference
    float getTankVolume(); // Returns calculated volume from ConfigManager
    float getScheduledChangeVolume(); // Returns litres
    
    // Water change operations (simplified)
    bool startWaterChange(float volumeLitres, float temp, float ph, float tds); // Start water change with current readings
    bool endWaterChange(float temp, float ph, float tds); // End water change with final readings and resume systems
    bool cancelWaterChange(); // Cancel without logging
    
    // Filter maintenance
    bool recordFilterMaintenance(const char* notes = "");
    unsigned long getDaysSinceLastFilterMaintenance();
    int getFilterMaintenanceCount() { return filterMaintenanceHistory.size(); }
    std::vector<FilterMaintenanceRecord> getRecentFilterMaintenance(int count);
    
    // Deferred save management
    void forceSave(); // Immediate save for critical operations
    
    // Phase control
    WaterChangePhase getCurrentPhase() { return currentPhase; }
    const char* getPhaseDescription();
    unsigned long getElapsedTime(); // Returns seconds since start
    bool isInProgress() { return currentPhase == PHASE_IN_PROGRESS; }
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
    unsigned long getStartTimestamp() { return waterChangeStartTimestamp; }
    float getTempBeforeChange() { return tempBeforeChange; }
    float getPhBeforeChange() { return phBeforeChange; }
    float getTdsBeforeChange() { return tdsBeforeChange; }
};

#endif
