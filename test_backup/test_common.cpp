#include "test_common.h"

// Mock Arduino functions for native testing
static unsigned long mockMillis = 0;
static time_t mockTime = 0;
static struct tm mockLocalTime;

unsigned long millis() {
    return mockMillis;
}

void delay(unsigned long ms) {
    // No-op for testing
}

float constrain(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void setMockMillis(unsigned long value) {
    mockMillis = value;
}

void advanceMockMillis(unsigned long amount) {
    mockMillis += amount;
}

// Mock time functions
time_t time(time_t* t) {
    if (t != nullptr) {
        *t = mockTime;
    }
    return mockTime;
}

struct tm* localtime_r(const time_t* timer, struct tm* buf) {
    if (buf != nullptr) {
        *buf = mockLocalTime;
    }
    return &mockLocalTime;
}

void setMockTime(int year, int month, int day, int hour, int minute, int second) {
    mockLocalTime.tm_year = year - 1900; // tm_year is years since 1900
    mockLocalTime.tm_mon = month - 1;    // tm_mon is 0-11
    mockLocalTime.tm_mday = day;
    mockLocalTime.tm_hour = hour;
    mockLocalTime.tm_min = minute;
    mockLocalTime.tm_sec = second;
    
    // Calculate unix timestamp (simplified)
    mockTime = 1704067200; // 2024-01-01 00:00:00 as base
    mockTime += (hour * 3600) + (minute * 60) + second;
}

// Common setUp/tearDown functions
void commonSetUp() {
    mockMillis = 0;
    mockTime = 0;
    memset(&mockLocalTime, 0, sizeof(mockLocalTime));
}

void commonTearDown() {
    // Reset state after each test
    mockMillis = 0;
    mockTime = 0;
    memset(&mockLocalTime, 0, sizeof(mockLocalTime));
}