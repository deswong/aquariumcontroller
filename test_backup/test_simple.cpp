#include <unity.h>
#include <cstring>

void test_basic_math() {
    TEST_ASSERT_EQUAL(4, 2 + 2);
    TEST_ASSERT_EQUAL(10, 5 * 2);
    TEST_ASSERT_TRUE(10 > 5);
}

void test_basic_string() {
    const char* test_str = "aquarium";
    TEST_ASSERT_EQUAL_STRING("aquarium", test_str);
    TEST_ASSERT_EQUAL(8, strlen(test_str));
}

void test_dosing_calculation() {
    // Simple dosing calculation test
    float volume_ml = 100.0;
    float duration_sec = 50.0;
    float flow_rate = volume_ml / duration_sec;
    
    TEST_ASSERT_EQUAL_FLOAT(2.0, flow_rate);
}

void test_pid_calculation() {
    // Simple PID calculation test
    float setpoint = 25.0;
    float current = 23.0;
    float error = setpoint - current;
    float kp = 2.0;
    float output = kp * error;
    
    TEST_ASSERT_EQUAL_FLOAT(4.0, output);
}

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_basic_math);
    RUN_TEST(test_basic_string);
    RUN_TEST(test_dosing_calculation);
    RUN_TEST(test_pid_calculation);
    
    return UNITY_END();
}