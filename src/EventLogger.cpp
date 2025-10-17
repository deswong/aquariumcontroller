#include "EventLogger.h"

const char* EventLogger::LOG_FILE = "/events.log";

EventLogger::EventLogger() {
}

EventLogger::~EventLogger() {
}

bool EventLogger::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("ERROR: Failed to mount SPIFFS");
        return false;
    }
    
    Serial.println("Event Logger initialized");
    
    // Log the startup
    info("System", "Event logger started");
    
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

void EventLogger::rotateLogIfNeeded() {
    File file = SPIFFS.open(LOG_FILE, "r");
    if (!file) return;
    
    int lineCount = 0;
    while (file.available()) {
        file.readStringUntil('\n');
        lineCount++;
    }
    file.close();
    
    if (lineCount >= MAX_LOG_SIZE) {
        Serial.printf("Rotating log file (%d lines)\n", lineCount);
        
        // Read all logs
        std::vector<LogEvent> logs = getRecentLogs(MAX_LOG_SIZE);
        
        // Keep only the most recent 50%
        int keepCount = MAX_LOG_SIZE / 2;
        
        // Rewrite file with recent logs
        file = SPIFFS.open(LOG_FILE, "w");
        if (file) {
            for (int i = logs.size() - keepCount; i < logs.size(); i++) {
                if (i >= 0) {
                    file.println(formatEvent(logs[i]));
                }
            }
            file.close();
        }
    }
}

void EventLogger::log(EventLevel level, const String& category, const String& message) {
    rotateLogIfNeeded();
    
    File file = SPIFFS.open(LOG_FILE, "a");
    if (!file) {
        Serial.println("ERROR: Failed to open log file");
        return;
    }
    
    LogEvent event;
    event.timestamp = millis();
    event.level = level;
    event.category = category;
    event.message = message;
    
    String logLine = formatEvent(event);
    file.println(logLine);
    file.close();
    
    // Also print to serial for debugging
    Serial.println("[LOG] " + logLine);
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
    std::vector<LogEvent> logs;
    
    File file = SPIFFS.open(LOG_FILE, "r");
    if (!file) {
        return logs;
    }
    
    // Read all lines first
    std::vector<String> lines;
    while (file.available()) {
        lines.push_back(file.readStringUntil('\n'));
    }
    file.close();
    
    // Parse the last 'count' lines
    int startIndex = max(0, (int)lines.size() - count);
    for (int i = startIndex; i < lines.size(); i++) {
        String line = lines[i];
        line.trim();
        
        if (line.length() == 0) continue;
        
        // Parse format: timestamp|level|category|message
        int pos1 = line.indexOf('|');
        int pos2 = line.indexOf('|', pos1 + 1);
        int pos3 = line.indexOf('|', pos2 + 1);
        
        if (pos1 > 0 && pos2 > 0 && pos3 > 0) {
            LogEvent event;
            event.timestamp = line.substring(0, pos1).toInt();
            event.level = stringToLevel(line.substring(pos1 + 1, pos2));
            event.category = line.substring(pos2 + 1, pos3);
            event.message = line.substring(pos3 + 1);
            logs.push_back(event);
        }
    }
    
    return logs;
}

std::vector<LogEvent> EventLogger::getLogsByLevel(EventLevel level, int count) {
    std::vector<LogEvent> allLogs = getRecentLogs(1000);
    std::vector<LogEvent> filtered;
    
    for (const auto& log : allLogs) {
        if (log.level == level) {
            filtered.push_back(log);
            if (filtered.size() >= count) break;
        }
    }
    
    return filtered;
}

std::vector<LogEvent> EventLogger::getLogsByCategory(const String& category, int count) {
    std::vector<LogEvent> allLogs = getRecentLogs(1000);
    std::vector<LogEvent> filtered;
    
    for (const auto& log : allLogs) {
        if (log.category == category) {
            filtered.push_back(log);
            if (filtered.size() >= count) break;
        }
    }
    
    return filtered;
}

void EventLogger::clearLogs() {
    SPIFFS.remove(LOG_FILE);
    Serial.println("Event logs cleared");
    info("System", "Logs cleared");
}

int EventLogger::getLogCount() {
    File file = SPIFFS.open(LOG_FILE, "r");
    if (!file) return 0;
    
    int count = 0;
    while (file.available()) {
        file.readStringUntil('\n');
        count++;
    }
    file.close();
    
    return count;
}

String EventLogger::formatEvent(const LogEvent& event) {
    // Format: timestamp|level|category|message
    unsigned long seconds = event.timestamp / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    String timeStr;
    if (days > 0) {
        timeStr = String(days) + "d " + String(hours % 24) + "h";
    } else if (hours > 0) {
        timeStr = String(hours) + "h " + String(minutes % 60) + "m";
    } else if (minutes > 0) {
        timeStr = String(minutes) + "m " + String(seconds % 60) + "s";
    } else {
        timeStr = String(seconds) + "s";
    }
    
    return String(event.timestamp) + "|" + 
           levelToString(event.level) + "|" + 
           event.category + "|" + 
           event.message + " [" + timeStr + "]";
}
