# Testing Guide - ESP32 Aquarium Controller

This document describes the comprehensive test suite for the aquarium controller project.

## Test Structure

The test suite is organized into three main categories:

```
test/
├── test_main.cpp          # Unit tests for individual components
├── test_mocks.cpp         # Tests using mock objects for hardware
└── test_integration.cpp   # Integration tests for complete workflows
```

## Running Tests

### Running All Tests on Hardware (ESP32)

```bash
# Build and upload tests to ESP32
pio test -e esp32dev

# Or upload and monitor
pio test -e esp32dev --verbose
```

### Running Tests Natively (on your computer)

```bash
# Run tests on native platform (faster, no hardware needed)
pio test -e native

# Run with verbose output
pio test -e native --verbose
```

### Running Specific Test Files

```bash
# Run only unit tests
pio test -e native -f test_main

# Run only integration tests
pio test -e native -f test_integration

# Run only mock tests
pio test -e native -f test_mocks
```

## Test Categories

### 1. Unit Tests (`test_main.cpp`)

Tests individual functions and components in isolation.

#### PID Controller Tests
- ✓ Initialization with correct parameters
- ✓ Proportional term calculation
- ✓ Integral windup prevention
- ✓ Emergency stop trigger
- ✓ Safety threshold validation

#### Sensor Tests
- ✓ Temperature valid range checking
- ✓ Moving average filtering
- ✓ pH voltage to value conversion
- ✓ pH valid range validation
- ✓ Three-point pH calibration
- ✓ TDS temperature compensation
- ✓ TDS valid range checking

#### Relay Controller Tests
- ✓ Minimum toggle interval enforcement
- ✓ Safety disable functionality
- ✓ Inverted logic support

#### Configuration Tests
- ✓ Default value initialization
- ✓ String copy safety (buffer overflow prevention)

#### Safety Tests
- ✓ Temperature safety limit enforcement
- ✓ pH safety limit enforcement
- ✓ Overshoot detection

#### Utility Tests
- ✓ Constrain function
- ✓ Voltage to ADC conversion
- ✓ Moving average buffer wraparound

#### Edge Case Tests
- ✓ Zero division protection
- ✓ Sensor disconnect detection
- ✓ Invalid JSON handling

#### Performance Tests
- ✓ Calculation speed validation
- ✓ Moving average efficiency

**Total Unit Tests: 30+**

### 2. Mock Tests (`test_mocks.cpp`)

Tests components that depend on hardware using mock objects.

#### Mock Preferences (NVS Storage)
- ✓ Float storage and retrieval
- ✓ Default value returns
- ✓ Multiple key management
- ✓ Different data types (float, int, bool, string)

#### PID Storage Tests
- ✓ Save and load PID parameters
- ✓ Parameter persistence across "reboots"

#### pH Calibration Storage
- ✓ Save calibration data
- ✓ Load calibration data
- ✓ Reset calibration

#### Configuration Storage
- ✓ WiFi credentials save/load
- ✓ MQTT settings save/load
- ✓ Target values save/load

#### Data Structure Tests
- ✓ Sensor data structure validation
- ✓ Config structure defaults

**Total Mock Tests: 15+**

### 3. Integration Tests (`test_integration.cpp`)

Tests complete workflows involving multiple components.

#### Control Loop Integration
- ✓ Complete PID control loop (sensor → PID → relay)
- ✓ Dual PID controllers (temperature + CO2)
- ✓ Sensor to storage pipeline

#### Safety System Integration
- ✓ Complete safety system (multiple safety checks)
- ✓ Emergency stop propagation
- ✓ Emergency recovery procedure

#### Configuration Integration
- ✓ Config change propagation to all systems
- ✓ Target change affecting PID

#### Calibration Integration
- ✓ Complete pH calibration workflow
- ✓ Multi-step calibration process

#### Data Flow Integration
- ✓ Real-time data pipeline (sensor → filter → validate → store → transmit)
- ✓ MQTT message generation

#### Learning Integration
- ✓ PID adaptive learning over time
- ✓ Parameter adjustment based on performance

#### Time-based Integration
- ✓ Different intervals for different tasks
- ✓ Multi-point data validation

**Total Integration Tests: 12+**

## Test Coverage

### Components Tested
- ✅ AdaptivePID
- ✅ TemperatureSensor (logic)
- ✅ PHSensor (logic + calibration)
- ✅ TDSSensor (logic)
- ✅ RelayController
- ✅ ConfigManager
- ✅ Safety systems
- ✅ Data structures
- ✅ Storage (NVS)

### Not Directly Testable (Hardware-Dependent)
- ❌ WiFi connectivity (requires network)
- ❌ MQTT client (requires broker)
- ❌ Web server (requires network)
- ❌ OTA updates (requires network)
- ❌ Actual sensor readings (requires sensors)
- ❌ FreeRTOS tasks (requires ESP32)

These components should be tested manually or with hardware-in-the-loop testing.

## Understanding Test Results

### Success Output
```
test/test_main.cpp:42:test_pid_compute_proportional [PASSED]
test/test_main.cpp:58:test_temperature_valid_range [PASSED]
...
-----------------------
42 Tests 0 Failures 0 Ignored
OK
```

### Failure Output
```
test/test_main.cpp:42:test_pid_compute_proportional:FAIL: Expected 10.0 Was 9.5
test/test_main.cpp:58:test_temperature_valid_range [PASSED]
...
-----------------------
42 Tests 1 Failures 0 Ignored
FAIL
```

## Writing New Tests

### Test Template

```cpp
void test_my_new_feature() {
    // Arrange - Set up test data
    float input = 25.0;
    float expected = 50.0;
    
    // Act - Perform the operation
    float result = myFunction(input);
    
    // Assert - Verify the result
    TEST_ASSERT_EQUAL_FLOAT(expected, result);
}
```

### Common Assertions

```cpp
// Equality
TEST_ASSERT_EQUAL_FLOAT(expected, actual);
TEST_ASSERT_EQUAL_INT(expected, actual);
TEST_ASSERT_EQUAL_STRING(expected, actual);

// Comparisons
TEST_ASSERT_LESS_THAN(threshold, actual);
TEST_ASSERT_GREATER_THAN(threshold, actual);

// Within tolerance
TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual);

// Boolean
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);

// Null checks
TEST_ASSERT_NOT_NULL(pointer);
TEST_ASSERT_NULL(pointer);
```

### Adding Tests to Suite

1. Write test function in appropriate file:
   - `test_main.cpp` for unit tests
   - `test_mocks.cpp` for mock-based tests
   - `test_integration.cpp` for integration tests

2. Add to test runner:
```cpp
void runAllTests() {
    UNITY_BEGIN();
    
    // Add your test here
    RUN_TEST(test_my_new_feature);
    
    UNITY_END();
}
```

## Continuous Integration

### GitHub Actions (Example)

```yaml
name: Run Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-python@v2
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run Tests
        run: pio test -e native
```

## Manual Testing Checklist

Some features require manual testing with hardware:

### Hardware Tests

- [ ] Temperature sensor reads correctly
- [ ] pH sensor reads correctly
- [ ] TDS sensor reads correctly
- [ ] Heater relay turns on/off
- [ ] CO2 relay turns on/off
- [ ] WiFi connects successfully
- [ ] Web interface loads and displays data
- [ ] OTA update works
- [ ] MQTT publishes data
- [ ] Emergency stop button works
- [ ] pH calibration saves correctly
- [ ] Settings persist after reboot

### Safety Tests

- [ ] Emergency stop activates at temperature limit
- [ ] Emergency stop activates at pH limit
- [ ] Relays disable during emergency
- [ ] System recovers from emergency correctly
- [ ] Minimum toggle interval prevents relay chatter

### Integration Tests (Hardware)

- [ ] PID controls temperature to target
- [ ] PID controls pH to target
- [ ] Web interface updates in real-time
- [ ] Configuration changes apply correctly
- [ ] System runs stable for 24+ hours

## Debugging Failed Tests

### Common Issues

**Test fails on native but passes on hardware**
- Likely missing mock implementation
- Check for hardware-specific functions

**Test intermittently fails**
- Check for timing dependencies
- Look for uninitialized variables
- Verify test isolation (setUp/tearDown)

**All tests fail to compile**
- Check include paths
- Verify library dependencies
- Check for missing mock definitions

### Debug Techniques

1. **Add Serial Output**
```cpp
void test_example() {
    Serial.println("Debug: Starting test");
    float result = myFunction(input);
    Serial.printf("Debug: Result = %.2f\n", result);
    TEST_ASSERT_EQUAL_FLOAT(expected, result);
}
```

2. **Run Single Test**
```bash
pio test -e native -f test_main -v
```

3. **Check Test Logs**
```bash
# Full verbose output
pio test -e native --verbose > test_output.log
```

## Test Coverage Goals

- **Unit Tests**: >80% code coverage for business logic
- **Integration Tests**: Cover all major workflows
- **Edge Cases**: Test boundary conditions and error cases
- **Safety**: 100% coverage of safety-critical code

## Performance Benchmarks

Tests include performance validation:

- PID calculation: < 100ms
- Sensor reading: < 200ms
- Moving average: < 10ms
- Config save/load: < 500ms

## Best Practices

1. **Test Independence**: Each test should run independently
2. **Fast Tests**: Keep unit tests fast (< 1ms each)
3. **Descriptive Names**: Use clear test names that describe what's being tested
4. **Arrange-Act-Assert**: Follow AAA pattern
5. **One Assertion**: Prefer one logical assertion per test
6. **Mock External Dependencies**: Don't rely on network, hardware, etc.
7. **Test Edge Cases**: Include boundary values and error conditions

## Troubleshooting

### "Test framework not found"
```bash
pio lib install --global "Unity"
```

### "Cannot find test files"
- Ensure tests are in `test/` directory
- Check file names start with `test_`

### "Mock functions undefined"
- Add mock implementations in test file
- Check `UNIT_TEST` define is set

## Future Enhancements

- [ ] Add code coverage reporting
- [ ] Add performance profiling
- [ ] Add stress tests
- [ ] Add regression test suite
- [ ] Add automated hardware-in-the-loop tests
- [ ] Add memory leak detection

## Resources

- [Unity Testing Framework](https://github.com/ThrowTheSwitch/Unity)
- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/plus/unit-testing.html)
- [Test-Driven Development](https://en.wikipedia.org/wiki/Test-driven_development)

---

**Happy Testing! 🧪**
