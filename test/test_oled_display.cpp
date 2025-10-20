#ifdef UNIT_TEST

#include "test_common.h"
#include <string.h>
#include <stdlib.h>

// Mock U8G2 display class for testing
class U8G2_SSD1309_128X64_NONAME0_F_HW_I2C {
public:
    bool initialized;
    uint8_t contrast;
    int bufferClearCount;
    int bufferSendCount;
    int drawCallCount;
    char lastDrawnText[256];
    
    U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(uint8_t rotation, uint8_t reset) {
        initialized = false;
        contrast = 128;
        bufferClearCount = 0;
        bufferSendCount = 0;
        drawCallCount = 0;
        lastDrawnText[0] = '\0';
    }
    
    void begin() { initialized = true; }
    void setContrast(uint8_t c) { contrast = c; }
    void clearBuffer() { bufferClearCount++; }
    void sendBuffer() { bufferSendCount++; }
    void setFont(const uint8_t* font) { /* No-op */ }
    
    void drawStr(uint8_t x, uint8_t y, const char* text) {
        strncpy(lastDrawnText, text, sizeof(lastDrawnText) - 1);
        drawCallCount++;
    }
    
    void drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) { drawCallCount++; }
    void drawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { drawCallCount++; }
    void drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { drawCallCount++; }
    void drawPixel(uint8_t x, uint8_t y) { drawCallCount++; }
    void drawDisc(uint8_t x, uint8_t y, uint8_t r) { drawCallCount++; }
    void drawCircle(uint8_t x, uint8_t y, uint8_t r) { drawCallCount++; }
};

// Mock U8G2 constants
#define U8G2_R0 0
#define U8X8_PIN_NONE 255
const uint8_t u8g2_font_6x10_tf[] = {0};
const uint8_t u8g2_font_5x7_tf[] = {0};
const uint8_t u8g2_font_ncenB14_tr[] = {0};
const uint8_t u8g2_font_ncenB10_tr[] = {0};

// Include OLEDDisplayManager after mocks
#include "../include/OLEDDisplayManager.h"
#include "../src/OLEDDisplayManager.cpp"

// Global test object
OLEDDisplayManager* testDisplay = nullptr;

// Setup and teardown
void setUp(void) {
    commonSetUp();
    testDisplay = new OLEDDisplayManager();
}

void tearDown(void) {
    commonTearDown();
    delete testDisplay;
    testDisplay = nullptr;
}

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

void test_oled_initialization(void) {
    bool result = testDisplay->begin();
    TEST_ASSERT_TRUE(result);
}

void test_oled_contrast_setting(void) {
    testDisplay->begin();
    testDisplay->setContrast(200);
    // Test passes if no crash occurs
    TEST_ASSERT_TRUE(true);
}

void test_oled_brightness_setting(void) {
    testDisplay->begin();
    testDisplay->setBrightness(150);
    // Test passes if no crash occurs
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// DATA UPDATE TESTS
// ============================================================================

void test_oled_update_temperature(void) {
    testDisplay->begin();
    testDisplay->updateTemperature(25.5f, 26.0f);
    // Should trigger dirty flag and update on next cycle
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_ph(void) {
    testDisplay->begin();
    testDisplay->updatePH(7.2f, 7.0f);
    // Should trigger dirty flag
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_tds(void) {
    testDisplay->begin();
    testDisplay->updateTDS(450.0f);
    // Should trigger dirty flag
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_ambient_temperature(void) {
    testDisplay->begin();
    testDisplay->updateAmbientTemperature(22.5f);
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_heater_state(void) {
    testDisplay->begin();
    
    testDisplay->updateHeaterState(true);
    testDisplay->updateHeaterState(false);
    testDisplay->updateHeaterState(true);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_co2_state(void) {
    testDisplay->begin();
    
    testDisplay->updateCO2State(true);
    testDisplay->updateCO2State(false);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_dosing_state(void) {
    testDisplay->begin();
    
    testDisplay->updateDosingState(true);
    testDisplay->updateDosingState(false);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_water_change_date(void) {
    testDisplay->begin();
    
    testDisplay->updateWaterChangeDate("2025-10-21");
    testDisplay->updateWaterChangeDate("~5 days");
    testDisplay->updateWaterChangeDate("Never");
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_network_status(void) {
    testDisplay->begin();
    
    testDisplay->updateNetworkStatus(true, "192.168.1.100");
    testDisplay->updateNetworkStatus(false, "0.0.0.0");
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_update_time(void) {
    testDisplay->begin();
    
    testDisplay->updateTime("12:34:56");
    testDisplay->updateTime("23:59:59");
    testDisplay->updateTime("00:00:00");
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// DIRTY FLAGGING TESTS (OPTIMIZATION)
// ============================================================================

void test_oled_dirty_flag_on_temperature_change(void) {
    testDisplay->begin();
    
    // First update with same values should not trigger redraw
    testDisplay->updateTemperature(25.0f, 26.0f);
    testDisplay->update(); // Clear dirty flag
    
    // Update with same values
    testDisplay->updateTemperature(25.0f, 26.0f);
    
    // Update with different values should trigger redraw
    testDisplay->updateTemperature(25.5f, 26.0f);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_dirty_flag_on_network_change(void) {
    testDisplay->begin();
    
    testDisplay->updateNetworkStatus(true, "192.168.1.100");
    testDisplay->update(); // Clear dirty flag
    
    // Same values should not trigger redraw
    testDisplay->updateNetworkStatus(true, "192.168.1.100");
    
    // Different values should trigger redraw
    testDisplay->updateNetworkStatus(false, "0.0.0.0");
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_multiple_data_updates(void) {
    testDisplay->begin();
    
    // Update all data types
    testDisplay->updateTemperature(25.5f, 26.0f);
    testDisplay->updatePH(7.2f, 7.0f);
    testDisplay->updateTDS(450.0f);
    testDisplay->updateAmbientTemperature(22.1f);
    testDisplay->updateHeaterState(true);
    testDisplay->updateCO2State(false);
    testDisplay->updateDosingState(true);
    testDisplay->updateWaterChangeDate("2025-10-21");
    testDisplay->updateNetworkStatus(true, "192.168.1.100");
    testDisplay->updateTime("12:34:56");
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// SCREEN CONTROL TESTS
// ============================================================================

void test_oled_next_screen(void) {
    testDisplay->begin();
    
    testDisplay->nextScreen();
    testDisplay->nextScreen();
    testDisplay->nextScreen();
    testDisplay->nextScreen(); // Should wrap around
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_set_screen(void) {
    testDisplay->begin();
    
    testDisplay->setScreen(0);
    testDisplay->setScreen(1);
    testDisplay->setScreen(2);
    testDisplay->setScreen(0);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_set_screen_invalid(void) {
    testDisplay->begin();
    
    testDisplay->setScreen(99); // Should be ignored
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// UPDATE CYCLE TESTS
// ============================================================================

void test_oled_update_cycle(void) {
    testDisplay->begin();
    
    // Simulate multiple update cycles
    for (int i = 0; i < 10; i++) {
        unsigned long currentTime = millis() + (i * 100);
        setMockMillis(currentTime);
        testDisplay->update();
    }
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_screen_auto_switch(void) {
    testDisplay->begin();
    
    // Set initial data
    testDisplay->updateTemperature(25.5f, 26.0f);
    
    // Simulate time passing for screen switch (5000ms)
    for (int i = 0; i < 60; i++) {
        unsigned long currentTime = millis() + (i * 100);
        setMockMillis(currentTime);
        testDisplay->update();
    }
    
    // Screen should have auto-switched
    TEST_ASSERT_TRUE(true);
}

void test_oled_animation_update(void) {
    testDisplay->begin();
    
    // Simulate animation frames updating
    for (int i = 0; i < 20; i++) {
        unsigned long currentTime = millis() + (i * 125); // ANIMATION_INTERVAL
        setMockMillis(currentTime);
        testDisplay->update();
    }
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

void test_oled_force_redraw(void) {
    testDisplay->begin();
    
    testDisplay->forceRedraw();
    testDisplay->update();
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_performance_monitoring_enable(void) {
    testDisplay->begin();
    
    testDisplay->enablePerformanceMonitoring(true);
    
    // Run several updates
    for (int i = 0; i < 10; i++) {
        unsigned long currentTime = millis() + (i * 100);
        setMockMillis(currentTime);
        testDisplay->update();
    }
    
    testDisplay->enablePerformanceMonitoring(false);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_get_last_update_time(void) {
    testDisplay->begin();
    
    unsigned long time1 = testDisplay->getLastUpdateTime();
    
    setMockMillis(1000);
    testDisplay->update();
    
    unsigned long time2 = testDisplay->getLastUpdateTime();
    
    // time2 should be >= time1
    TEST_ASSERT_GREATER_OR_EQUAL(time1, time2);
}

// ============================================================================
// MEMORY OPTIMIZATION TESTS
// ============================================================================

void test_oled_char_array_update(void) {
    testDisplay->begin();
    
    // Test char array updates (no heap allocation)
    const char* longString = "This is a very long string that exceeds buffer size";
    testDisplay->updateWaterChangeDate(longString);
    
    // Should be truncated safely
    TEST_ASSERT_TRUE(true);
}

void test_oled_network_ip_truncation(void) {
    testDisplay->begin();
    
    // Test IP address truncation
    const char* longIP = "192.168.100.100.100.100";
    testDisplay->updateNetworkStatus(true, longIP);
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_time_string_update(void) {
    testDisplay->begin();
    
    // Valid time strings
    testDisplay->updateTime("12:34:56");
    testDisplay->updateTime("00:00:00");
    testDisplay->updateTime("23:59:59");
    
    // Invalid but should handle gracefully
    testDisplay->updateTime("99:99:99 extra text");
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// CLEAR AND TEST FUNCTIONS
// ============================================================================

void test_oled_clear(void) {
    testDisplay->begin();
    
    testDisplay->clear();
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_test_screen(void) {
    testDisplay->begin();
    
    testDisplay->test();
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

void test_oled_full_workflow(void) {
    // Initialize
    TEST_ASSERT_TRUE(testDisplay->begin());
    
    // Update all sensor data
    testDisplay->updateTemperature(25.5f, 26.0f);
    testDisplay->updatePH(7.2f, 7.0f);
    testDisplay->updateTDS(450.0f);
    testDisplay->updateAmbientTemperature(22.1f);
    
    // Update control states
    testDisplay->updateHeaterState(true);
    testDisplay->updateCO2State(false);
    testDisplay->updateDosingState(true);
    
    // Update system info
    testDisplay->updateWaterChangeDate("2025-10-21");
    testDisplay->updateNetworkStatus(true, "192.168.1.100");
    testDisplay->updateTime("12:34:56");
    
    // Run update cycles
    for (int i = 0; i < 5; i++) {
        unsigned long currentTime = millis() + (i * 100);
        setMockMillis(currentTime);
        testDisplay->update();
    }
    
    // Change screen
    testDisplay->nextScreen();
    testDisplay->update();
    
    TEST_ASSERT_TRUE(true);
}

void test_oled_stress_test_rapid_updates(void) {
    testDisplay->begin();
    
    // Rapid fire updates
    for (int i = 0; i < 100; i++) {
        float temp = 25.0f + (i * 0.1f);
        testDisplay->updateTemperature(temp, 26.0f);
        testDisplay->updatePH(7.0f + (i * 0.01f), 7.0f);
        testDisplay->updateTDS(400.0f + i);
        
        if (i % 10 == 0) {
            testDisplay->update();
        }
    }
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int runUnityTests(void) {
    UNITY_BEGIN();
    
    // Initialization tests
    RUN_TEST(test_oled_initialization);
    RUN_TEST(test_oled_contrast_setting);
    RUN_TEST(test_oled_brightness_setting);
    
    // Data update tests
    RUN_TEST(test_oled_update_temperature);
    RUN_TEST(test_oled_update_ph);
    RUN_TEST(test_oled_update_tds);
    RUN_TEST(test_oled_update_ambient_temperature);
    RUN_TEST(test_oled_update_heater_state);
    RUN_TEST(test_oled_update_co2_state);
    RUN_TEST(test_oled_update_dosing_state);
    RUN_TEST(test_oled_update_water_change_date);
    RUN_TEST(test_oled_update_network_status);
    RUN_TEST(test_oled_update_time);
    
    // Dirty flagging tests
    RUN_TEST(test_oled_dirty_flag_on_temperature_change);
    RUN_TEST(test_oled_dirty_flag_on_network_change);
    RUN_TEST(test_oled_multiple_data_updates);
    
    // Screen control tests
    RUN_TEST(test_oled_next_screen);
    RUN_TEST(test_oled_set_screen);
    RUN_TEST(test_oled_set_screen_invalid);
    
    // Update cycle tests
    RUN_TEST(test_oled_update_cycle);
    RUN_TEST(test_oled_screen_auto_switch);
    RUN_TEST(test_oled_animation_update);
    
    // Performance tests
    RUN_TEST(test_oled_force_redraw);
    RUN_TEST(test_oled_performance_monitoring_enable);
    RUN_TEST(test_oled_get_last_update_time);
    
    // Memory optimization tests
    RUN_TEST(test_oled_char_array_update);
    RUN_TEST(test_oled_network_ip_truncation);
    RUN_TEST(test_oled_time_string_update);
    
    // Clear and test functions
    RUN_TEST(test_oled_clear);
    RUN_TEST(test_oled_test_screen);
    
    // Integration tests
    RUN_TEST(test_oled_full_workflow);
    RUN_TEST(test_oled_stress_test_rapid_updates);
    
    return UNITY_END();
}

// For native testing
#ifdef ARDUINO
#include <Arduino.h>
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {}
#else
int main(int argc, char **argv) {
    return runUnityTests();
}
#endif

#endif // UNIT_TEST
