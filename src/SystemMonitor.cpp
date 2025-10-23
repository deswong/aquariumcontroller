#include "SystemMonitor.h"
#include "Logger.h"
#include <esp_system.h>
#include <esp_heap_caps.h>

SystemMonitor::SystemMonitor()
    : lastUpdate(0),
      updateInterval(60000),  // Update every minute
      stackWarningPercent(80.0),
      heapWarningPercent(80.0),
      lastFreeHeap(0),
      lastHeapCheck(0),
      heapDecreaseCount(0) {
}

void SystemMonitor::begin() {
    lastFreeHeap = ESP.getFreeHeap();
    lastHeapCheck = millis();
    LOG_INFO("SysMon", "System monitor initialized");
    
    // Print initial status
    printHeapInfo();
    printTaskInfo();
}

void SystemMonitor::update() {
    unsigned long now = millis();
    
    if (now - lastUpdate > updateInterval) {
        lastUpdate = now;
        
        // Check heap for memory leaks
        if (detectMemoryLeak()) {
            LOG_WARN("SysMon", "Potential memory leak detected!");
        }
        
        // Check all task stacks
        auto tasks = getTaskInfo();
        for (const auto& task : tasks) {
            checkStackUsage(task);
        }
        
        // Check heap usage
        HeapInfo heap = getHeapInfo();
        checkHeapUsage(heap);
        
        // Check watchdogs
        checkWatchdogs();
    }
}

std::vector<TaskInfo> SystemMonitor::getTaskInfo() {
    std::vector<TaskInfo> tasks;
    
    // Note: uxTaskGetSystemState requires configUSE_TRACE_FACILITY
    // For now, return empty list or use alternative approach
    // Individual task stats can still be queried via getTaskInfo(handle)
    
    LOG_DEBUG("SysMon", "Task enumeration not available (requires configUSE_TRACE_FACILITY)");
    
    return tasks;
}

TaskInfo SystemMonitor::getTaskInfo(TaskHandle_t handle) {
    TaskInfo info;
    
    if (handle != nullptr) {
        info.name = pcTaskGetName(handle);
        info.stackHighWaterMark = uxTaskGetStackHighWaterMark(handle);
        
        // Note: Can't get total stack size from handle alone easily
        info.stackSize = 0;
        info.stackUsagePercent = 0.0;
        info.state = eTaskGetState(handle);
    }
    
    return info;
}

void SystemMonitor::printTaskInfo() {
    auto tasks = getTaskInfo();
    
    LOG_INFO("SysMon", "=== Task Information ===");
    LOG_INFO("SysMon", "%-20s %8s %8s %6s", "Task Name", "Stack", "Free", "Usage");
    
    for (const auto& task : tasks) {
        const char* stateStr[] = {"RUN", "RDY", "BLK", "SUS", "DEL"};
        LOG_INFO("SysMon", "%-20s %8d %8d %5.1f%% [%s]",
                task.name.c_str(),
                task.stackSize,
                task.stackHighWaterMark,
                task.stackUsagePercent,
                stateStr[task.state]);
    }
}

HeapInfo SystemMonitor::getHeapInfo() {
    HeapInfo info;
    
    info.freeHeap = ESP.getFreeHeap();
    info.totalHeap = ESP.getHeapSize();
    info.minFreeHeap = ESP.getMinFreeHeap();
    info.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    if (info.totalHeap > 0) {
        info.usagePercent = ((float)(info.totalHeap - info.freeHeap) / info.totalHeap) * 100.0;
    } else {
        info.usagePercent = 0.0;
    }
    
    return info;
}

void SystemMonitor::printHeapInfo() {
    HeapInfo heap = getHeapInfo();
    
    LOG_INFO("SysMon", "=== Heap Information ===");
    LOG_INFO("SysMon", "Total Heap:          %zu bytes", heap.totalHeap);
    LOG_INFO("SysMon", "Free Heap:           %zu bytes", heap.freeHeap);
    LOG_INFO("SysMon", "Min Free Heap:       %zu bytes", heap.minFreeHeap);
    LOG_INFO("SysMon", "Largest Free Block:  %zu bytes", heap.largestFreeBlock);
    LOG_INFO("SysMon", "Heap Usage:          %.1f%%", heap.usagePercent);
}

bool SystemMonitor::detectMemoryLeak() {
    unsigned long now = millis();
    
    // Check every 5 minutes
    if (now - lastHeapCheck < 300000) {
        return false;
    }
    
    lastHeapCheck = now;
    size_t currentHeap = ESP.getFreeHeap();
    
    // Check if heap is consistently decreasing
    if (currentHeap < lastFreeHeap - 1024) {  // More than 1KB decrease
        heapDecreaseCount++;
        LOG_WARN("SysMon", "Heap decreased by %d bytes (count: %d)", 
                lastFreeHeap - currentHeap, heapDecreaseCount);
        
        if (heapDecreaseCount >= 3) {
            // Heap has been decreasing for 3 consecutive checks (15 minutes)
            heapDecreaseCount = 0;  // Reset counter
            return true;
        }
    } else {
        heapDecreaseCount = 0;  // Reset if heap stable or increased
    }
    
    lastFreeHeap = currentHeap;
    return false;
}

float SystemMonitor::getCPUUsage(uint8_t core) {
    // This is a simplified version
    // For more accurate CPU usage, would need to integrate with FreeRTOS stats
    return 0.0;  // Placeholder
}

void SystemMonitor::printCPUInfo() {
    LOG_INFO("SysMon", "=== CPU Information ===");
    LOG_INFO("SysMon", "CPU Frequency: %d MHz", getCpuFrequencyMhz());
    LOG_INFO("SysMon", "Core Count: %d", ESP.getChipCores());
}

void SystemMonitor::feedWatchdog(const char* taskName) {
    unsigned long now = millis();
    
    // Find existing watchdog entry
    for (auto& wd : watchdogs) {
        if (wd.taskName == taskName) {
            wd.lastFeed = now;
            return;
        }
    }
    
    // Add new watchdog entry
    WatchdogEntry wd;
    wd.taskName = taskName;
    wd.lastFeed = now;
    wd.timeout = 60000;  // 1 minute default timeout
    watchdogs.push_back(wd);
}

void SystemMonitor::checkWatchdogs() {
    unsigned long now = millis();
    
    for (const auto& wd : watchdogs) {
        if (now - wd.lastFeed > wd.timeout) {
            LOG_ERROR("SysMon", "Watchdog timeout for task: %s", wd.taskName.c_str());
        }
    }
}

void SystemMonitor::checkStackUsage(const TaskInfo& info) {
    if (info.stackUsagePercent > stackWarningPercent) {
        LOG_WARN("SysMon", "High stack usage in task '%s': %.1f%% (free: %d bytes)",
                info.name.c_str(), info.stackUsagePercent, info.stackHighWaterMark);
    }
}

void SystemMonitor::checkHeapUsage(const HeapInfo& info) {
    if (info.usagePercent > heapWarningPercent) {
        LOG_WARN("SysMon", "High heap usage: %.1f%% (free: %zu bytes)",
                info.usagePercent, info.freeHeap);
    }
    
    // Warn if largest free block is fragmented
    if (info.largestFreeBlock < info.freeHeap / 2) {
        LOG_WARN("SysMon", "Heap fragmentation detected (largest block: %zu of %zu free)",
                info.largestFreeBlock, info.freeHeap);
    }
}
