#include <unity.h>
#include "test_common.h"
#include <time.h>
#include <map>
#include <string>

// Mock time values
static time_t mock_current_time = 0;
static struct tm mock_timeinfo = {0};

// Mock time function
time_t time(time_t *t) {
    if (t != nullptr) {
        *t = mock_current_time;
    }
    return mock_current_time;
}

// Mock localtime_r function
struct tm* localtime_r(const time_t *timep, struct tm *result) {
    if (result != nullptr) {
        *result = mock_timeinfo;
    }
    return result;
}

// Helper to set mock time
void setMockUnixTime(int year, int month, int day, int hour, int minute, int second) {
    mock_timeinfo.tm_year = year - 1900;
    mock_timeinfo.tm_mon = month - 1;
    mock_timeinfo.tm_mday = day;
    mock_timeinfo.tm_hour = hour;
    mock_timeinfo.tm_min = minute;
    mock_timeinfo.tm_sec = second;
    
    // Calculate Unix timestamp (simplified - assumes UTC)
    // Days since epoch (Jan 1, 1970)
    int years_since_epoch = year - 1970;
    int days = years_since_epoch * 365 + (years_since_epoch / 4); // Approximate leap years
    days += (month - 1) * 30; // Approximate month days
    days += day - 1;
    
    mock_current_time = (time_t)(days * 86400UL + hour * 3600UL + minute * 60UL + second);
}

// Mock NVS storage for water change records
static std::map<std::string, uint32_t> mockNVS_uint32;
static std::map<std::string, float> mockNVS_float;

// Mock WaterChangeAssistant class for testing
class MockWaterChangeAssistant {
private:
    time_t lastChangeTime;
    uint32_t changeIntervalDays;
    
public:
    MockWaterChangeAssistant() : lastChangeTime(0), changeIntervalDays(7) {}
    
    void setLastChangeTime(time_t timestamp) {
        lastChangeTime = timestamp;
    }
    
    void setChangeInterval(uint32_t days) {
        changeIntervalDays = days;
    }
    
    time_t getLastChangeTime() const {
        return lastChangeTime;
    }
    
    // Main function under test: Days since last water change
    int getDaysSinceLastChange() {
        if (lastChangeTime == 0) {
            return 999; // Never changed
        }
        
        time_t now;
        time(&now);
        
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        // Validate NTP sync - check if year is reasonable
        if (timeinfo.tm_year <= (2020 - 1900)) {
            return 999; // NTP not synced yet
        }
        
        unsigned long timeSinceLastChange = now - lastChangeTime;
        return timeSinceLastChange / 86400; // Convert seconds to days
    }
    
    // Main function under test: Days until next water change
    int getDaysUntilNextChange() {
        if (lastChangeTime == 0) {
            return -1; // Never changed, can't predict
        }
        
        time_t now;
        time(&now);
        
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        // Validate NTP sync
        if (timeinfo.tm_year <= (2020 - 1900)) {
            return -1; // NTP not synced yet
        }
        
        time_t nextChangeTime = lastChangeTime + (changeIntervalDays * 86400);
        int daysUntil = (nextChangeTime - now) / 86400;
        
        return daysUntil;
    }
    
    // Check if water change is due
    bool isChangeDue() {
        int daysUntil = getDaysUntilNextChange();
        return (daysUntil <= 0 && daysUntil != -1);
    }
};

// Test functions
void test_water_change_never_changed() {
    MockWaterChangeAssistant wca;
    
    // Set current time to valid date (2024-01-15)
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    
    // Never changed - should return sentinel values
    TEST_ASSERT_EQUAL_INT(999, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(-1, wca.getDaysUntilNextChange());
    TEST_ASSERT_FALSE(wca.isChangeDue());
}

void test_water_change_same_day() {
    MockWaterChangeAssistant wca;
    
    // Set current time to 2024-01-15 12:00:00
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    time_t change_time = mock_current_time;
    
    // Record water change at current time
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7); // 7-day interval
    
    // Check immediately - should be 0 days since
    TEST_ASSERT_EQUAL_INT(0, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(7, wca.getDaysUntilNextChange());
    TEST_ASSERT_FALSE(wca.isChangeDue());
}

void test_water_change_one_day_later() {
    MockWaterChangeAssistant wca;
    
    // Last change: 2024-01-15 12:00:00
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7);
    
    // Move forward 1 day: 2024-01-16 12:00:00
    setMockUnixTime(2024, 1, 16, 12, 0, 0);
    
    // Should be 1 day since, 6 days until next
    TEST_ASSERT_EQUAL_INT(1, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(6, wca.getDaysUntilNextChange());
    TEST_ASSERT_FALSE(wca.isChangeDue());
}

void test_water_change_exactly_at_interval() {
    MockWaterChangeAssistant wca;
    
    // Last change: 2024-01-15 12:00:00
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7);
    
    // Move forward exactly 7 days: 2024-01-22 12:00:00
    setMockUnixTime(2024, 1, 22, 12, 0, 0);
    
    // Should be 7 days since, 0 days until next (due now)
    TEST_ASSERT_EQUAL_INT(7, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(0, wca.getDaysUntilNextChange());
    TEST_ASSERT_TRUE(wca.isChangeDue());
}

void test_water_change_overdue() {
    MockWaterChangeAssistant wca;
    
    // Last change: 2024-01-15 12:00:00
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7);
    
    // Move forward 10 days: 2024-01-25 12:00:00
    setMockUnixTime(2024, 1, 25, 12, 0, 0);
    
    // Should be 10 days since, -3 days until next (overdue by 3 days)
    TEST_ASSERT_EQUAL_INT(10, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(-3, wca.getDaysUntilNextChange());
    TEST_ASSERT_TRUE(wca.isChangeDue());
}

void test_water_change_ntp_not_synced() {
    MockWaterChangeAssistant wca;
    
    // Set invalid time (epoch - 1970)
    setMockUnixTime(1970, 1, 1, 0, 0, 0);
    
    // Set a valid last change time (but current time is invalid)
    wca.setLastChangeTime(1705320000); // Some valid timestamp
    wca.setChangeInterval(7);
    
    // Should return error codes due to NTP not synced
    TEST_ASSERT_EQUAL_INT(999, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(-1, wca.getDaysUntilNextChange());
}

void test_water_change_ntp_just_synced() {
    MockWaterChangeAssistant wca;
    
    // Start with invalid time
    setMockUnixTime(1970, 1, 1, 0, 0, 0);
    TEST_ASSERT_EQUAL_INT(999, wca.getDaysSinceLastChange());
    
    // NTP sync happens - set valid time
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7);
    
    // Should now work correctly
    TEST_ASSERT_EQUAL_INT(0, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(7, wca.getDaysUntilNextChange());
}

void test_water_change_different_intervals() {
    MockWaterChangeAssistant wca;
    
    // Last change: 2024-01-15 12:00:00
    setMockUnixTime(2024, 1, 15, 12, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    
    // Test 3-day interval
    wca.setChangeInterval(3);
    setMockUnixTime(2024, 1, 18, 12, 0, 0); // 3 days later
    TEST_ASSERT_EQUAL_INT(3, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(0, wca.getDaysUntilNextChange());
    
    // Test 14-day interval (bi-weekly)
    wca.setChangeInterval(14);
    setMockUnixTime(2024, 1, 22, 12, 0, 0); // 7 days later
    TEST_ASSERT_EQUAL_INT(7, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(7, wca.getDaysUntilNextChange());
    
    // Test 30-day interval (monthly)
    wca.setChangeInterval(30);
    setMockUnixTime(2024, 2, 14, 12, 0, 0); // 30 days later
    TEST_ASSERT_EQUAL_INT(30, wca.getDaysSinceLastChange());
    TEST_ASSERT_EQUAL_INT(0, wca.getDaysUntilNextChange());
}

void test_water_change_time_boundaries() {
    MockWaterChangeAssistant wca;
    
    // Last change at midnight
    setMockUnixTime(2024, 1, 15, 0, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7);
    
    // Check at 23:59:59 same day (still day 0)
    setMockUnixTime(2024, 1, 15, 23, 59, 59);
    TEST_ASSERT_EQUAL_INT(0, wca.getDaysSinceLastChange());
    
    // Check at 00:00:01 next day (now day 1)
    setMockUnixTime(2024, 1, 16, 0, 0, 1);
    TEST_ASSERT_EQUAL_INT(1, wca.getDaysSinceLastChange());
}

void test_water_change_long_term_tracking() {
    MockWaterChangeAssistant wca;
    
    // Last change: 2024-01-01 12:00:00
    setMockUnixTime(2024, 1, 1, 12, 0, 0);
    time_t change_time = mock_current_time;
    wca.setLastChangeTime(change_time);
    wca.setChangeInterval(7);
    
    // Check 100 days later: 2024-04-10 12:00:00
    setMockUnixTime(2024, 4, 10, 12, 0, 0);
    
    // Should be 100 days since last change
    int days_since = wca.getDaysSinceLastChange();
    TEST_ASSERT_INT_WITHIN(1, 100, days_since); // Allow ±1 day for month calculation approximation
}

void setUp(void) {
    commonSetUp();
    // Reset mock time to default
    mock_current_time = 0;
    mock_timeinfo = {0};
}

void tearDown(void) {
    commonTearDown();
}

// Arduino setup function for ESP32 testing
void setup(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_water_change_never_changed);
    RUN_TEST(test_water_change_same_day);
    RUN_TEST(test_water_change_one_day_later);
    RUN_TEST(test_water_change_exactly_at_interval);
    RUN_TEST(test_water_change_overdue);
    RUN_TEST(test_water_change_ntp_not_synced);
    RUN_TEST(test_water_change_ntp_just_synced);
    RUN_TEST(test_water_change_different_intervals);
    RUN_TEST(test_water_change_time_boundaries);
    RUN_TEST(test_water_change_long_term_tracking);
    
    UNITY_END();
}

// Arduino loop function for ESP32 testing
void loop(void) {
    // Empty loop - tests run once in setup()
}

#ifdef NATIVE
// Native main function for PC testing
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_water_change_never_changed);
    RUN_TEST(test_water_change_same_day);
    RUN_TEST(test_water_change_one_day_later);
    RUN_TEST(test_water_change_exactly_at_interval);
    RUN_TEST(test_water_change_overdue);
    RUN_TEST(test_water_change_ntp_not_synced);
    RUN_TEST(test_water_change_ntp_just_synced);
    RUN_TEST(test_water_change_different_intervals);
    RUN_TEST(test_water_change_time_boundaries);
    RUN_TEST(test_water_change_long_term_tracking);
    
    return UNITY_END();
}
#endif
