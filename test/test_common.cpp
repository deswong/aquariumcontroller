#include "test_common.h"

// Mock time variables
unsigned long mock_millis = 0;

// Mock Arduino functions implementation
unsigned long millis() {
    return mock_millis;
}

unsigned long micros() {
    return mock_millis * 1000;
}

void setMockMillis(unsigned long value) {
    mock_millis = value;
}

void advanceMockMillis(unsigned long delta) {
    mock_millis += delta;
}

void setMockTime(int year, int month, int day, int hour, int minute, int second) {
    // Simple mock time implementation - just set millis based on time of day
    mock_millis = (hour * 3600 + minute * 60 + second) * 1000;
}

void pinMode(int pin, int mode) {
    // No-op in tests
}

void digitalWrite(int pin, int value) {
    // No-op in tests
}

int digitalRead(int pin) {
    return HIGH; // Default HIGH
}

int analogRead(int pin) {
    return 512; // Default mid-range value
}

void delay(unsigned long ms) {
    mock_millis += ms;
}

void delayMicroseconds(unsigned long us) {
    // No-op in tests, too small to matter
}

// Mock Serial (only for native testing)
#ifdef NATIVE
MockSerial Serial;
#endif

// Common test utilities
float calculateFlowRate(float volume_ml, float duration_sec, float speed_percent) {
    if (duration_sec <= 0 || speed_percent <= 0) return 0.0;
    float speed_factor = speed_percent / 100.0;
    return volume_ml / (duration_sec * speed_factor);
}

bool validatePHValue(float ph) {
    return ph >= 0.0 && ph <= 14.0;
}

bool validateTemperature(float temp) {
    return temp >= -50.0 && temp <= 100.0; // Reasonable range for aquarium
}

// Test setup/teardown functions
void commonSetUp() {
    // Reset mock state for each test
    mock_millis = 0;
}

void commonTearDown() {
    // Clean up after each test
    mock_millis = 0;
}