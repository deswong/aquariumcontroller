#include "Logger.h"
#include <stdarg.h>
#include <stdio.h>

// Static member initialization
LogLevel Logger::currentLevel = LOG_LEVEL_INFO;
bool Logger::colorEnabled = true;
SemaphoreHandle_t Logger::logMutex = nullptr;

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_MAGENTA "\033[35m"

void Logger::init(LogLevel level) {
    currentLevel = level;
    logMutex = xSemaphoreCreateMutex();
    
    if (logMutex != nullptr) {
        LOG_INFO("Logger", "Logger initialized with level: %s", getLevelString(level));
    }
}

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

LogLevel Logger::getLevel() {
    return currentLevel;
}

void Logger::setColorOutput(bool enable) {
    colorEnabled = enable;
}

const char* Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        default: return "NONE ";
    }
}

const char* Logger::getLevelColor(LogLevel level) {
    if (!colorEnabled) return "";
    
    switch (level) {
        case LOG_LEVEL_DEBUG: return COLOR_CYAN;
        case LOG_LEVEL_INFO:  return COLOR_GREEN;
        case LOG_LEVEL_WARN:  return COLOR_YELLOW;
        case LOG_LEVEL_ERROR: return COLOR_RED;
        default: return COLOR_RESET;
    }
}

void Logger::log(LogLevel level, const char* tag, const char* format, va_list args) {
    // Skip if below current log level
    if (level < currentLevel) {
        return;
    }
    
    // Thread-safe logging
    if (logMutex != nullptr && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Print timestamp
        unsigned long ms = millis();
        Serial.printf("[%6lu.%03lu] ", ms / 1000, ms % 1000);
        
        // Print level with color
        if (colorEnabled) {
            Serial.print(getLevelColor(level));
        }
        Serial.printf("[%s] ", getLevelString(level));
        
        // Print tag
        Serial.printf("[%-12s] ", tag);
        
        // Print message
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Serial.print(buffer);
        
        // Reset color
        if (colorEnabled) {
            Serial.print(COLOR_RESET);
        }
        
        Serial.println();
        
        xSemaphoreGive(logMutex);
    }
}

void Logger::debug(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_DEBUG, tag, format, args);
    va_end(args);
}

void Logger::info(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_INFO, tag, format, args);
    va_end(args);
}

void Logger::warn(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_WARN, tag, format, args);
    va_end(args);
}

void Logger::error(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LOG_LEVEL_ERROR, tag, format, args);
    va_end(args);
}

void Logger::logTask(const char* taskName, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char tag[32];
    snprintf(tag, sizeof(tag), "Task:%s", taskName);
    log(LOG_LEVEL_DEBUG, tag, format, args);
    
    va_end(args);
}

void Logger::logSensor(const char* sensorName, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char tag[32];
    snprintf(tag, sizeof(tag), "Sensor:%s", sensorName);
    log(LOG_LEVEL_INFO, tag, format, args);
    
    va_end(args);
}

void Logger::logNetwork(const char* component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char tag[32];
    snprintf(tag, sizeof(tag), "Net:%s", component);
    log(LOG_LEVEL_INFO, tag, format, args);
    
    va_end(args);
}

void Logger::logML(const char* component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char tag[32];
    snprintf(tag, sizeof(tag), "ML:%s", component);
    log(LOG_LEVEL_DEBUG, tag, format, args);
    
    va_end(args);
}

void Logger::logPerformance(const char* operation, unsigned long durationUs) {
    if (currentLevel > LOG_LEVEL_DEBUG) {
        return;
    }
    
    if (logMutex != nullptr && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.printf("[PERF] %s: %lu us", operation, durationUs);
        
        if (durationUs > 10000) {
            Serial.printf(" (%.2f ms)", durationUs / 1000.0);
        }
        
        Serial.println();
        xSemaphoreGive(logMutex);
    }
}

void Logger::hexDump(const char* tag, const uint8_t* data, size_t length) {
    if (currentLevel > LOG_LEVEL_DEBUG || data == nullptr || length == 0) {
        return;
    }
    
    if (logMutex != nullptr && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.printf("[HEXDUMP] [%s] %zu bytes:\n", tag, length);
        
        for (size_t i = 0; i < length; i += 16) {
            Serial.printf("%04x: ", i);
            
            // Print hex values
            for (size_t j = 0; j < 16; j++) {
                if (i + j < length) {
                    Serial.printf("%02x ", data[i + j]);
                } else {
                    Serial.print("   ");
                }
            }
            
            Serial.print(" |");
            
            // Print ASCII representation
            for (size_t j = 0; j < 16 && i + j < length; j++) {
                char c = data[i + j];
                Serial.print((c >= 32 && c <= 126) ? c : '.');
            }
            
            Serial.println("|");
        }
        
        xSemaphoreGive(logMutex);
    }
}
