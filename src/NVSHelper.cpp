#include "NVSHelper.h"
#include <ArduinoJson.h>

NVSHelper::NVSStats NVSHelper::getStats() {
    NVSStats stats;
    nvs_stats_t nvs_stats;
    
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
    
    if (err == ESP_OK) {
        stats.usedEntries = nvs_stats.used_entries;
        stats.freeEntries = nvs_stats.free_entries;
        stats.totalEntries = nvs_stats.total_entries;
        stats.namespaceCount = nvs_stats.namespace_count;
        
        if (stats.totalEntries > 0) {
            stats.usagePercent = (float)stats.usedEntries / stats.totalEntries * 100.0f;
        } else {
            stats.usagePercent = 0.0f;
        }
        
        // Determine health status
        stats.status = getHealthStatus(stats.usagePercent);
        stats.healthy = (stats.usagePercent < 70.0f);
        
    } else {
        // Error reading stats
        stats.usedEntries = 0;
        stats.freeEntries = 0;
        stats.totalEntries = 0;
        stats.namespaceCount = 0;
        stats.usagePercent = 0.0f;
        stats.healthy = false;
        stats.status = "error";
    }
    
    return stats;
}

void NVSHelper::printStats() {
    NVSStats stats = getStats();
    
    if (stats.status == "error") {
        Serial.println("ERROR: Could not retrieve NVS statistics");
        return;
    }
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║       NVS Flash Statistics             ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Used Entries:     %-5u              ║\n", stats.usedEntries);
    Serial.printf("║ Free Entries:     %-5u              ║\n", stats.freeEntries);
    Serial.printf("║ Total Entries:    %-5u              ║\n", stats.totalEntries);
    Serial.printf("║ Namespace Count:  %-5u              ║\n", stats.namespaceCount);
    Serial.printf("║ Usage:            %5.1f%%             ║\n", stats.usagePercent);
    Serial.println("╠════════════════════════════════════════╣");
    
    if (stats.usagePercent >= 90.0f) {
        Serial.println("║ Status: ⚠️  CRITICAL - Very High      ║");
        Serial.println("║ Action: Erase old data immediately!   ║");
    } else if (stats.usagePercent >= 80.0f) {
        Serial.println("║ Status: ⚠️  HIGH - Approaching Limit  ║");
        Serial.println("║ Action: Consider clearing old data    ║");
    } else if (stats.usagePercent >= 70.0f) {
        Serial.println("║ Status: ⚠️  ELEVATED - Monitor Usage  ║");
        Serial.println("║ Action: Review stored data            ║");
    } else {
        Serial.println("║ Status: ✅ HEALTHY - Normal Operation ║");
        Serial.println("║ Action: None required                 ║");
    }
    
    Serial.println("╚════════════════════════════════════════╝\n");
}

String NVSHelper::getStatsJSON() {
    NVSStats stats = getStats();
    
    // Use static JSON document to avoid heap fragmentation
    StaticJsonDocument<512> doc;
    
    if (stats.status == "error") {
        doc["error"] = "Could not retrieve NVS statistics";
        doc["success"] = false;
    } else {
        doc["success"] = true;
        doc["used_entries"] = stats.usedEntries;
        doc["free_entries"] = stats.freeEntries;
        doc["total_entries"] = stats.totalEntries;
        doc["namespace_count"] = stats.namespaceCount;
        doc["usage_percent"] = stats.usagePercent;
        doc["healthy"] = stats.healthy;
        doc["status"] = stats.status;
        doc["recommendation"] = getRecommendation(stats.usagePercent);
        
        // Add visual indicator
        if (stats.usagePercent >= 90.0f) {
            doc["alert_level"] = "critical";
            doc["icon"] = "⚠️";
        } else if (stats.usagePercent >= 80.0f) {
            doc["alert_level"] = "high";
            doc["icon"] = "⚠️";
        } else if (stats.usagePercent >= 70.0f) {
            doc["alert_level"] = "warning";
            doc["icon"] = "⚠️";
        } else {
            doc["alert_level"] = "normal";
            doc["icon"] = "✅";
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool NVSHelper::isHealthy() {
    NVSStats stats = getStats();
    return stats.healthy;
}

float NVSHelper::getUsagePercent() {
    NVSStats stats = getStats();
    return stats.usagePercent;
}

bool NVSHelper::eraseAll() {
    Serial.println("\n⚠️  ============================================");
    Serial.println("⚠️  WARNING: ERASING ALL NVS DATA!");
    Serial.println("⚠️  All configuration will be lost!");
    Serial.println("⚠️  ============================================\n");
    
    // Erase NVS
    esp_err_t err = nvs_flash_erase();
    
    if (err != ESP_OK) {
        Serial.printf("❌ ERROR: NVS erase failed: %s\n", esp_err_to_name(err));
        return false;
    }
    
    Serial.println("✓ NVS partition erased successfully");
    
    // Re-initialize NVS
    err = nvs_flash_init();
    
    if (err != ESP_OK) {
        Serial.printf("❌ ERROR: NVS re-initialization failed: %s\n", esp_err_to_name(err));
        return false;
    }
    
    Serial.println("✓ NVS re-initialized successfully");
    Serial.println("✓ All NVS data cleared - device will restart\n");
    
    return true;
}

bool NVSHelper::eraseAllWithConfirmation(const String& confirmation) {
    if (confirmation != "ERASE_ALL_NVS") {
        Serial.println("❌ ERROR: Invalid confirmation string");
        Serial.println("   Required: 'ERASE_ALL_NVS'");
        Serial.printf("   Received: '%s'\n", confirmation.c_str());
        return false;
    }
    
    return eraseAll();
}

String NVSHelper::getHealthStatus(float usagePercent) {
    if (usagePercent >= 90.0f) {
        return "critical";
    } else if (usagePercent >= 80.0f) {
        return "high";
    } else if (usagePercent >= 70.0f) {
        return "elevated";
    } else {
        return "healthy";
    }
}

String NVSHelper::getRecommendation(float usagePercent) {
    if (usagePercent >= 90.0f) {
        return "URGENT: NVS nearly full! Clear old data or erase NVS immediately.";
    } else if (usagePercent >= 80.0f) {
        return "WARNING: NVS usage high. Consider clearing old configuration data.";
    } else if (usagePercent >= 70.0f) {
        return "NOTICE: NVS usage elevated. Monitor usage and review stored data.";
    } else if (usagePercent >= 50.0f) {
        return "Normal operation. NVS usage is healthy.";
    } else {
        return "Excellent. Plenty of NVS storage available.";
    }
}
