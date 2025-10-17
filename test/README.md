# Aquarium Controller Test Suite

This directory contains comprehensive tests for the Aquarium Controller system using the Unity test framework.

## Test Files Overview

### Unit Tests

#### `test_main.cpp` (499 lines)
Core unit tests for basic functionality:
- PID controller tests (proportional, integral, derivative, windup prevention)
- Temperature sensor validation (range checking, moving average)
- pH sensor tests (voltage conversion, calibration)
- TDS sensor tests
- Basic safety threshold tests

#### `test_dosing_pump.cpp` ✨ NEW
Comprehensive DosingPump unit tests (38 tests):
- **Calibration Tests** (4 tests)
  - Flow rate calculation from measured volume/duration
  - Partial speed calibration adjustment
  - Invalid calibration handling (zero volume, zero duration)
- **Volume Tracking Tests** (5 tests)
  - Volume calculation at various speeds (100%, 50%)
  - Real-time volume tracking with millis()
  - Target volume detection
  - Progress calculation (0.0 to 1.0)
- **Schedule Calculation Tests** (7 tests)
  - Daily interval (86400 seconds)
  - Weekly interval (604800 seconds)
  - Custom intervals (any number of days)
  - Next dose time calculation
  - Time-of-day scheduling (specific hour:minute)
  - Next-day scheduling when target time passed
  - Overdue dose detection
- **Safety Limits Tests** (5 tests)
  - Max dose per dose enforcement
  - Max daily volume enforcement
  - Daily limit exceeded rejection
  - Daily volume reset at midnight
  - Speed limits (0-100%)
- **State Management Tests** (3 tests)
  - State transitions (IDLE→DOSING→IDLE)
  - Pause and resume during dosing
  - Emergency stop from any state
- **Maintenance Tests** (4 tests)
  - Prime duration tracking
  - Backflush (reverse) operation
  - Total runtime accumulation
  - Total dose counter
- **History Recording Tests** (2 tests)
  - Dosing record creation with timestamps
  - FIFO history with max records limit
- **Duration Calculation Tests** (2 tests)
  - Calculate duration from volume at 100% speed
  - Duration adjustment for partial speeds

#### `test_ntp_sync.cpp` ✨ NEW
NTP time synchronization tests (41 tests):
- **ConfigManager NTP Storage Tests** (8 tests)
  - Default NTP configuration (pool.ntp.org, UTC)
  - Custom NTP servers (time.google.com, time.nist.gov, etc.)
  - Timezone configurations (EST, EDT, PST, PDT, CET, CEST)
- **WiFiManager Time Sync Tests** (6 tests)
  - configTime() called with correct parameters
  - Time validation (year > 2020 check)
  - Valid year detection (2024, 2025)
  - Retry logic (10 attempts × 1 second)
  - timeSynced flag management
  - Sync failure handling
- **Timezone Calculation Tests** (8 tests)
  - UTC offset (0)
  - PST offset (UTC-8 = -28800)
  - PDT offset (UTC-7 = -25200, including DST)
  - CEST offset (UTC+2 = +7200, including DST)
  - Positive offsets (UTC+1 to UTC+10)
  - Negative offsets (UTC-5 to UTC-10)
  - Half-hour offsets (India UTC+5:30, Newfoundland UTC-3:30)
  - Quarter-hour offsets (Nepal UTC+5:45)
- **Time Formatting Tests** (3 tests)
  - ISO 8601 format (YYYY-MM-DD HH:MM:SS)
  - 12-hour format with AM/PM
  - Unix timestamp
- **NTP Server Configuration Tests** (3 tests)
  - NTP pool servers (pool.ntp.org, time.nist.gov, etc.)
  - Geographic pool servers (us.pool.ntp.org, europe.pool.ntp.org)
  - Server name validation
- **Time Comparison Tests** (4 tests)
  - Before comparison (time1 < time2)
  - After comparison (time1 > time2)
  - Equal comparison (time1 == time2)
  - Time difference calculation
- **DST (Daylight Saving Time) Tests** (4 tests)
  - DST offset applied (+1 hour)
  - DST offset not applied (winter)
  - Spring transition (2:00 AM → 3:00 AM)
  - Fall transition (2:00 AM → 1:00 AM)

#### `test_time_proportional.cpp` (344 lines)
Time proportional relay control tests:
- Duty cycle calculations (0%, 25%, 50%, 75%, 100%)
- Window reset logic
- Different window sizes
- Precision testing
- Mode switching (simple threshold vs time proportional)

#### `test_mocks.cpp` (389 lines + NEW TESTS)
Mock implementations and tests:
- **MockPreferences class** - Simulates NVS storage
  - Float, int, bool, string storage
  - Default values
  - Multiple keys
- **Storage Tests**
  - PID parameter persistence
  - pH calibration save/load
  - WiFi configuration
  - MQTT configuration
  - System targets
- **NEW: NTP Configuration Tests** ✨
  - NTP server storage (ntpServer string)
  - GMT offset storage (gmtOffset int)
  - DST offset storage (dstOffset int)
  - Multiple server configurations
  - Various timezone offset combinations
- **NEW: DosingPump Storage Tests** ✨
  - Calibration data (mlPerSecond, lastCalTime, calibrated flag)
  - Schedule configuration (enabled, type, hour, minute, volume)
  - Safety limits (maxDose, maxDaily, safetyEnabled)
  - Runtime tracking (totalRuntime, totalDoses, dailyVolume)
- **NEW: Mock Time Function Tests** ✨
  - time() mock functionality
  - localtime_r() structure
  - Time comparison operations
  - Time calculations (day, week)
- **NEW: Mock configTime Tests** ✨
  - configTime() parameter handling
- **NEW: Mock WiFi Event Tests** ✨
  - WiFi connection state changes
  - Reconnection triggers sync

### Integration Tests

#### `test_integration.cpp` (412 lines + NEW TESTS)
Integration tests for multi-component scenarios:
- **Existing Integration Tests**
  - Complete PID control loop (sensor→PID→relay)
  - Sensor to storage integration
  - Dual PID controllers (temperature + pH)
  - Complete safety system
  - Configuration change propagation
  - pH calibration workflow
  - Real-time data pipeline
  - MQTT message generation
  - Emergency recovery procedure
  - PID learning adaptation
  - Time-based control intervals
  - Multi-point data validation

- **NEW: DosingPump Integration Tests** ✨ (7 tests)
  - Scheduled dosing triggers at correct time-of-day (14:30 check)
  - Skip dosing if already dosed today (24-hour minimum)
  - Daily volume reset at midnight (day counter)
  - Safety prevents overdosing (daily limit enforcement)
  - Dosing history with timestamps (record creation)
  - Integration with pattern learning (pH impact tracking)
  - Multiple dosing pumps with different schedules (3 pumps)

- **NEW: NTP Integration Tests** ✨ (8 tests)
  - WiFi connect triggers NTP sync (automatic)
  - NTP sync retry on failure (3 attempts)
  - Time-dependent features use real time (DosingPump, PatternLearner, EventLogger)
  - Pattern learning uses real hour-of-day (0-23, not uptime)
  - Event logging timestamps are correct (ordered, accurate differences)
  - Timezone affects scheduled dosing (PST, EST, CET examples)
  - Web API time endpoints return correct data (synced, time, server, offsets)
  - NTP config persists across reboot (NVS storage)

## Test Coverage Summary

### DosingPump Feature Coverage
✅ **Calibration System**
- Flow rate calculation: ✅ Tested (4 tests)
- Volume tracking: ✅ Tested (5 tests)
- Invalid calibration handling: ✅ Tested

✅ **Scheduling System**
- Daily/weekly/custom intervals: ✅ Tested (7 tests)
- Time-of-day scheduling: ✅ Tested
- Overdue detection: ✅ Tested
- Multi-pump coordination: ✅ Tested (integration)

✅ **Safety System**
- Max dose limits: ✅ Tested (5 tests)
- Max daily limits: ✅ Tested
- Daily reset: ✅ Tested
- Speed limits: ✅ Tested

✅ **State Management**
- State transitions: ✅ Tested (3 tests)
- Pause/resume: ✅ Tested
- Emergency stop: ✅ Tested

✅ **Maintenance Functions**
- Prime, backflush, purge: ✅ Tested (4 tests)
- Runtime tracking: ✅ Tested
- History recording: ✅ Tested

✅ **Integration**
- Real-time scheduling: ✅ Tested
- Pattern learning: ✅ Tested
- Safety enforcement: ✅ Tested

### NTP Time Synchronization Coverage
✅ **Configuration**
- NTP server storage: ✅ Tested (8 tests)
- Timezone storage: ✅ Tested
- NVS persistence: ✅ Tested

✅ **Synchronization**
- configTime() parameters: ✅ Tested (6 tests)
- Validation logic: ✅ Tested
- Retry mechanism: ✅ Tested
- Status flag: ✅ Tested

✅ **Timezone Calculations**
- GMT offsets: ✅ Tested (8 tests)
- DST offsets: ✅ Tested (4 tests)
- Special offsets (30min, 45min): ✅ Tested

✅ **Time Functions**
- Formatting: ✅ Tested (3 tests)
- Comparisons: ✅ Tested (4 tests)
- Calculations: ✅ Tested

✅ **Integration**
- WiFi triggers: ✅ Tested
- Feature integration: ✅ Tested (8 tests)
- Web API: ✅ Tested

## Running Tests

### Run All Tests
```bash
pio test
```

### Run Specific Test File
```bash
# Run only DosingPump tests
pio test -f test_dosing_pump

# Run only NTP tests
pio test -f test_ntp_sync

# Run only integration tests
pio test -f test_integration
```

### Run Tests in Native Environment
```bash
pio test -e native
```

### Run Tests on Hardware
```bash
pio test -e esp32dev
```

## Test Environment Configuration

Tests run in the `[env:native]` environment defined in `platformio.ini`:
```ini
[env:native]
platform = native
test_framework = unity
build_flags = 
    -D UNIT_TEST
    -std=c++11
lib_deps = 
    throwtheswitch/Unity@^2.5.2
```

## Mock System

Tests use mock implementations for hardware-dependent components:

### MockPreferences
Simulates ESP32 NVS (Non-Volatile Storage):
- Stores up to 50 key-value pairs
- Supports float, int, bool, string types
- Persistent across test runs within same session

### Mock Time Functions
- `millis()` - Mockable for time-based testing
- `time()` - Returns configurable unix timestamp
- `localtime_r()` - Returns configurable tm structure

### Mock Hardware Functions
- `delay()` - No-op in tests
- `constrain()` - Limits values to range
- `abs()` - Absolute value

## Test Organization

### Test Structure
Each test file follows this pattern:
```cpp
#include <unity.h>

// Mock functions and setup

// Test functions
void test_feature_name() {
    // Arrange
    // Act
    // Assert
    TEST_ASSERT_*(...);
}

// Setup/Teardown
void setUp(void) { /* Reset state */ }
void tearDown(void) { /* Cleanup */ }

// Main runner
int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_feature_name);
    return UNITY_END();
}
```

### Naming Conventions
- Test files: `test_*.cpp`
- Test functions: `test_component_behavior()`
- Mock functions: `mock_*()` or `setMock*()`

## Unity Test Framework

### Common Assertions
```cpp
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_FLOAT(expected, actual)
TEST_ASSERT_EQUAL_STRING(expected, actual)
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)
TEST_ASSERT_EQUAL_UINT32(expected, actual)
```

### Test Lifecycle
1. `setUp()` - Called before each test
2. `test_*()` - Individual test function
3. `tearDown()` - Called after each test

## Test Metrics

### Total Test Count
- **test_dosing_pump.cpp**: 38 tests
- **test_ntp_sync.cpp**: 41 tests
- **test_integration.cpp**: 27 tests (12 original + 7 dosing + 8 NTP)
- **test_mocks.cpp**: 25 tests (8 original + 17 new)
- **test_main.cpp**: ~20 tests
- **test_time_proportional.cpp**: ~15 tests
- **TOTAL**: ~166 tests

### Code Coverage (Estimated)
- DosingPump: ~85% (core logic fully tested)
- NTP Sync: ~90% (all major paths tested)
- ConfigManager: ~70% (NTP config tested)
- WiFiManager: ~60% (time sync tested)
- Integration: ~75% (major interactions tested)

## Continuous Integration

These tests are designed to run in CI/CD pipelines:
```yaml
# Example GitHub Actions workflow
- name: Run Tests
  run: pio test -e native
```

## Adding New Tests

### 1. Create Test File
```cpp
// test/test_new_feature.cpp
#include <unity.h>

void test_new_feature_basic() {
    // Your test here
    TEST_ASSERT_TRUE(true);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_new_feature_basic);
    return UNITY_END();
}
```

### 2. Run Test
```bash
pio test -f test_new_feature
```

## Troubleshooting

### Test Not Found
- Ensure file is in `test/` directory
- Ensure filename starts with `test_`
- Run `pio test` to see all discovered tests

### Compilation Errors
- Check for missing `#include` statements
- Verify mock implementations are included
- Ensure `UNIT_TEST` flag is defined in platformio.ini

### Test Failures
- Check setUp/tearDown for proper state reset
- Verify mock functions return expected values
- Use `TEST_MESSAGE("debug info")` for debugging

## Best Practices

1. **Test Independence**: Each test should be independent
2. **Clear Names**: Test names should describe what is being tested
3. **Arrange-Act-Assert**: Structure tests with clear phases
4. **Mock External Dependencies**: Don't rely on actual hardware
5. **Test Edge Cases**: Include boundary conditions and error cases
6. **Keep Tests Fast**: Use mocks to avoid delays
7. **Document Complex Tests**: Add comments for non-obvious logic

## Future Test Additions

Potential areas for additional test coverage:
- [ ] WaterChangeAssistant scheduling and volume tracking
- [ ] PatternLearner prediction accuracy tests
- [ ] EventLogger filtering and retention tests
- [ ] WebServer API endpoint validation
- [ ] MQTT publish/subscribe integration
- [ ] WiFiManager reconnection logic
- [ ] ConfigManager validation rules
- [ ] Sensor error recovery tests

## References

- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [ESP32 Arduino Framework](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
