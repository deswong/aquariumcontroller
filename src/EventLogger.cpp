#include "EventLogger.h"
#include <time.h>

EventLogger::EventLogger() : currentLogCount(0), spiffsAvailable(false) {
}

EventLogger::~EventLogger() {
}

bool EventLogger::begin() {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {  // true = format if mount fails
        Serial.println("[EventLogger] SPIFFS mount failed! Falling back to Serial only.");
        spiffsAvailable = false;
        return false;
    }
    
    spiffsAvailable = true;
    
    // Create logs directory if it doesn't exist
    if (!SPIFFS.exists("/logs")) {
        SPIFFS.mkdir("/logs");
        Serial.println("[EventLogger] Created /logs directory");
    }
    
    // Count existing log entries
    currentLogCount = countLogEntries();
    
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    
    Serial.printf("[EventLogger] SPIFFS initialized - %d entries, %u/%u bytes used\n", 
                  currentLogCount, usedBytes, totalBytes);
    
    // Log the startup
    info("System", "Event logger started with SPIFFS rolling log");
    
    return true;
}

String EventLogger::levelToString(EventLevel level) {
    switch (level) {
        case EVENT_INFO:     return "INFO";
        case EVENT_WARNING:  return "WARNING";
        case EVENT_ERROR:    return "ERROR";
        case EVENT_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN";
    }
}

EventLevel EventLogger::stringToLevel(const String& str) {
    if (str == "INFO") return EVENT_INFO;
    if (str == "WARNING") return EVENT_WARNING;
    if (str == "ERROR") return EVENT_ERROR;
    if (str == "CRITICAL") return EVENT_CRITICAL;
    return EVENT_INFO;
}

void EventLogger::log(EventLevel level, const String& category, const String& message) {
    // Create log event
    LogEvent event;
    event.timestamp = time(nullptr);  // Use NTP time
    event.level = level;
    event.category = category;
    event.message = message;
    
    String logLine = formatEvent(event);
    
    // Always output to serial
    Serial.println("[LOG] " + logLine);
    
    // Write to SPIFFS if available
    if (spiffsAvailable) {
        writeLogEntry(event);
        
        // Check if rotation needed
        currentLogCount++;
        if (currentLogCount >= EVENT_LOG_MAX_ENTRIES) {
            Serial.printf("[EventLogger] Rotating log file (reached %d entries)\n", EVENT_LOG_MAX_ENTRIES);
            rotateLogFile();
            currentLogCount = countLogEntries();
        }
    }
}

void EventLogger::info(const String& category, const String& message) {
    log(EVENT_INFO, category, message);
}

void EventLogger::warning(const String& category, const String& message) {
    log(EVENT_WARNING, category, message);
}

void EventLogger::error(const String& category, const String& message) {
    log(EVENT_ERROR, category, message);
}

void EventLogger::critical(const String& category, const String& message) {
    log(EVENT_CRITICAL, category, message);
}

std::vector<LogEvent> EventLogger::getRecentLogs(int count) {
    if (!spiffsAvailable) {
        return std::vector<LogEvent>();
    }
    
    std::vector<LogEvent> logs = readLogFile(LOG_FILE_PATH, count);
    
    // If we need more and there's an archive, add from archive
    if (logs.size() < (size_t)count && SPIFFS.exists(LOG_ARCHIVE_PATH)) {
        int remaining = count - logs.size();
        std::vector<LogEvent> archiveLogs = readLogFile(LOG_ARCHIVE_PATH, remaining);
        logs.insert(logs.end(), archiveLogs.begin(), archiveLogs.end());
    }
    
    return logs;
}

std::vector<LogEvent> EventLogger::getLogsByLevel(EventLevel level, int count) {
    std::vector<LogEvent> allLogs = getRecentLogs(count * 2); // Get more to filter
    std::vector<LogEvent> filtered;
    
    for (const auto& log : allLogs) {
        if (log.level == level) {
            filtered.push_back(log);
            if ((int)filtered.size() >= count) break;
        }
    }
    
    return filtered;
}

std::vector<LogEvent> EventLogger::getLogsByCategory(const String& category, int count) {
    std::vector<LogEvent> allLogs = getRecentLogs(count * 2); // Get more to filter
    std::vector<LogEvent> filtered;
    
    for (const auto& log : allLogs) {
        if (log.category == category) {
            filtered.push_back(log);
            if ((int)filtered.size() >= count) break;
        }
    }
    
    return filtered;
}

void EventLogger::clearLogs() {
    if (!spiffsAvailable) {
        Serial.println("[EventLogger] SPIFFS not available");
        return;
    }
    
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        SPIFFS.remove(LOG_FILE_PATH);
    }
    if (SPIFFS.exists(LOG_ARCHIVE_PATH)) {
        SPIFFS.remove(LOG_ARCHIVE_PATH);
    }
    
    currentLogCount = 0;
    Serial.println("[EventLogger] All logs cleared");
}

int EventLogger::getLogCount() {
    return currentLogCount;
}

String EventLogger::formatEvent(const LogEvent& event) {
    // Format: timestamp|level|category|message
    // Convert Unix timestamp to readable format
    time_t rawtime = event.timestamp;
    struct tm* timeinfo = localtime(&rawtime);
    
    char timeStr[20];
    if (timeinfo && event.timestamp > 1000000000) {  // Valid NTP time
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    } else {
        // NTP not synced yet, show boot time
        unsigned long seconds = millis() / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        snprintf(timeStr, sizeof(timeStr), "Boot+%luh%lum", hours, minutes % 60);
    }
    
    return String(event.timestamp) + "|" + 
           levelToString(event.level) + "|" + 
           event.category + "|" + 
           event.message + " [" + String(timeStr) + "]";
}

// ============================================================================
// SPIFFS Log Management
// ============================================================================

void EventLogger::rotateLogFile() {
    if (!spiffsAvailable) return;
    
    // Delete old archive
    if (SPIFFS.exists(LOG_ARCHIVE_PATH)) {
        SPIFFS.remove(LOG_ARCHIVE_PATH);
    }
    
    // Rename current log to archive
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        SPIFFS.rename(LOG_FILE_PATH, LOG_ARCHIVE_PATH);
        Serial.println("[EventLogger] Log file rotated to archive");
    }
}

int EventLogger::countLogEntries() {
    if (!spiffsAvailable || !SPIFFS.exists(LOG_FILE_PATH)) {
        return 0;
    }
    
    File file = SPIFFS.open(LOG_FILE_PATH, "r");
    if (!file) {
        return 0;
    }
    
    int count = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 0) {
            count++;
        }
    }
    file.close();
    
    return count;
}

void EventLogger::writeLogEntry(const LogEvent& event) {
    if (!spiffsAvailable) return;
    
    File file = SPIFFS.open(LOG_FILE_PATH, "a");  // Append mode
    if (!file) {
        Serial.println("[EventLogger] Failed to open log file for writing");
        return;
    }
    
    // Write in CSV format: timestamp,level,category,message
    file.print(event.timestamp);
    file.print(",");
    file.print(levelToString(event.level));
    file.print(",");
    file.print(event.category);
    file.print(",");
    file.println(event.message);
    
    file.close();
}

std::vector<LogEvent> EventLogger::readLogFile(const String& path, int maxCount) {
    std::vector<LogEvent> logs;
    
    if (!spiffsAvailable || !SPIFFS.exists(path)) {
        return logs;
    }
    
    File file = SPIFFS.open(path, "r");
    if (!file) {
        return logs;
    }
    
    // Read all lines into vector
    std::vector<String> lines;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 0) {
            lines.push_back(line);
        }
    }
    file.close();
    
    // Get the last N entries (most recent)
    int startIdx = 0;
    if (maxCount > 0 && (int)lines.size() > maxCount) {
        startIdx = lines.size() - maxCount;
    }
    
    // Parse lines into LogEvent structures (in reverse order - newest first)
    for (int i = lines.size() - 1; i >= startIdx; i--) {
        LogEvent event = parseLogLine(lines[i]);
        if (event.timestamp > 0) {  // Valid entry
            logs.push_back(event);
        }
    }
    
    return logs;
}

LogEvent EventLogger::parseLogLine(const String& line) {
    LogEvent event;
    event.timestamp = 0;
    
    // Parse CSV format: timestamp,level,category,message
    int firstComma = line.indexOf(',');
    if (firstComma == -1) return event;
    
    int secondComma = line.indexOf(',', firstComma + 1);
    if (secondComma == -1) return event;
    
    int thirdComma = line.indexOf(',', secondComma + 1);
    if (thirdComma == -1) return event;
    
    // Extract fields
    event.timestamp = line.substring(0, firstComma).toInt();
    String levelStr = line.substring(firstComma + 1, secondComma);
    event.level = stringToLevel(levelStr);
    event.category = line.substring(secondComma + 1, thirdComma);
    event.message = line.substring(thirdComma + 1);
    
    return event;
}

size_t EventLogger::getLogFileSize() {
    if (!spiffsAvailable) return 0;
    
    size_t totalSize = 0;
    
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        File file = SPIFFS.open(LOG_FILE_PATH, "r");
        if (file) {
            totalSize += file.size();
            file.close();
        }
    }
    
    if (SPIFFS.exists(LOG_ARCHIVE_PATH)) {
        File file = SPIFFS.open(LOG_ARCHIVE_PATH, "r");
        if (file) {
            totalSize += file.size();
            file.close();
        }
    }
    
    return totalSize;
}

void EventLogger::exportLogsToSerial() {
    if (!spiffsAvailable) {
        Serial.println("[EventLogger] SPIFFS not available");
        return;
    }
    
    Serial.println("\n========== LOG EXPORT ==========");
    
    // Export archive first (older)
    if (SPIFFS.exists(LOG_ARCHIVE_PATH)) {
        Serial.println("[Archive - Older Logs]");
        File file = SPIFFS.open(LOG_ARCHIVE_PATH, "r");
        if (file) {
            while (file.available()) {
                Serial.println(file.readStringUntil('\n'));
            }
            file.close();
        }
    }
    
    // Export current log (newer)
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        Serial.println("[Current - Recent Logs]");
        File file = SPIFFS.open(LOG_FILE_PATH, "r");
        if (file) {
            while (file.available()) {
                Serial.println(file.readStringUntil('\n'));
            }
            file.close();
        }
    }
    
    Serial.println("========== END EXPORT ==========\n");
}
