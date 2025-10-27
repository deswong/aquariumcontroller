#ifndef NVS_HELPER_H
#define NVS_HELPER_H

#include <Arduino.h>
#include <nvs.h>
#include <nvs_flash.h>

/**
 * NVS Helper - Utilities for monitoring and managing NVS flash storage
 * 
 * Provides:
 * - NVS usage statistics and health monitoring
 * - NVS erase and recovery functions
 * - JSON output for web interface integration
 */
class NVSHelper {
public:
    /**
     * NVS Statistics Structure
     */
    struct NVSStats {
        uint32_t usedEntries;
        uint32_t freeEntries;
        uint32_t totalEntries;
        uint32_t namespaceCount;
        float usagePercent;
        bool healthy;
        String status;  // "healthy", "elevated", "high", "critical"
    };
    
    /**
     * Get NVS statistics
     * @return NVSStats structure with current usage information
     */
    static NVSStats getStats();
    
    /**
     * Print NVS statistics to serial monitor
     */
    static void printStats();
    
    /**
     * Get NVS statistics as JSON string for web interface
     * @return JSON string with statistics
     */
    static String getStatsJSON();
    
    /**
     * Check if NVS usage is healthy
     * @return true if usage is below warning threshold (70%)
     */
    static bool isHealthy();
    
    /**
     * Get NVS usage percentage
     * @return Usage percentage (0-100)
     */
    static float getUsagePercent();
    
    /**
     * Erase all NVS data and re-initialize
     * WARNING: This will delete all stored configuration!
     * @return true if successful
     */
    static bool eraseAll();
    
    /**
     * Erase and reinitialize with confirmation
     * Requires "ERASE_ALL_NVS" as confirmation string for safety
     * @param confirmation Must be exactly "ERASE_ALL_NVS"
     * @return true if successful
     */
    static bool eraseAllWithConfirmation(const String& confirmation);
    
    /**
     * Get health status string
     * @param usagePercent Current usage percentage
     * @return "healthy", "elevated", "high", or "critical"
     */
    static String getHealthStatus(float usagePercent);
    
    /**
     * Get recommended action based on usage
     * @param usagePercent Current usage percentage
     * @return Recommendation string
     */
    static String getRecommendation(float usagePercent);
};

#endif // NVS_HELPER_H
