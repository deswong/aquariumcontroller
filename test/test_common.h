#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <unity.h>
#include <cstring>
#include <cmath>
#include <stdint.h>

// Mock Arduino types and constants
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Mock time functions
extern unsigned long mock_millis;
unsigned long millis();
unsigned long micros();
void setMockMillis(unsigned long value);
void advanceMockMillis(unsigned long delta);
void setMockTime(int year, int month, int day, int hour, int minute, int second);

// Mock Arduino functions
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int analogRead(int pin);
void delay(unsigned long ms);
void delayMicroseconds(unsigned long us);

// Mock Serial for testing
class MockSerial {
public:
    void begin(unsigned long baud) {}
    void print(const char* str) {}
    void println(const char* str) {}
    void printf(const char* format, ...) {}
};

extern MockSerial Serial;

// Common test utilities
float calculateFlowRate(float volume_ml, float duration_sec, float speed_percent);
bool validatePHValue(float ph);
bool validateTemperature(float temp);

// Test setup/teardown
void commonSetUp();
void commonTearDown();

#endif // TEST_COMMON_H