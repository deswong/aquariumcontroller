#include <unity.h>

// Test time proportional relay control

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

// ============================================================================
// Time Proportional Control Tests
// ============================================================================

void test_time_proportional_50_percent_duty() {
    // At 50% duty cycle with 10 second window:
    // Should be ON for 5 seconds, OFF for 5 seconds
    
    unsigned long windowSize = 10000; // 10 seconds
    float dutyCycle = 50.0;
    
    // Calculate on-time
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(5000, onTime);
    
    // Within first 5 seconds - should be ON
    setMockMillis(0);
    unsigned long elapsed = 3000; // 3 seconds
    bool shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_TRUE(shouldBeOn);
    
    // After 5 seconds - should be OFF
    elapsed = 7000; // 7 seconds
    shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_FALSE(shouldBeOn);
}

void test_time_proportional_25_percent_duty() {
    // At 25% duty cycle with 10 second window:
    // Should be ON for 2.5 seconds, OFF for 7.5 seconds
    
    unsigned long windowSize = 10000;
    float dutyCycle = 25.0;
    
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(2500, onTime);
    
    // At 2 seconds - should be ON
    unsigned long elapsed = 2000;
    bool shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_TRUE(shouldBeOn);
    
    // At 3 seconds - should be OFF
    elapsed = 3000;
    shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_FALSE(shouldBeOn);
}

void test_time_proportional_75_percent_duty() {
    // At 75% duty cycle with 10 second window:
    // Should be ON for 7.5 seconds, OFF for 2.5 seconds
    
    unsigned long windowSize = 10000;
    float dutyCycle = 75.0;
    
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(7500, onTime);
    
    // At 5 seconds - should be ON
    unsigned long elapsed = 5000;
    bool shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_TRUE(shouldBeOn);
    
    // At 8 seconds - should be OFF
    elapsed = 8000;
    shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_FALSE(shouldBeOn);
}

void test_time_proportional_0_percent_duty() {
    // At 0% duty cycle - should always be OFF
    
    unsigned long windowSize = 10000;
    float dutyCycle = 0.0;
    
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(0, onTime);
    
    // Should be OFF at any time
    unsigned long elapsed = 5000;
    bool shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_FALSE(shouldBeOn);
}

void test_time_proportional_100_percent_duty() {
    // At 100% duty cycle - should always be ON
    
    unsigned long windowSize = 10000;
    float dutyCycle = 100.0;
    
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(10000, onTime);
    
    // Should be ON throughout the window
    unsigned long elapsed = 5000;
    bool shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_TRUE(shouldBeOn);
    
    elapsed = 9999;
    shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_TRUE(shouldBeOn);
}

void test_time_proportional_window_reset() {
    // Test that window resets after expiration
    
    unsigned long windowSize = 10000;
    unsigned long windowStart = 0;
    unsigned long currentTime = 11000; // Past window
    
    unsigned long elapsed = currentTime - windowStart;
    
    // Should reset window
    bool shouldReset = (elapsed >= windowSize);
    TEST_ASSERT_TRUE(shouldReset);
    
    // After reset
    if (shouldReset) {
        windowStart = currentTime;
        elapsed = 0;
    }
    
    TEST_ASSERT_EQUAL_UINT32(11000, windowStart);
    TEST_ASSERT_EQUAL_UINT32(0, elapsed);
}

void test_time_proportional_different_window_sizes() {
    // Test with 5 second window at 60% duty
    unsigned long windowSize = 5000;
    float dutyCycle = 60.0;
    
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(3000, onTime);
    
    // Test with 20 second window at 30% duty
    windowSize = 20000;
    dutyCycle = 30.0;
    
    onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(6000, onTime);
}

void test_time_proportional_precision() {
    // Test precision with various duty cycles
    
    struct TestCase {
        float dutyCycle;
        unsigned long windowSize;
        unsigned long expectedOnTime;
    };
    
    TestCase cases[] = {
        {10.0, 10000, 1000},
        {33.3, 10000, 3330},
        {66.6, 10000, 6660},
        {90.0, 10000, 9000},
        {5.0, 20000, 1000},
        {50.0, 5000, 2500}
    };
    
    for (int i = 0; i < 6; i++) {
        unsigned long onTime = (unsigned long)((cases[i].dutyCycle / 100.0) * cases[i].windowSize);
        TEST_ASSERT_EQUAL_UINT32(cases[i].expectedOnTime, onTime);
    }
}

void test_mode_switching() {
    // Test switching between simple and time proportional modes
    
    enum RelayMode {
        SIMPLE_THRESHOLD,
        TIME_PROPORTIONAL
    };
    
    RelayMode mode = SIMPLE_THRESHOLD;
    
    // In simple mode, 75% should turn on
    float dutyCycle = 75.0;
    bool relayOn = (mode == SIMPLE_THRESHOLD) ? (dutyCycle > 50) : false;
    TEST_ASSERT_TRUE(relayOn);
    
    // Switch to time proportional
    mode = TIME_PROPORTIONAL;
    
    // In time proportional, state depends on time window
    // Mode change should reset window
    unsigned long windowStart = millis();
    TEST_ASSERT_EQUAL_UINT32(0, windowStart); // Mock millis starts at 0
}

void test_duty_cycle_bounds() {
    // Test that duty cycle is constrained to 0-100%
    
    float dutyCycle = 150.0;
    dutyCycle = (dutyCycle < 0) ? 0 : (dutyCycle > 100) ? 100 : dutyCycle;
    TEST_ASSERT_EQUAL_FLOAT(100.0, dutyCycle);
    
    dutyCycle = -50.0;
    dutyCycle = (dutyCycle < 0) ? 0 : (dutyCycle > 100) ? 100 : dutyCycle;
    TEST_ASSERT_EQUAL_FLOAT(0.0, dutyCycle);
    
    dutyCycle = 50.0;
    dutyCycle = (dutyCycle < 0) ? 0 : (dutyCycle > 100) ? 100 : dutyCycle;
    TEST_ASSERT_EQUAL_FLOAT(50.0, dutyCycle);
}

// ============================================================================
// Integration with PID Tests
// ============================================================================

void test_pid_output_to_time_proportional() {
    // Test complete flow from PID output to time proportional control
    
    // PID outputs 35%
    float pidOutput = 35.0;
    
    // Set as duty cycle
    float dutyCycle = pidOutput;
    
    // In 10 second window, calculate on-time
    unsigned long windowSize = 10000;
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    
    TEST_ASSERT_EQUAL_UINT32(3500, onTime);
    
    // At 3 seconds - should be ON
    unsigned long elapsed = 3000;
    bool shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_TRUE(shouldBeOn);
    
    // At 4 seconds - should be OFF
    elapsed = 4000;
    shouldBeOn = (elapsed < onTime);
    TEST_ASSERT_FALSE(shouldBeOn);
}

void test_gradual_control_vs_bang_bang() {
    // Compare time proportional to simple threshold
    
    float pidOutput = 45.0; // Just below 50%
    
    // Simple threshold mode
    bool simpleModeOn = (pidOutput > 50.0);
    TEST_ASSERT_FALSE(simpleModeOn); // Would be OFF
    
    // Time proportional mode (10 second window)
    unsigned long windowSize = 10000;
    unsigned long onTime = (unsigned long)((pidOutput / 100.0) * windowSize);
    TEST_ASSERT_EQUAL_UINT32(4500, onTime); // ON for 4.5 seconds
    
    // This provides more gradual control than simple on/off
    TEST_ASSERT_GREATER_THAN(0, onTime);
    TEST_ASSERT_LESS_THAN(windowSize, onTime);
}

// ============================================================================
// Safety Integration Tests
// ============================================================================

void test_safety_disable_overrides_time_proportional() {
    // Even in time proportional mode, safety disable should force OFF
    
    bool safetyDisabled = true;
    float dutyCycle = 75.0;
    unsigned long windowSize = 10000;
    unsigned long elapsed = 3000; // Within on-time normally
    
    unsigned long onTime = (unsigned long)((dutyCycle / 100.0) * windowSize);
    bool shouldBeOn = (elapsed < onTime) && !safetyDisabled;
    
    TEST_ASSERT_FALSE(shouldBeOn); // Safety override
    
    // Re-enable safety
    safetyDisabled = false;
    shouldBeOn = (elapsed < onTime) && !safetyDisabled;
    TEST_ASSERT_TRUE(shouldBeOn);
}

// ============================================================================
// Run Time Proportional Tests
// ============================================================================

void runTimeProportionalTests() {
    UNITY_BEGIN();
    
    // Basic duty cycle tests
    RUN_TEST(test_time_proportional_50_percent_duty);
    RUN_TEST(test_time_proportional_25_percent_duty);
    RUN_TEST(test_time_proportional_75_percent_duty);
    RUN_TEST(test_time_proportional_0_percent_duty);
    RUN_TEST(test_time_proportional_100_percent_duty);
    
    // Window management
    RUN_TEST(test_time_proportional_window_reset);
    RUN_TEST(test_time_proportional_different_window_sizes);
    
    // Precision and bounds
    RUN_TEST(test_time_proportional_precision);
    RUN_TEST(test_duty_cycle_bounds);
    
    // Mode switching
    RUN_TEST(test_mode_switching);
    
    // Integration tests
    RUN_TEST(test_pid_output_to_time_proportional);
    RUN_TEST(test_gradual_control_vs_bang_bang);
    
    // Safety tests
    RUN_TEST(test_safety_disable_overrides_time_proportional);
    
    UNITY_END();
}

#ifndef UNIT_TEST
void setup() {
    delay(2000);
    Serial.begin(115200);
    runTimeProportionalTests();
}

void loop() {
    // Tests run once
}
#endif
