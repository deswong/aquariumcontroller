#include "EventLogger.h"
#include <time.h>

EventLogger::EventLogger() {
}

EventLogger::~EventLogger() {
}

bool EventLogger::begin() {
    Serial.println("Event Logger initialized (Serial output only)");
    
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

void EventLogger::log(EventLevel level, const String& category, const String& message) {
    // Create formatted log message
    LogEvent event;
    event.timestamp = time(nullptr);  // Use NTP time instead of millis
    event.level = level;
    event.category = category;
    event.message = message;
    
    String logLine = formatEvent(event);
    
    // Output to serial only
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
    // Return empty vector - logs not stored anymore
    return std::vector<LogEvent>();
}

std::vector<LogEvent> EventLogger::getLogsByLevel(EventLevel level, int count) {
    // Return empty vector - logs not stored anymore
    return std::vector<LogEvent>();
}

std::vector<LogEvent> EventLogger::getLogsByCategory(const String& category, int count) {
    // Return empty vector - logs not stored anymore
    return std::vector<LogEvent>();
}

void EventLogger::clearLogs() {
    Serial.println("Event logs cleared (Serial output only)");
}

int EventLogger::getLogCount() {
    return 0; // No logs stored
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
