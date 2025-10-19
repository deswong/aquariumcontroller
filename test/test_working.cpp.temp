#include <unity.h>
#include "test_common.h"

// PID calculation
float calculatePIDOutput(float setpoint, float current, float kp, float ki, float kd, 
                        float previous_error, float integral, float dt) {
    float error = setpoint - current;
    integral += error * dt;
    float derivative = (error - previous_error) / dt;
    return (kp * error) + (ki * integral) + (kd * derivative);
}

// Temperature safety check
bool isTemperatureSafe(float temp, float min_safe, float max_safe) {
    return (temp >= min_safe && temp <= max_safe);
}

// pH calibration slope calculation
float calculatePHSlope(float voltage1, float ph1, float voltage2, float ph2) {
    if (fabs(voltage2 - voltage1) < 0.001) return 0.0; // Avoid division by zero
    return (ph2 - ph1) / (voltage2 - voltage1);
}

// Test functions
void test_dosing_pump_calibration() {
    // Test: Flow rate calculation at 100% speed
    float flow_rate = calculateFlowRate(100.0, 50.0, 100.0);
    TEST_ASSERT_EQUAL_FLOAT(2.0, flow_rate);
    
    // Test: Flow rate calculation at 50% speed
    flow_rate = calculateFlowRate(50.0, 50.0, 50.0);
    TEST_ASSERT_EQUAL_FLOAT(2.0, flow_rate);
    
    // Test: Invalid input handling
    flow_rate = calculateFlowRate(100.0, 0.0, 100.0); // Zero duration
    TEST_ASSERT_EQUAL_FLOAT(0.0, flow_rate);
}

void test_pid_controller() {
    // Test: Basic proportional control
    float output = calculatePIDOutput(25.0, 23.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    TEST_ASSERT_EQUAL_FLOAT(4.0, output); // 2.0 * (25 - 23) = 4.0
    
    // Test: PID with all terms
    output = calculatePIDOutput(25.0, 24.0, 1.0, 0.1, 0.05, 0.5, 2.0, 1.0);
    // P = 1.0 * 1.0 = 1.0
    // I = 0.1 * (2.0 + 1.0 * 1.0) = 0.3
    // D = 0.05 * (1.0 - 0.5) / 1.0 = 0.025
    // Total = 1.0 + 0.3 + 0.025 = 1.325
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.325, output);
}

void test_temperature_safety() {
    // Test: Normal temperature range
    TEST_ASSERT_TRUE(isTemperatureSafe(25.0, 20.0, 30.0));
    
    // Test: Temperature too low
    TEST_ASSERT_FALSE(isTemperatureSafe(18.0, 20.0, 30.0));
    
    // Test: Temperature too high
    TEST_ASSERT_FALSE(isTemperatureSafe(32.0, 20.0, 30.0));
    
    // Test: Edge cases
    TEST_ASSERT_TRUE(isTemperatureSafe(20.0, 20.0, 30.0)); // Min boundary
    TEST_ASSERT_TRUE(isTemperatureSafe(30.0, 20.0, 30.0)); // Max boundary
}

void test_ph_calibration() {
    // Test: pH slope calculation between two calibration points
    // pH 7.0 at 2.5V, pH 4.0 at 2.8V
    float slope = calculatePHSlope(2.5, 7.0, 2.8, 4.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001, -10.0, slope); // (4-7)/(2.8-2.5) = -10
    
    // Test: Reversed order
    slope = calculatePHSlope(2.8, 4.0, 2.5, 7.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001, -10.0, slope); // (7-4)/(2.5-2.8) = -10
    
    // Test: Invalid calibration (same voltage)
    slope = calculatePHSlope(2.5, 7.0, 2.5, 4.0);
    TEST_ASSERT_EQUAL_FLOAT(0.0, slope);
}

void test_mock_time_functions() {
    // Test: Mock millis function
    setMockMillis(1000);
    TEST_ASSERT_EQUAL(1000, millis());
    
    setMockMillis(5000);
    TEST_ASSERT_EQUAL(5000, millis());
}

void test_aquarium_calculations() {
    // Test: Water volume calculation for rectangular tank
    float length_cm = 100.0;
    float width_cm = 50.0;
    float height_cm = 40.0;
    float volume_liters = (length_cm * width_cm * height_cm) / 1000.0;
    TEST_ASSERT_EQUAL_FLOAT(200.0, volume_liters);
    
    // Test: Water change percentage
    float tank_volume = 200.0;
    float change_volume = 50.0;
    float change_percent = (change_volume / tank_volume) * 100.0;
    TEST_ASSERT_EQUAL_FLOAT(25.0, change_percent);
}

void setUp(void) {
    commonSetUp();
}

void tearDown(void) {
    commonTearDown();
}

// Arduino setup function for ESP32 testing
void setup(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_dosing_pump_calibration);
    RUN_TEST(test_pid_controller);
    RUN_TEST(test_temperature_safety);
    RUN_TEST(test_ph_calibration);
    RUN_TEST(test_mock_time_functions);
    RUN_TEST(test_aquarium_calculations);
    
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
    
    RUN_TEST(test_dosing_pump_calibration);
    RUN_TEST(test_pid_controller);
    RUN_TEST(test_temperature_safety);
    RUN_TEST(test_ph_calibration);
    RUN_TEST(test_mock_time_functions);
    RUN_TEST(test_aquarium_calculations);
    
    return UNITY_END();
}
#endif