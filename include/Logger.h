#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Log levels
enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4
};

// Compile-time logging control
#ifndef LOG_LEVEL_COMPILE_TIME
    #ifdef DEBUG_MODE
        #define LOG_LEVEL_COMPILE_TIME LOG_LEVEL_DEBUG
    #else
        #define LOG_LEVEL_COMPILE_TIME LOG_LEVEL_INFO
    #endif
#endif

class Logger {
public:
    static void init(LogLevel level = LOG_LEVEL_INFO);
    static void setLevel(LogLevel level);
    static LogLevel getLevel();
    
    // Core logging methods
    static void debug(const char* tag, const char* format, ...);
    static void info(const char* tag, const char* format, ...);
    static void warn(const char* tag, const char* format, ...);
    static void error(const char* tag, const char* format, ...);
    
    // Specialized logging methods
    static void logTask(const char* taskName, const char* format, ...);
    static void logSensor(const char* sensorName, const char* format, ...);
    static void logNetwork(const char* component, const char* format, ...);
    static void logML(const char* component, const char* format, ...);
    
    // Performance logging
    static void logPerformance(const char* operation, unsigned long durationUs);
    
    // Hexdump for debugging
    static void hexDump(const char* tag, const uint8_t* data, size_t length);
    
    // Enable/disable color output
    static void setColorOutput(bool enable);
    
private:
    static LogLevel currentLevel;
    static bool colorEnabled;
    static SemaphoreHandle_t logMutex;
    
    static void log(LogLevel level, const char* tag, const char* format, va_list args);
    static const char* getLevelString(LogLevel level);
    static const char* getLevelColor(LogLevel level);
};

// Convenience macros that compile out based on LOG_LEVEL_COMPILE_TIME
#if LOG_LEVEL_COMPILE_TIME <= LOG_LEVEL_DEBUG
    #define LOG_DEBUG(tag, ...) Logger::debug(tag, __VA_ARGS__)
#else
    #define LOG_DEBUG(tag, ...) ((void)0)
#endif

#if LOG_LEVEL_COMPILE_TIME <= LOG_LEVEL_INFO
    #define LOG_INFO(tag, ...) Logger::info(tag, __VA_ARGS__)
#else
    #define LOG_INFO(tag, ...) ((void)0)
#endif

#if LOG_LEVEL_COMPILE_TIME <= LOG_LEVEL_WARN
    #define LOG_WARN(tag, ...) Logger::warn(tag, __VA_ARGS__)
#else
    #define LOG_WARN(tag, ...) ((void)0)
#endif

#if LOG_LEVEL_COMPILE_TIME <= LOG_LEVEL_ERROR
    #define LOG_ERROR(tag, ...) Logger::error(tag, __VA_ARGS__)
#else
    #define LOG_ERROR(tag, ...) ((void)0)
#endif

// Specialized macros
#define LOG_TASK(name, ...) Logger::logTask(name, __VA_ARGS__)
#define LOG_SENSOR(name, ...) Logger::logSensor(name, __VA_ARGS__)
#define LOG_NETWORK(comp, ...) Logger::logNetwork(comp, __VA_ARGS__)
#define LOG_ML(comp, ...) Logger::logML(comp, __VA_ARGS__)
#define LOG_PERF(op, us) Logger::logPerformance(op, us)

// Performance measurement helper
#define LOG_PERF_START() unsigned long __perf_start = micros()
#define LOG_PERF_END(operation) Logger::logPerformance(operation, micros() - __perf_start)

#endif // LOGGER_H
