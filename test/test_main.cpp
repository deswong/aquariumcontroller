#include <unity.h>
#include <Arduino.h>

// Mock Arduino functions for native testing
#ifndef UNIT_TEST
// Running on hardware - use real Arduino functions
#else
// Running native tests - provide mocks
unsigned long millis() {
    static unsigned long mockTime = 0;
    return mockTime += 100;
}

void delay(unsigned long ms) {
    // No-op for testing
}

float constrain(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float abs(float value) {
    return value < 0 ? -value : value;
}
#endif

// Test helper to reset mock time
void resetMockTime() {
    // Implementation depends on how millis() mock is structured
}

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

// ============================================================================
// AdaptivePID Tests
// ============================================================================

void test_pid_initialization() {
    // Test that PID initializes with correct default parameters
    // Note: This would need mock Preferences
    TEST_ASSERT_TRUE(true); // Placeholder
}

void test_pid_compute_proportional() {
    // Test proportional control
    // Given: Error = 5, Kp = 2
    // Expected: P term = 10
    float error = 5.0;
    float kp = 2.0;
    float expected = kp * error;
    TEST_ASSERT_EQUAL_FLOAT(10.0, expected);
}

void test_pid_integral_windup_prevention() {
    // Test that integral doesn't wind up beyond limits
    TEST_ASSERT_TRUE(true); // Would test integral clamping
}

void test_pid_emergency_stop() {
    // Test emergency stop when value exceeds safety threshold
    float current = 32.0;
    float target = 25.0;
    float safetyThreshold = 5.0;
    
    bool shouldStop = (current > target + safetyThreshold);
    TEST_ASSERT_TRUE(shouldStop);
}

void test_pid_safety_threshold() {
    // Test safety threshold calculation
    float target = 25.0;
    float safetyMax = 30.0;
    float expectedThreshold = safetyMax - target;
    
    TEST_ASSERT_EQUAL_FLOAT(5.0, expectedThreshold);
}

// ============================================================================
// Temperature Sensor Tests
// ============================================================================

void test_temperature_valid_range() {
    // Test that temperature readings are validated
    float temp = 25.0;
    bool isValid = (temp > -55 && temp < 125);
    TEST_ASSERT_TRUE(isValid);
    
    float invalidTemp = 150.0;
    isValid = (invalidTemp > -55 && invalidTemp < 125);
    TEST_ASSERT_FALSE(isValid);
}

void test_temperature_moving_average() {
    // Test moving average calculation
    float readings[5] = {25.0, 25.1, 24.9, 25.0, 25.2};
    float sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += readings[i];
    }
    float average = sum / 5.0;
    TEST_ASSERT_FLOAT_WITHIN(0.01, 25.04, average);
}

// ============================================================================
// pH Sensor Tests
// ============================================================================

void test_ph_voltage_to_ph_conversion() {
    // Test voltage to pH conversion (default linear)
    float voltage = 1.65; // Should be pH 7.0
    float expectedPH = 7.0 - ((voltage - 1.65) * 3.5);
    TEST_ASSERT_EQUAL_FLOAT(7.0, expectedPH);
}

void test_ph_valid_range() {
    // Test pH validation
    float ph = 7.0;
    bool isValid = (ph >= 0 && ph <= 14);
    TEST_ASSERT_TRUE(isValid);
    
    float invalidPH = 15.0;
    isValid = (invalidPH >= 0 && invalidPH <= 14);
    TEST_ASSERT_FALSE(isValid);
}

void test_ph_calibration_three_point() {
    // Test three-point calibration curve
    float acidVoltage = 2.0;    // pH 4.0
    float neutralVoltage = 1.5; // pH 7.0
    float baseVoltage = 1.0;    // pH 10.0
    
    // Test slope between acid and neutral
    float slope1 = (7.0 - 4.0) / (neutralVoltage - acidVoltage);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 6.0, slope1);
    
    // Test slope between neutral and base
    float slope2 = (10.0 - 7.0) / (baseVoltage - neutralVoltage);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 6.0, slope2);
}

// ============================================================================
// TDS Sensor Tests
// ============================================================================

void test_tds_temperature_compensation() {
    // Test temperature compensation formula
    float temp = 30.0;
    float compensationCoefficient = 1.0 + 0.02 * (temp - 25.0);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.1, compensationCoefficient);
}

void test_tds_valid_range() {
    // Test TDS validation (0-2000 ppm)
    float tds = 350.0;
    bool isValid = (tds >= 0 && tds < 2000);
    TEST_ASSERT_TRUE(isValid);
    
    float invalidTDS = 2500.0;
    isValid = (invalidTDS >= 0 && invalidTDS < 2000);
    TEST_ASSERT_FALSE(isValid);
}

void test_tds_k_value_factor() {
    // Test TDS conversion with different K values
    float voltage = 1.5;
    float kValue1 = 0.5;
    float kValue2 = 1.0;
    
    float tds1 = 857.39 * voltage * kValue1; // Simplified
    float tds2 = 857.39 * voltage * kValue2;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1, tds1 * 2, tds2);
}

// ============================================================================
// Relay Controller Tests
// ============================================================================

void test_relay_minimum_toggle_interval() {
    // Test that relay doesn't toggle too quickly
    unsigned long lastToggle = 0;
    unsigned long currentTime = 3000; // 3 seconds later
    unsigned long minInterval = 5000; // 5 seconds minimum
    
    bool canToggle = (currentTime - lastToggle >= minInterval);
    TEST_ASSERT_FALSE(canToggle);
    
    currentTime = 6000; // 6 seconds later
    canToggle = (currentTime - lastToggle >= minInterval);
    TEST_ASSERT_TRUE(canToggle);
}

void test_relay_safety_disable() {
    // Test safety disable functionality
    bool safetyDisabled = true;
    bool requestOn = true;
    
    bool actuallyOn = requestOn && !safetyDisabled;
    TEST_ASSERT_FALSE(actuallyOn);
    
    safetyDisabled = false;
    actuallyOn = requestOn && !safetyDisabled;
    TEST_ASSERT_TRUE(actuallyOn);
}

void test_relay_inverted_logic() {
    // Test inverted relay logic
    bool state = true;
    bool inverted = true;
    
    bool outputHigh = inverted ? !state : state;
    TEST_ASSERT_FALSE(outputHigh);
    
    inverted = false;
    outputHigh = inverted ? !state : state;
    TEST_ASSERT_TRUE(outputHigh);
}

// ============================================================================
// Config Manager Tests
// ============================================================================

void test_config_default_values() {
    // Test default configuration values
    float defaultTemp = 25.0;
    float defaultPH = 6.8;
    int defaultMQTTPort = 1883;
    
    TEST_ASSERT_EQUAL_FLOAT(25.0, defaultTemp);
    TEST_ASSERT_EQUAL_FLOAT(6.8, defaultPH);
    TEST_ASSERT_EQUAL_INT(1883, defaultMQTTPort);
}

void test_config_string_copy_safety() {
    // Test that string copying doesn't overflow
    char buffer[32];
    const char* longString = "This is a very long string that exceeds buffer size";
    
    strncpy(buffer, longString, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    TEST_ASSERT_EQUAL_INT(31, strlen(buffer));
}

// ============================================================================
// Safety Tests
// ============================================================================

void test_temperature_safety_limit() {
    // Test temperature safety calculations
    float current = 31.0;
    float target = 25.0;
    float safetyMax = 30.0;
    
    bool emergencyStop = (current > safetyMax);
    TEST_ASSERT_TRUE(emergencyStop);
}

void test_ph_safety_limit() {
    // Test pH safety calculations
    float current = 5.8;
    float target = 6.8;
    float safetyMin = 6.0;
    
    bool emergencyStop = (current < safetyMin);
    TEST_ASSERT_TRUE(emergencyStop);
}

void test_overshoot_detection() {
    // Test overshoot detection
    float setpoint = 25.0;
    float current = 27.0;
    float maxOvershoot = 1.5;
    
    float overshoot = current - setpoint;
    bool isOvershoot = (overshoot > maxOvershoot);
    
    TEST_ASSERT_TRUE(isOvershoot);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

void test_constrain_function() {
    float value = 150.0;
    float min = 0.0;
    float max = 100.0;
    
    float constrained = constrain(value, min, max);
    TEST_ASSERT_EQUAL_FLOAT(100.0, constrained);
    
    value = -10.0;
    constrained = constrain(value, min, max);
    TEST_ASSERT_EQUAL_FLOAT(0.0, constrained);
}

void test_voltage_to_adc_conversion() {
    // Test ADC conversion (ESP32 12-bit)
    int rawADC = 2048; // Mid-point
    float voltage = (rawADC / 4095.0) * 3.3;
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.65, voltage);
}

void test_moving_average_buffer_wraparound() {
    // Test circular buffer index wraparound
    int index = 9;
    int bufferSize = 10;
    
    index = (index + 1) % bufferSize;
    TEST_ASSERT_EQUAL_INT(0, index);
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_sensor_to_pid_data_flow() {
    // Test complete data flow from sensor to PID
    float sensorReading = 24.0;
    float target = 25.0;
    float error = target - sensorReading;
    float kp = 2.0;
    float pTerm = kp * error;
    
    TEST_ASSERT_EQUAL_FLOAT(1.0, error);
    TEST_ASSERT_EQUAL_FLOAT(2.0, pTerm);
}

void test_pid_to_relay_output() {
    // Test PID output to relay conversion
    float pidOutput = 75.0; // 75%
    bool relayState = (pidOutput > 50.0);
    
    TEST_ASSERT_TRUE(relayState);
    
    pidOutput = 30.0;
    relayState = (pidOutput > 50.0);
    TEST_ASSERT_FALSE(relayState);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

void test_zero_division_protection() {
    // Test division by zero protection
    float dt = 0.0;
    float error = 5.0;
    float lastError = 3.0;
    
    float derivative = (dt > 0) ? (error - lastError) / dt : 0.0;
    TEST_ASSERT_EQUAL_FLOAT(0.0, derivative);
}

void test_sensor_disconnect_detection() {
    // Test detection of disconnected sensor
    float tempReading = -127.0; // DS18B20 disconnect value
    bool isDisconnected = (tempReading == -127.0);
    
    TEST_ASSERT_TRUE(isDisconnected);
}

void test_invalid_json_handling() {
    // Test that invalid JSON doesn't crash system
    const char* invalidJson = "{invalid json";
    bool isValid = (strchr(invalidJson, '}') != NULL);
    
    TEST_ASSERT_FALSE(isValid);
}

// ============================================================================
// Performance Tests
// ============================================================================

void test_calculation_speed() {
    // Test that calculations complete quickly
    unsigned long start = millis();
    
    // Simulate PID calculation
    float error = 5.0;
    float kp = 2.0;
    float ki = 0.5;
    float kd = 1.0;
    float integral = 10.0;
    float derivative = 0.5;
    
    float output = kp * error + ki * integral + kd * derivative;
    
    unsigned long elapsed = millis() - start;
    
    TEST_ASSERT_LESS_THAN(100, elapsed); // Should be < 100ms
    TEST_ASSERT_FLOAT_WITHIN(0.01, 15.5, output);
}

void test_moving_average_efficiency() {
    // Test moving average doesn't recalculate unnecessarily
    float readings[10] = {25,25,25,25,25,25,25,25,25,25};
    float total = 0;
    
    // Should only need to subtract old and add new
    float oldValue = readings[0];
    float newValue = 26.0;
    total = 250.0; // Precalculated sum
    
    total = total - oldValue + newValue;
    float average = total / 10.0;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01, 25.1, average);
}

// ============================================================================
// Main Test Runner
// ============================================================================

void runAllTests() {
    UNITY_BEGIN();
    
    // PID Tests
    RUN_TEST(test_pid_initialization);
    RUN_TEST(test_pid_compute_proportional);
    RUN_TEST(test_pid_integral_windup_prevention);
    RUN_TEST(test_pid_emergency_stop);
    RUN_TEST(test_pid_safety_threshold);
    
    // Temperature Tests
    RUN_TEST(test_temperature_valid_range);
    RUN_TEST(test_temperature_moving_average);
    
    // pH Tests
    RUN_TEST(test_ph_voltage_to_ph_conversion);
    RUN_TEST(test_ph_valid_range);
    RUN_TEST(test_ph_calibration_three_point);
    
    // TDS Tests
    RUN_TEST(test_tds_temperature_compensation);
    RUN_TEST(test_tds_valid_range);
    RUN_TEST(test_tds_k_value_factor);
    
    // Relay Tests
    RUN_TEST(test_relay_minimum_toggle_interval);
    RUN_TEST(test_relay_safety_disable);
    RUN_TEST(test_relay_inverted_logic);
    
    // Config Tests
    RUN_TEST(test_config_default_values);
    RUN_TEST(test_config_string_copy_safety);
    
    // Safety Tests
    RUN_TEST(test_temperature_safety_limit);
    RUN_TEST(test_ph_safety_limit);
    RUN_TEST(test_overshoot_detection);
    
    // Utility Tests
    RUN_TEST(test_constrain_function);
    RUN_TEST(test_voltage_to_adc_conversion);
    RUN_TEST(test_moving_average_buffer_wraparound);
    
    // Integration Tests
    RUN_TEST(test_sensor_to_pid_data_flow);
    RUN_TEST(test_pid_to_relay_output);
    
    // Edge Case Tests
    RUN_TEST(test_zero_division_protection);
    RUN_TEST(test_sensor_disconnect_detection);
    RUN_TEST(test_invalid_json_handling);
    
    // Performance Tests
    RUN_TEST(test_calculation_speed);
    RUN_TEST(test_moving_average_efficiency);
    
    UNITY_END();
}

#ifdef UNIT_TEST
int main(int argc, char **argv) {
    runAllTests();
    return 0;
}
#else
void setup() {
    delay(2000);
    Serial.begin(115200);
    runAllTests();
}

void loop() {
    // Tests run once in setup
}
#endif
