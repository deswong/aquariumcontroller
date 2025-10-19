#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <unity.h>
#include <cstdio>
#include <cstring>
#include <ctime>

// Shared mock functions declarations
extern unsigned long millis();
extern void delay(unsigned long ms);
extern float constrain(float value, float min, float max);
extern void setMockMillis(unsigned long value);
extern void advanceMockMillis(unsigned long amount);

// Time mock functions
extern time_t time(time_t* t);
extern struct tm* localtime_r(const time_t* timer, struct tm* buf);
extern void setMockTime(int year, int month, int day, int hour, int minute, int second);

// Common setUp/tearDown
void commonSetUp();
void commonTearDown();

#endif // TEST_COMMON_H