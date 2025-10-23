#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct TaskInfo {
    String name;
    UBaseType_t stackHighWaterMark;  // Bytes remaining
    UBaseType_t stackSize;           // Total bytes
    float stackUsagePercent;
    eTaskState state;
};

struct HeapInfo {
    size_t freeHeap;
    size_t totalHeap;
    size_t minFreeHeap;
    size_t largestFreeBlock;
    float usagePercent;
};

class SystemMonitor {
public:
    SystemMonitor();
    
    void begin();
    void update();
    
    // Task monitoring
    std::vector<TaskInfo> getTaskInfo();
    TaskInfo getTaskInfo(TaskHandle_t handle);
    void printTaskInfo();
    
    // Heap monitoring
    HeapInfo getHeapInfo();
    void printHeapInfo();
    bool detectMemoryLeak();
    
    // CPU monitoring
    float getCPUUsage(uint8_t core);  // Returns 0-100%
    void printCPUInfo();
    
    // Watchdog monitoring
    void feedWatchdog(const char* taskName);
    void checkWatchdogs();
    
    // Alert thresholds
    void setStackWarningThreshold(float percent) { stackWarningPercent = percent; }
    void setHeapWarningThreshold(float percent) { heapWarningPercent = percent; }
    
private:
    unsigned long lastUpdate;
    unsigned long updateInterval;
    
    // Thresholds
    float stackWarningPercent;  // Alert if stack usage > this
    float heapWarningPercent;   // Alert if heap usage > this
    
    // Memory leak detection
    size_t lastFreeHeap;
    unsigned long lastHeapCheck;
    int heapDecreaseCount;
    
    // Watchdog tracking
    struct WatchdogEntry {
        String taskName;
        unsigned long lastFeed;
        unsigned long timeout;
    };
    std::vector<WatchdogEntry> watchdogs;
    
    void checkStackUsage(const TaskInfo& info);
    void checkHeapUsage(const HeapInfo& info);
};

#endif // SYSTEM_MONITOR_H
