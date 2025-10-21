#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <Arduino.h>
#include <vector>
#include <FS.h>
#include <SPIFFS.h>

// Maximum log entries before rotation (configurable via build flags)
#ifndef EVENT_LOG_MAX_ENTRIES
#define EVENT_LOG_MAX_ENTRIES 1000  // Default for 8MB flash
#endif

#define LOG_FILE_PATH "/logs/events.log"
#define LOG_ARCHIVE_PATH "/logs/events_old.log"

enum EventLevel {
    EVENT_INFO,
    EVENT_WARNING,
    EVENT_ERROR,
    EVENT_CRITICAL
};

struct LogEvent {
    unsigned long timestamp;  // Unix timestamp (seconds since 1970-01-01)
    EventLevel level;
    String category;
    String message;
};

class EventLogger {
private:
    int currentLogCount;
    bool spiffsAvailable;
    
    String levelToString(EventLevel level);
    EventLevel stringToLevel(const String& str);
    
    // SPIFFS management
    void rotateLogFile();
    int countLogEntries();
    void writeLogEntry(const LogEvent& event);
    std::vector<LogEvent> readLogFile(const String& path, int maxCount = -1);
    LogEvent parseLogLine(const String& line);

public:
    EventLogger();
    ~EventLogger();
    
    bool begin();
    void log(EventLevel level, const String& category, const String& message);
    
    // Convenience methods
    void info(const String& category, const String& message);
    void warning(const String& category, const String& message);
    void error(const String& category, const String& message);
    void critical(const String& category, const String& message);
    
    // Retrieval
    std::vector<LogEvent> getRecentLogs(int count = 50);
    std::vector<LogEvent> getLogsByLevel(EventLevel level, int count = 50);
    std::vector<LogEvent> getLogsByCategory(const String& category, int count = 50);
    
    // Utilities
    void clearLogs();
    int getLogCount();
    String formatEvent(const LogEvent& event);
    
    // SPIFFS info
    bool isSPIFFSAvailable() { return spiffsAvailable; }
    size_t getLogFileSize();
    void exportLogsToSerial();
};

#endif
