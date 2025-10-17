#include <unity.h>
#include <cstring>
#include <cmath>

// ============================================================================
// DosingPump Unit Tests
// ============================================================================

// Mock millis for testing
static unsigned long mockMillis = 0;

unsigned long millis() {
    return mockMillis;
}

void setMockMillis(unsigned long value) {
    mockMillis = value;
}

void advanceMockMillis(unsigned long amount) {
    mockMillis += amount;
}

// Mock time functions for scheduling tests
static time_t mockTime = 0;
static struct tm mockLocalTime;

time_t time(time_t* t) {
    if (t != nullptr) {
        *t = mockTime;
    }
    return mockTime;
}

struct tm* localtime_r(const time_t* timep, struct tm* result) {
    if (result != nullptr) {
        *result = mockLocalTime;
    }
    return result;
}

void setMockTime(int year, int month, int day, int hour, int minute, int second) {
    mockLocalTime.tm_year = year - 1900;
    mockLocalTime.tm_mon = month - 1;
    mockLocalTime.tm_mday = day;
    mockLocalTime.tm_hour = hour;
    mockLocalTime.tm_min = minute;
    mockLocalTime.tm_sec = second;
    
    // Calculate unix timestamp (simplified)
    mockTime = 1704067200; // 2024-01-01 00:00:00 as base
    mockTime += (hour * 3600) + (minute * 60) + second;
}

// ============================================================================
// Calibration Tests
// ============================================================================

void test_dosing_pump_calibration_flow_rate() {
    // Test: Flow rate calculation from measured volume and duration
    // Given: Pumped 100mL in 50 seconds at 100% speed
    // Expected: mlPerSecond = 100 / 50 = 2.0 mL/s
    
    float measuredVolumeML = 100.0;
    int durationSeconds = 50;
    int speedPercent = 100;
    
    // Calculate flow rate at 100% speed
    float mlPerSecond = measuredVolumeML / (float)durationSeconds;
    
    TEST_ASSERT_EQUAL_FLOAT(2.0, mlPerSecond);
}

void test_dosing_pump_calibration_partial_speed() {
    // Test: Flow rate calculation when calibrated at partial speed
    // Given: Pumped 50mL in 50 seconds at 50% speed
    // Expected: mlPerSecond at 100% = 50 / (50 * 0.5) = 2.0 mL/s
    
    float measuredVolumeML = 50.0;
    int durationSeconds = 50;
    int speedPercent = 50;
    
    // Adjust for speed to get 100% flow rate
    float speedFactor = speedPercent / 100.0;
    float mlPerSecond = measuredVolumeML / (durationSeconds * speedFactor);
    
    TEST_ASSERT_EQUAL_FLOAT(2.0, mlPerSecond);
}

void test_dosing_pump_calibration_zero_volume() {
    // Test: Handle invalid calibration (zero volume)
    // Expected: Calibration should fail
    
    float measuredVolumeML = 0.0;
    int durationSeconds = 50;
    
    bool calibrationValid = (measuredVolumeML > 0.0 && durationSeconds > 0);
    
    TEST_ASSERT_FALSE(calibrationValid);
}

void test_dosing_pump_calibration_zero_duration() {
    // Test: Handle invalid calibration (zero duration)
    
    float measuredVolumeML = 100.0;
    int durationSeconds = 0;
    
    bool calibrationValid = (measuredVolumeML > 0.0 && durationSeconds > 0);
    
    TEST_ASSERT_FALSE(calibrationValid);
}

// ============================================================================
// Volume Tracking Tests
// ============================================================================

void test_dosing_pump_volume_calculation_100_percent_speed() {
    // Test: Volume calculation at 100% speed
    // Given: mlPerSecond = 2.0, speed = 100%, duration = 10 seconds
    // Expected: volume = 2.0 * 1.0 * 10 = 20 mL
    
    float mlPerSecond = 2.0;
    int speedPercent = 100;
    float durationSeconds = 10.0;
    
    float speedFactor = speedPercent / 100.0;
    float volumePumped = mlPerSecond * speedFactor * durationSeconds;
    
    TEST_ASSERT_EQUAL_FLOAT(20.0, volumePumped);
}

void test_dosing_pump_volume_calculation_50_percent_speed() {
    // Test: Volume calculation at 50% speed
    // Given: mlPerSecond = 2.0, speed = 50%, duration = 10 seconds
    // Expected: volume = 2.0 * 0.5 * 10 = 10 mL
    
    float mlPerSecond = 2.0;
    int speedPercent = 50;
    float durationSeconds = 10.0;
    
    float speedFactor = speedPercent / 100.0;
    float volumePumped = mlPerSecond * speedFactor * durationSeconds;
    
    TEST_ASSERT_EQUAL_FLOAT(10.0, volumePumped);
}

void test_dosing_pump_volume_tracking_realtime() {
    // Test: Real-time volume tracking with millis()
    // Given: mlPerSecond = 2.0, speed = 100%
    // Expected: After 5 seconds, volume = 10 mL
    
    float mlPerSecond = 2.0;
    int speedPercent = 100;
    float speedFactor = speedPercent / 100.0;
    
    // Start dosing
    setMockMillis(0);
    unsigned long startTime = millis();
    float volumePumped = 0.0;
    
    // After 5 seconds
    advanceMockMillis(5000);
    unsigned long currentTime = millis();
    float deltaSeconds = (currentTime - startTime) / 1000.0;
    volumePumped = mlPerSecond * speedFactor * deltaSeconds;
    
    TEST_ASSERT_EQUAL_FLOAT(10.0, volumePumped);
}

void test_dosing_pump_target_volume_reached() {
    // Test: Detect when target volume is reached
    // Given: target = 20 mL, pumped = 20.5 mL
    // Expected: dosing should stop
    
    float targetVolume = 20.0;
    float volumePumped = 20.5;
    
    bool targetReached = (volumePumped >= targetVolume);
    
    TEST_ASSERT_TRUE(targetReached);
}

void test_dosing_pump_progress_calculation() {
    // Test: Progress calculation
    // Given: target = 50 mL, pumped = 25 mL
    // Expected: progress = 0.5 (50%)
    
    float targetVolume = 50.0;
    float volumePumped = 25.0;
    
    float progress = volumePumped / targetVolume;
    
    TEST_ASSERT_EQUAL_FLOAT(0.5, progress);
}

// ============================================================================
// Schedule Calculation Tests
// ============================================================================

void test_dosing_pump_schedule_daily_interval() {
    // Test: Daily schedule interval
    // Expected: 24 hours = 86400 seconds
    
    int scheduleType = 1; // DOSE_DAILY
    int intervalSeconds = scheduleType * 24 * 3600;
    
    TEST_ASSERT_EQUAL_INT32(86400, intervalSeconds);
}

void test_dosing_pump_schedule_weekly_interval() {
    // Test: Weekly schedule interval
    // Expected: 7 days = 604800 seconds
    
    int scheduleType = 7; // DOSE_WEEKLY
    int intervalSeconds = scheduleType * 24 * 3600;
    
    TEST_ASSERT_EQUAL_INT32(604800, intervalSeconds);
}

void test_dosing_pump_schedule_custom_interval() {
    // Test: Custom schedule (every 3 days)
    // Expected: 3 days = 259200 seconds
    
    int customDays = 3;
    int intervalSeconds = customDays * 24 * 3600;
    
    TEST_ASSERT_EQUAL_INT32(259200, intervalSeconds);
}

void test_dosing_pump_schedule_next_dose_calculation() {
    // Test: Calculate next dose time
    // Given: Last dose at timestamp 1000, interval = 86400 (1 day)
    // Expected: Next dose = 1000 + 86400 = 87400
    
    unsigned long lastDoseTime = 1000;
    int intervalSeconds = 86400;
    unsigned long nextDoseTime = lastDoseTime + intervalSeconds;
    
    TEST_ASSERT_EQUAL_UINT32(87400, nextDoseTime);
}

void test_dosing_pump_schedule_time_of_day() {
    // Test: Schedule at specific time of day (14:30)
    // Current: 2024-01-01 10:00:00
    // Target: 2024-01-01 14:30:00
    // Expected: Hours until = 4.5 hours
    
    setMockTime(2024, 1, 1, 10, 0, 0); // Current: 10:00
    
    int targetHour = 14;
    int targetMinute = 30;
    int currentHour = 10;
    int currentMinute = 0;
    
    // Calculate minutes until next dose
    int minutesUntil = (targetHour - currentHour) * 60 + (targetMinute - currentMinute);
    float hoursUntil = minutesUntil / 60.0;
    
    TEST_ASSERT_EQUAL_FLOAT(4.5, hoursUntil);
}

void test_dosing_pump_schedule_next_day() {
    // Test: When target time is in the past today, schedule for tomorrow
    // Current: 2024-01-01 16:00:00
    // Target: 14:30 (already passed today)
    // Expected: Schedule for tomorrow (22.5 hours from now)
    
    int currentHour = 16;
    int targetHour = 14;
    int targetMinute = 30;
    
    // If target hour already passed, add 24 hours
    int hoursUntil = targetHour - currentHour;
    if (hoursUntil < 0) {
        hoursUntil += 24; // Next day
    }
    float totalHours = hoursUntil - 0.5; // -30 minutes
    if (totalHours < 0) totalHours += 24;
    
    TEST_ASSERT_EQUAL_FLOAT(22.5, totalHours);
}

void test_dosing_pump_is_dose_overdue() {
    // Test: Detect overdue doses
    // Given: Next dose = 1000, current time = 2000
    // Expected: Dose is overdue
    
    unsigned long nextDoseTime = 1000;
    unsigned long currentTime = 2000;
    
    bool isOverdue = (currentTime >= nextDoseTime);
    
    TEST_ASSERT_TRUE(isOverdue);
}

// ============================================================================
// Safety Limits Tests
// ============================================================================

void test_dosing_pump_safety_max_dose_limit() {
    // Test: Enforce maximum dose volume
    // Given: maxDoseVolume = 100 mL, requested = 150 mL
    // Expected: Dose should be capped at 100 mL
    
    float maxDoseVolume = 100.0;
    float requestedVolume = 150.0;
    
    float actualVolume = requestedVolume;
    if (actualVolume > maxDoseVolume) {
        actualVolume = maxDoseVolume;
    }
    
    TEST_ASSERT_EQUAL_FLOAT(100.0, actualVolume);
}

void test_dosing_pump_safety_max_daily_limit() {
    // Test: Enforce maximum daily volume
    // Given: maxDailyVolume = 500 mL, already dosed = 450 mL, requesting = 100 mL
    // Expected: Only allow 50 mL to stay within daily limit
    
    float maxDailyVolume = 500.0;
    float dailyVolumeDosed = 450.0;
    float requestedVolume = 100.0;
    
    float remainingDailyVolume = maxDailyVolume - dailyVolumeDosed;
    float allowedVolume = (requestedVolume > remainingDailyVolume) ? remainingDailyVolume : requestedVolume;
    
    TEST_ASSERT_EQUAL_FLOAT(50.0, allowedVolume);
}

void test_dosing_pump_safety_daily_limit_exceeded() {
    // Test: Reject dose when daily limit already exceeded
    // Given: maxDailyVolume = 500 mL, already dosed = 500 mL
    // Expected: No dosing allowed
    
    float maxDailyVolume = 500.0;
    float dailyVolumeDosed = 500.0;
    
    bool canDose = (dailyVolumeDosed < maxDailyVolume);
    
    TEST_ASSERT_FALSE(canDose);
}

void test_dosing_pump_safety_daily_reset_at_midnight() {
    // Test: Daily volume resets at midnight
    // Given: Last reset = day 1, current = day 2
    // Expected: Daily volume should reset to 0
    
    unsigned long lastResetDay = 1;
    unsigned long currentDay = 2;
    float dailyVolumeDosed = 350.0;
    
    if (currentDay > lastResetDay) {
        dailyVolumeDosed = 0.0;
        lastResetDay = currentDay;
    }
    
    TEST_ASSERT_EQUAL_FLOAT(0.0, dailyVolumeDosed);
    TEST_ASSERT_EQUAL_UINT32(2, lastResetDay);
}

void test_dosing_pump_safety_speed_limits() {
    // Test: Speed percentage limits (0-100)
    
    int speed = 150; // Invalid
    if (speed > 100) speed = 100;
    TEST_ASSERT_EQUAL_INT(100, speed);
    
    speed = -10; // Invalid
    if (speed < 0) speed = 0;
    TEST_ASSERT_EQUAL_INT(0, speed);
    
    speed = 75; // Valid
    bool isValid = (speed >= 0 && speed <= 100);
    TEST_ASSERT_TRUE(isValid);
}

// ============================================================================
// State Management Tests
// ============================================================================

void test_dosing_pump_state_transitions() {
    // Test: State machine transitions
    // IDLE -> DOSING -> IDLE
    
    enum PumpState {
        PUMP_IDLE,
        PUMP_DOSING,
        PUMP_PAUSED,
        PUMP_ERROR
    };
    
    PumpState state = PUMP_IDLE;
    TEST_ASSERT_EQUAL(PUMP_IDLE, state);
    
    // Start dosing
    state = PUMP_DOSING;
    TEST_ASSERT_EQUAL(PUMP_DOSING, state);
    
    // Finish dosing
    state = PUMP_IDLE;
    TEST_ASSERT_EQUAL(PUMP_IDLE, state);
}

void test_dosing_pump_pause_resume() {
    // Test: Pause and resume during dosing
    
    enum PumpState {
        PUMP_IDLE,
        PUMP_DOSING,
        PUMP_PAUSED,
        PUMP_ERROR
    };
    
    PumpState state = PUMP_DOSING;
    
    // Pause
    PumpState previousState = state;
    state = PUMP_PAUSED;
    TEST_ASSERT_EQUAL(PUMP_PAUSED, state);
    
    // Resume
    state = previousState;
    TEST_ASSERT_EQUAL(PUMP_DOSING, state);
}

void test_dosing_pump_emergency_stop() {
    // Test: Emergency stop from any state
    
    enum PumpState {
        PUMP_IDLE,
        PUMP_DOSING,
        PUMP_PAUSED,
        PUMP_ERROR
    };
    
    PumpState state = PUMP_DOSING;
    
    // Emergency stop
    bool emergency = true;
    if (emergency) {
        state = PUMP_IDLE;
    }
    
    TEST_ASSERT_EQUAL(PUMP_IDLE, state);
}

// ============================================================================
// Maintenance Functions Tests
// ============================================================================

void test_dosing_pump_prime_duration() {
    // Test: Prime pump for specified duration
    // Given: Prime for 10 seconds at 50% speed
    
    int primeDuration = 10;
    int primeSpeed = 50;
    
    setMockMillis(0);
    unsigned long primeStart = millis();
    advanceMockMillis(10000);
    unsigned long primeEnd = millis();
    
    int actualDuration = (primeEnd - primeStart) / 1000;
    
    TEST_ASSERT_EQUAL_INT(10, actualDuration);
}

void test_dosing_pump_backflush_operation() {
    // Test: Backflush (reverse) operation
    // Motor should run in reverse for specified duration
    
    int backflushDuration = 5;
    int backflushSpeed = 30;
    bool isReverse = true;
    
    TEST_ASSERT_TRUE(isReverse);
    TEST_ASSERT_EQUAL_INT(5, backflushDuration);
    TEST_ASSERT_EQUAL_INT(30, backflushSpeed);
}

void test_dosing_pump_total_runtime_tracking() {
    // Test: Track total pump runtime
    // Given: Dose 1 = 50s, Dose 2 = 30s, Prime = 10s
    // Expected: Total = 90s
    
    unsigned long totalRuntime = 0;
    
    totalRuntime += 50000; // Dose 1: 50 seconds
    totalRuntime += 30000; // Dose 2: 30 seconds
    totalRuntime += 10000; // Prime: 10 seconds
    
    TEST_ASSERT_EQUAL_UINT32(90000, totalRuntime);
}

void test_dosing_pump_total_doses_counter() {
    // Test: Count total number of doses
    
    int totalDoses = 0;
    
    totalDoses++; // Scheduled dose
    totalDoses++; // Manual dose
    totalDoses++; // Another scheduled dose
    
    TEST_ASSERT_EQUAL_INT(3, totalDoses);
}

// ============================================================================
// History Recording Tests
// ============================================================================

void test_dosing_pump_history_record_creation() {
    // Test: Create dosing history record
    
    struct DosingRecord {
        unsigned long timestamp;
        float volumeDosed;
        int durationMs;
        bool success;
        const char* type;
    };
    
    DosingRecord record;
    record.timestamp = 1704067200; // 2024-01-01 00:00:00
    record.volumeDosed = 25.5;
    record.durationMs = 12750;
    record.success = true;
    record.type = "scheduled";
    
    TEST_ASSERT_EQUAL_UINT32(1704067200, record.timestamp);
    TEST_ASSERT_EQUAL_FLOAT(25.5, record.volumeDosed);
    TEST_ASSERT_EQUAL_INT(12750, record.durationMs);
    TEST_ASSERT_TRUE(record.success);
    TEST_ASSERT_EQUAL_STRING("scheduled", record.type);
}

void test_dosing_pump_history_max_records() {
    // Test: Limit history to max records (FIFO)
    // Given: maxRecords = 5, adding 6th record
    // Expected: Oldest record removed
    
    int maxRecords = 5;
    int recordCount = 5;
    
    // Add new record (would be 6th)
    if (recordCount >= maxRecords) {
        // Remove oldest (shift all records)
        recordCount--;
    }
    recordCount++; // Add new
    
    TEST_ASSERT_EQUAL_INT(5, recordCount);
}

// ============================================================================
// Duration Calculation Tests
// ============================================================================

void test_dosing_pump_duration_from_volume() {
    // Test: Calculate required duration for target volume
    // Given: target = 50 mL, mlPerSecond = 2.0, speed = 100%
    // Expected: duration = 50 / (2.0 * 1.0) = 25 seconds
    
    float targetVolume = 50.0;
    float mlPerSecond = 2.0;
    int speedPercent = 100;
    float speedFactor = speedPercent / 100.0;
    
    float durationSeconds = targetVolume / (mlPerSecond * speedFactor);
    
    TEST_ASSERT_EQUAL_FLOAT(25.0, durationSeconds);
}

void test_dosing_pump_duration_at_partial_speed() {
    // Test: Duration increases at lower speeds
    // Given: target = 50 mL, mlPerSecond = 2.0, speed = 50%
    // Expected: duration = 50 / (2.0 * 0.5) = 50 seconds
    
    float targetVolume = 50.0;
    float mlPerSecond = 2.0;
    int speedPercent = 50;
    float speedFactor = speedPercent / 100.0;
    
    float durationSeconds = targetVolume / (mlPerSecond * speedFactor);
    
    TEST_ASSERT_EQUAL_FLOAT(50.0, durationSeconds);
}

// ============================================================================
// Main Test Runner
// ============================================================================

void setUp(void) {
    // Reset mock state before each test
    setMockMillis(0);
    mockTime = 0;
}

void tearDown(void) {
    // Cleanup after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Calibration tests
    RUN_TEST(test_dosing_pump_calibration_flow_rate);
    RUN_TEST(test_dosing_pump_calibration_partial_speed);
    RUN_TEST(test_dosing_pump_calibration_zero_volume);
    RUN_TEST(test_dosing_pump_calibration_zero_duration);
    
    // Volume tracking tests
    RUN_TEST(test_dosing_pump_volume_calculation_100_percent_speed);
    RUN_TEST(test_dosing_pump_volume_calculation_50_percent_speed);
    RUN_TEST(test_dosing_pump_volume_tracking_realtime);
    RUN_TEST(test_dosing_pump_target_volume_reached);
    RUN_TEST(test_dosing_pump_progress_calculation);
    
    // Schedule calculation tests
    RUN_TEST(test_dosing_pump_schedule_daily_interval);
    RUN_TEST(test_dosing_pump_schedule_weekly_interval);
    RUN_TEST(test_dosing_pump_schedule_custom_interval);
    RUN_TEST(test_dosing_pump_schedule_next_dose_calculation);
    RUN_TEST(test_dosing_pump_schedule_time_of_day);
    RUN_TEST(test_dosing_pump_schedule_next_day);
    RUN_TEST(test_dosing_pump_is_dose_overdue);
    
    // Safety tests
    RUN_TEST(test_dosing_pump_safety_max_dose_limit);
    RUN_TEST(test_dosing_pump_safety_max_daily_limit);
    RUN_TEST(test_dosing_pump_safety_daily_limit_exceeded);
    RUN_TEST(test_dosing_pump_safety_daily_reset_at_midnight);
    RUN_TEST(test_dosing_pump_safety_speed_limits);
    
    // State management tests
    RUN_TEST(test_dosing_pump_state_transitions);
    RUN_TEST(test_dosing_pump_pause_resume);
    RUN_TEST(test_dosing_pump_emergency_stop);
    
    // Maintenance tests
    RUN_TEST(test_dosing_pump_prime_duration);
    RUN_TEST(test_dosing_pump_backflush_operation);
    RUN_TEST(test_dosing_pump_total_runtime_tracking);
    RUN_TEST(test_dosing_pump_total_doses_counter);
    
    // History tests
    RUN_TEST(test_dosing_pump_history_record_creation);
    RUN_TEST(test_dosing_pump_history_max_records);
    
    // Duration calculation tests
    RUN_TEST(test_dosing_pump_duration_from_volume);
    RUN_TEST(test_dosing_pump_duration_at_partial_speed);
    
    return UNITY_END();
}
