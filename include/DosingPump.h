#ifndef DOSING_PUMP_H
#define DOSING_PUMP_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// DRV8871 DC Motor Driver Control
// IN1 HIGH, IN2 LOW = Forward (pumping)
// IN1 LOW, IN2 HIGH = Reverse (backflush)
// Both LOW = Coast (stop)
// Both HIGH = Brake (fast stop)

enum DosingSchedule {
    DOSE_MANUAL = 0,      // Manual dosing only
    DOSE_DAILY = 1,       // Once per day
    DOSE_WEEKLY = 7,      // Once per week
    DOSE_CUSTOM = 99      // Custom interval in days
};

enum PumpState {
    PUMP_IDLE,
    PUMP_DOSING,
    PUMP_PRIMING,
    PUMP_BACKFLUSHING,
    PUMP_CALIBRATING,
    PUMP_PAUSED,
    PUMP_ERROR
};

struct DosingRecord {
    unsigned long timestamp;
    float volumeDosed;      // mL
    int durationMs;         // Actual duration
    bool success;
    String type;            // "scheduled", "manual", "calibration"
};

struct DosingScheduleConfig {
    bool enabled;
    DosingSchedule schedule;
    int customDays;         // For DOSE_CUSTOM
    int hour;               // Hour to dose (0-23)
    int minute;             // Minute to dose (0-59)
    float doseVolume;       // mL per dose
    unsigned long lastDoseTime;
    unsigned long nextDoseTime;
};

class DosingPump {
private:
    // Hardware pins
    uint8_t in1Pin;         // DRV8871 IN1 (forward)
    uint8_t in2Pin;         // DRV8871 IN2 (reverse)
    uint8_t pwmChannel;     // ESP32 PWM channel
    
    // Calibration data
    float mlPerSecond;      // Flow rate at 100% speed
    bool calibrated;
    unsigned long lastCalibrationTime;
    
    // Current state
    PumpState currentState;
    unsigned long stateStartTime;
    int currentSpeed;       // 0-100%
    float targetVolume;     // mL
    float volumePumped;     // mL
    
    // Schedule
    DosingScheduleConfig scheduleConfig;
    
    // History
    std::vector<DosingRecord> history;
    int maxHistoryRecords;
    
    // Safety
    bool safetyEnabled;
    int maxDoseVolume;      // mL per dose
    int maxDailyVolume;     // mL per day
    float dailyVolumeDosed;
    unsigned long lastDayReset;
    
    // Maintenance
    unsigned long totalRuntime;      // milliseconds
    unsigned long lastMaintenanceTime;
    int totalDoses;
    
    // Storage
    Preferences* prefs;
    
    // Deferred saving (dirty flag pattern)
    bool configDirty;
    bool historyDirty;
    unsigned long lastSaveTime;
    static const unsigned long SAVE_DELAY_MS = 5000; // 5 seconds
    
    // Internal methods
    void setMotorForward(int speed);
    void setMotorReverse(int speed);
    void setMotorBrake();
    void setMotorCoast();
    void updateVolumeTracking();
    void addToHistory(DosingRecord record);
    void checkSafetyLimits();
    void resetDailyVolume();
    int calculateScheduleInterval();
    void calculateNextDoseTime();
    
    void markConfigDirty();
    void markHistoryDirty();
    
public:
    DosingPump(uint8_t in1, uint8_t in2, uint8_t pwmCh = 0);
    ~DosingPump();
    
    // Initialization
    void begin();
    void loadConfig();
    void saveConfig();
    void loadCalibration();
    void saveCalibration();
    void loadHistory();
    void saveHistory();
    
    // Basic pump control
    void start(float volumeML, int speedPercent = 100);
    void stop();
    void pause();
    void resume();
    void emergencyStop();
    
    // Deferred save management
    void forceSave(); // Immediate save for critical operations
    
    // Maintenance functions
    void prime(int durationSeconds, int speedPercent = 50);
    void backflush(int durationSeconds, int speedPercent = 30);
    void purge(int durationSeconds, int speedPercent = 100);
    void runCleaning(int cycles = 3);
    
    // Calibration
    void startCalibration(int speedPercent = 100);
    void finishCalibration(float measuredVolumeML, int durationSeconds);
    void cancelCalibration();
    bool isCalibrated();
    int getDaysSinceCalibration();
    
    // Dosing control
    void doseManual(float volumeML, int speedPercent = 100);
    void doseScheduled();
    bool isDosing();
    float getProgress(); // 0.0 to 1.0
    
    // Schedule management
    void setSchedule(DosingSchedule schedule, int hour, int minute, float volumeML);
    void setCustomSchedule(int days, int hour, int minute, float volumeML);
    void enableSchedule(bool enable);
    bool isScheduleEnabled();
    DosingScheduleConfig getScheduleConfig();
    int getHoursUntilNextDose();
    bool isDoseOverdue();
    
    // Safety
    void setSafetyLimits(int maxDoseML, int maxDailyML);
    void setSafetyEnabled(bool enable);
    bool isSafetyEnabled();
    float getDailyVolumeDosed();
    float getRemainingDailyVolume();
    
    // State & status
    PumpState getState();
    String getStateString();
    float getFlowRate();
    int getCurrentSpeed();
    float getTargetVolume();
    float getVolumePumped();
    int getElapsedTime(); // milliseconds
    
    // History
    std::vector<DosingRecord> getHistory(int count = 50);
    int getHistoryCount();
    void clearHistory();
    
    // Statistics
    unsigned long getTotalRuntime();
    int getTotalDoses();
    float getAverageDoseVolume();
    float getTotalVolumeDosed();
    unsigned long getLastMaintenanceTime();
    void resetMaintenanceCounter();
    
    // Update loop
    void update();
    
    // JSON status
    String getStatusJSON();
};

#endif // DOSING_PUMP_H
