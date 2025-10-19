#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <Arduino.h>
#include <vector>

enum EventLevel {
    EVENT_INFO,
    EVENT_WARNING,
    EVENT_ERROR,
    EVENT_CRITICAL
};

struct LogEvent {
    unsigned long timestamp;  // Milliseconds since boot
    EventLevel level;
    String category;
    String message;
};

class EventLogger {
private:
    String levelToString(EventLevel level);
    EventLevel stringToLevel(const String& str);

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
};

#endif
