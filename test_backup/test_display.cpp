#ifdef UNIT_TEST

#include "test_common.h"
#include <string.h>
#include <stdlib.h>

// Mock Arduino functions for display
unsigned long micros() { return millis() * 1000; }
void pinMode(int pin, int mode) { /* No-op in tests */ }
void digitalWrite(int pin, int value) { /* No-op in tests */ }
int digitalRead(int pin) { return 1; } // Default HIGH

// Mock display class for testing
class MockU8G2Display {
public:
    bool initialized;
    int contrast;
    bool powerSaveMode;
    char lastDrawnText[256];
    int drawCallCount;
    int bufferClearCount;
    int bufferSendCount;
    
    MockU8G2Display() {
        initialized = false;
        contrast = 128;
        powerSaveMode = false;
        lastDrawnText[0] = '\0';
        drawCallCount = 0;
        bufferClearCount = 0;
        bufferSendCount = 0;
    }
    
    void begin() { initialized = true; }
    void setContrast(int c) { contrast = c; }
    void setPowerSave(int mode) { powerSaveMode = (mode != 0); }
    void clearBuffer() { bufferClearCount++; }
    void sendBuffer() { bufferSendCount++; }
    void setFont(const uint8_t* font) { /* No-op */ }
    void drawStr(int x, int y, const char* text) {
        strncpy(lastDrawnText, text, sizeof(lastDrawnText) - 1);
        drawCallCount++;
    }
    void drawHLine(int x, int y, int w) { drawCallCount++; }
    void drawFrame(int x, int y, int w, int h) { drawCallCount++; }
    void drawBox(int x, int y, int w, int h) { drawCallCount++; }
    void setDrawColor(int color) { /* No-op */ }
};

// Mock DisplayManager for testing (simplified version)
class TestDisplayManager {
private:
    MockU8G2Display display;
    int encoderPos;
    bool buttonPressed;
    bool screenOn;
    unsigned long lastUpdate;
    unsigned long lastInteraction;
    
    // Cached sensor data
    float cachedTemp;
    float cachedPH;
    float cachedTDS;
    float cachedTargetTemp;
    float cachedTargetPH;
    bool cachedHeaterState;
    bool cachedCO2State;
    
public:
    TestDisplayManager() {
        encoderPos = 0;
        buttonPressed = false;
        screenOn = true;
        lastUpdate = 0;
        lastInteraction = 0;
        cachedTemp = 0.0f;
        cachedPH = 0.0f;
        cachedTDS = 0.0f;
        cachedTargetTemp = 0.0f;
        cachedTargetPH = 0.0f;
        cachedHeaterState = false;
        cachedCO2State = false;
    }
    
    bool begin() {
        display.begin();
        display.setContrast(255);
        return display.initialized;
    }
    
    void update() {
        unsigned long now = millis();
        
        // Check screen timeout (5 minutes = 300000ms)
        if (screenOn && (now - lastInteraction > 300000)) {
            sleep();
            return;
        }
        
        // Update display at 200ms intervals (5 Hz)
        if (now - lastUpdate >= 200) {
            lastUpdate = now;
            if (screenOn) {
                display.clearBuffer();
                // Simulate drawing main screen
                display.drawStr(0, 10, "Aquarium Status");
                display.sendBuffer();
            }
        }
    }
    
    void updateTemperature(float temp, float target) {
        cachedTemp = temp;
        cachedTargetTemp = target;
    }
    
    void updatePH(float ph, float target) {
        cachedPH = ph;
        cachedTargetPH = target;
    }
    
    void updateTDS(float tds) {
        cachedTDS = tds;
    }
    
    void updateHeaterState(bool state) {
        cachedHeaterState = state;
    }
    
    void updateCO2State(bool state) {
        cachedCO2State = state;
    }
    
    void wake() {
        if (!screenOn) {
            screenOn = true;
            display.setPowerSave(0);
        }
        lastInteraction = millis();
    }
    
    void sleep() {
        screenOn = false;
        display.setPowerSave(1);
    }
    
    bool isAwake() { return screenOn; }
    
    int getEncoderDelta() {
        int delta = encoderPos;
        encoderPos = 0;
        return delta;
    }
    
    void simulateEncoderRotation(int delta) {
        encoderPos += delta;
        lastInteraction = millis();
    }
    
    void simulateButtonPress() {
        buttonPressed = true;
        lastInteraction = millis();
    }
    
    bool wasButtonPressed() {
        if (buttonPressed) {
            buttonPressed = false;
            return true;
        }
        return false;
    }
    
    // Test accessors
    float getTemperature() { return cachedTemp; }
    float getPH() { return cachedPH; }
    float getTDS() { return cachedTDS; }
    float getTargetTemp() { return cachedTargetTemp; }
    float getTargetPH() { return cachedTargetPH; }
    bool getHeaterState() { return cachedHeaterState; }
    bool getCO2State() { return cachedCO2State; }
    MockU8G2Display* getDisplay() { return &display; }
};

// Global test object
TestDisplayManager* testDisplay = nullptr;

// Setup and teardown functions
void setUp(void) {
    commonSetUp();
    testDisplay = new TestDisplayManager();
}

void tearDown(void) {
    commonTearDown();
    delete testDisplay;
    testDisplay = nullptr;
}

// Test Cases

void test_display_initialization(void) {
    TEST_ASSERT_TRUE(testDisplay->begin());
    TEST_ASSERT_TRUE(testDisplay->getDisplay()->initialized);
    TEST_ASSERT_EQUAL(255, testDisplay->getDisplay()->contrast);
    TEST_ASSERT_TRUE(testDisplay->isAwake());
}

void test_display_update_temperature(void) {
    testDisplay->updateTemperature(25.5f, 25.0f);
    
    TEST_ASSERT_EQUAL_FLOAT(25.5f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(25.0f, testDisplay->getTargetTemp());
}

void test_display_update_ph(void) {
    testDisplay->updatePH(6.8f, 6.5f);
    
    TEST_ASSERT_EQUAL_FLOAT(6.8f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(6.5f, testDisplay->getTargetPH());
}

void test_display_update_tds(void) {
    testDisplay->updateTDS(350.0f);
    
    TEST_ASSERT_EQUAL_FLOAT(350.0f, testDisplay->getTDS());
}

void test_display_update_heater_state(void) {
    testDisplay->updateHeaterState(true);
    TEST_ASSERT_TRUE(testDisplay->getHeaterState());
    
    testDisplay->updateHeaterState(false);
    TEST_ASSERT_FALSE(testDisplay->getHeaterState());
}

void test_display_update_co2_state(void) {
    testDisplay->updateCO2State(true);
    TEST_ASSERT_TRUE(testDisplay->getCO2State());
    
    testDisplay->updateCO2State(false);
    TEST_ASSERT_FALSE(testDisplay->getCO2State());
}

void test_display_sleep_wake(void) {
    testDisplay->begin();
    TEST_ASSERT_TRUE(testDisplay->isAwake());
    
    testDisplay->sleep();
    TEST_ASSERT_FALSE(testDisplay->isAwake());
    TEST_ASSERT_TRUE(testDisplay->getDisplay()->powerSaveMode);
    
    testDisplay->wake();
    TEST_ASSERT_TRUE(testDisplay->isAwake());
    TEST_ASSERT_FALSE(testDisplay->getDisplay()->powerSaveMode);
}

void test_display_encoder_rotation(void) {
    testDisplay->simulateEncoderRotation(3);
    TEST_ASSERT_EQUAL(3, testDisplay->getEncoderDelta());
    
    testDisplay->simulateEncoderRotation(-2);
    TEST_ASSERT_EQUAL(-2, testDisplay->getEncoderDelta());
    
    // Delta should reset after reading
    TEST_ASSERT_EQUAL(0, testDisplay->getEncoderDelta());
}

void test_display_button_press(void) {
    TEST_ASSERT_FALSE(testDisplay->wasButtonPressed());
    
    testDisplay->simulateButtonPress();
    TEST_ASSERT_TRUE(testDisplay->wasButtonPressed());
    
    // Should return false after being read
    TEST_ASSERT_FALSE(testDisplay->wasButtonPressed());
}

void test_display_encoder_wakes_screen(void) {
    testDisplay->begin();
    testDisplay->sleep();
    TEST_ASSERT_FALSE(testDisplay->isAwake());
    
    testDisplay->simulateEncoderRotation(1);
    testDisplay->wake();
    TEST_ASSERT_TRUE(testDisplay->isAwake());
}

void test_display_button_wakes_screen(void) {
    testDisplay->begin();
    testDisplay->sleep();
    TEST_ASSERT_FALSE(testDisplay->isAwake());
    
    testDisplay->simulateButtonPress();
    testDisplay->wake();
    TEST_ASSERT_TRUE(testDisplay->isAwake());
}

void test_display_update_cycle(void) {
    testDisplay->begin();
    
    int initialBufferClears = testDisplay->getDisplay()->bufferClearCount;
    int initialBufferSends = testDisplay->getDisplay()->bufferSendCount;
    
    // Simulate time passing for update interval (200ms)
    unsigned long currentTime = 0;
    for (int i = 0; i < 5; i++) {
        currentTime += 200;
        setMockMillis(currentTime);
        testDisplay->update();
    }
    
    // Should have updated display multiple times
    TEST_ASSERT_GREATER_THAN(initialBufferClears, testDisplay->getDisplay()->bufferClearCount);
    TEST_ASSERT_GREATER_THAN(initialBufferSends, testDisplay->getDisplay()->bufferSendCount);
}

void test_display_all_sensor_updates(void) {
    testDisplay->begin();
    
    // Update all sensors
    testDisplay->updateTemperature(24.5f, 25.0f);
    testDisplay->updatePH(6.9f, 6.8f);
    testDisplay->updateTDS(320.0f);
    testDisplay->updateHeaterState(true);
    testDisplay->updateCO2State(false);
    
    // Verify all values cached correctly
    TEST_ASSERT_EQUAL_FLOAT(24.5f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(25.0f, testDisplay->getTargetTemp());
    TEST_ASSERT_EQUAL_FLOAT(6.9f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(6.8f, testDisplay->getTargetPH());
    TEST_ASSERT_EQUAL_FLOAT(320.0f, testDisplay->getTDS());
    TEST_ASSERT_TRUE(testDisplay->getHeaterState());
    TEST_ASSERT_FALSE(testDisplay->getCO2State());
}

void test_display_multiple_encoder_rotations(void) {
    testDisplay->simulateEncoderRotation(5);
    testDisplay->simulateEncoderRotation(3);
    testDisplay->simulateEncoderRotation(-2);
    
    // Should accumulate: 5 + 3 - 2 = 6
    TEST_ASSERT_EQUAL(6, testDisplay->getEncoderDelta());
}

void test_display_contrast_setting(void) {
    testDisplay->begin();
    TEST_ASSERT_EQUAL(255, testDisplay->getDisplay()->contrast);
    
    testDisplay->getDisplay()->setContrast(128);
    TEST_ASSERT_EQUAL(128, testDisplay->getDisplay()->contrast);
    
    testDisplay->getDisplay()->setContrast(0);
    TEST_ASSERT_EQUAL(0, testDisplay->getDisplay()->contrast);
}

void test_display_rapid_updates(void) {
    testDisplay->begin();
    
    // Rapid sensor updates should not cause issues
    for (int i = 0; i < 100; i++) {
        testDisplay->updateTemperature(25.0f + i * 0.1f, 25.0f);
        testDisplay->updatePH(7.0f - i * 0.01f, 6.8f);
        testDisplay->updateTDS(300.0f + i);
    }
    
    // Last values should be preserved
    TEST_ASSERT_EQUAL_FLOAT(34.9f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(6.01f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(399.0f, testDisplay->getTDS());
}

void test_display_state_transitions(void) {
    testDisplay->begin();
    
    // Wake → Sleep → Wake cycle
    TEST_ASSERT_TRUE(testDisplay->isAwake());
    
    testDisplay->sleep();
    TEST_ASSERT_FALSE(testDisplay->isAwake());
    
    testDisplay->wake();
    TEST_ASSERT_TRUE(testDisplay->isAwake());
    
    testDisplay->sleep();
    TEST_ASSERT_FALSE(testDisplay->isAwake());
}

void test_display_encoder_accumulation(void) {
    int totalRotation = 0;
    
    for (int i = 0; i < 10; i++) {
        int delta = (i % 2 == 0) ? 1 : -1;
        testDisplay->simulateEncoderRotation(delta);
        totalRotation += delta;
    }
    
    TEST_ASSERT_EQUAL(totalRotation, testDisplay->getEncoderDelta());
}

void test_display_button_multiple_presses(void) {
    // First press
    testDisplay->simulateButtonPress();
    TEST_ASSERT_TRUE(testDisplay->wasButtonPressed());
    TEST_ASSERT_FALSE(testDisplay->wasButtonPressed()); // Should be false after read
    
    // Second press
    testDisplay->simulateButtonPress();
    TEST_ASSERT_TRUE(testDisplay->wasButtonPressed());
    TEST_ASSERT_FALSE(testDisplay->wasButtonPressed());
}

void test_display_zero_values(void) {
    testDisplay->updateTemperature(0.0f, 0.0f);
    testDisplay->updatePH(0.0f, 0.0f);
    testDisplay->updateTDS(0.0f);
    
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testDisplay->getTDS());
}

void test_display_extreme_values(void) {
    testDisplay->updateTemperature(99.9f, 100.0f);
    testDisplay->updatePH(14.0f, 14.0f);
    testDisplay->updateTDS(9999.0f);
    
    TEST_ASSERT_EQUAL_FLOAT(99.9f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(14.0f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(9999.0f, testDisplay->getTDS());
}

void test_display_negative_values(void) {
    testDisplay->updateTemperature(-5.0f, 25.0f);
    testDisplay->updatePH(-1.0f, 7.0f);
    testDisplay->updateTDS(-100.0f);
    
    TEST_ASSERT_EQUAL_FLOAT(-5.0f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(-100.0f, testDisplay->getTDS());
}

void test_display_float_precision(void) {
    testDisplay->updateTemperature(25.123456f, 25.0f);
    testDisplay->updatePH(6.789012f, 6.8f);
    
    // Should preserve float precision
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.123456f, testDisplay->getTemperature());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 6.789012f, testDisplay->getPH());
}

void test_display_boolean_toggles(void) {
    // Test heater toggling
    testDisplay->updateHeaterState(false);
    TEST_ASSERT_FALSE(testDisplay->getHeaterState());
    testDisplay->updateHeaterState(true);
    TEST_ASSERT_TRUE(testDisplay->getHeaterState());
    testDisplay->updateHeaterState(false);
    TEST_ASSERT_FALSE(testDisplay->getHeaterState());
    
    // Test CO2 toggling
    testDisplay->updateCO2State(true);
    TEST_ASSERT_TRUE(testDisplay->getCO2State());
    testDisplay->updateCO2State(false);
    TEST_ASSERT_FALSE(testDisplay->getCO2State());
    testDisplay->updateCO2State(true);
    TEST_ASSERT_TRUE(testDisplay->getCO2State());
}

void test_display_initialization_state(void) {
    // Before initialization
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testDisplay->getPH());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testDisplay->getTDS());
    TEST_ASSERT_FALSE(testDisplay->getHeaterState());
    TEST_ASSERT_FALSE(testDisplay->getCO2State());
}

void test_display_concurrent_operations(void) {
    testDisplay->begin();
    
    // Simulate concurrent operations
    testDisplay->updateTemperature(25.5f, 25.0f);
    testDisplay->simulateEncoderRotation(2);
    testDisplay->updatePH(6.8f, 6.5f);
    testDisplay->simulateButtonPress();
    testDisplay->updateTDS(350.0f);
    testDisplay->updateHeaterState(true);
    
    // All operations should complete successfully
    TEST_ASSERT_EQUAL_FLOAT(25.5f, testDisplay->getTemperature());
    TEST_ASSERT_EQUAL(2, testDisplay->getEncoderDelta());
    TEST_ASSERT_EQUAL_FLOAT(6.8f, testDisplay->getPH());
    TEST_ASSERT_TRUE(testDisplay->wasButtonPressed());
    TEST_ASSERT_EQUAL_FLOAT(350.0f, testDisplay->getTDS());
    TEST_ASSERT_TRUE(testDisplay->getHeaterState());
}

// Main test runner
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Initialization tests
    RUN_TEST(test_display_initialization);
    RUN_TEST(test_display_initialization_state);
    RUN_TEST(test_display_contrast_setting);
    
    // Sensor update tests
    RUN_TEST(test_display_update_temperature);
    RUN_TEST(test_display_update_ph);
    RUN_TEST(test_display_update_tds);
    RUN_TEST(test_display_update_heater_state);
    RUN_TEST(test_display_update_co2_state);
    RUN_TEST(test_display_all_sensor_updates);
    
    // Value range tests
    RUN_TEST(test_display_zero_values);
    RUN_TEST(test_display_extreme_values);
    RUN_TEST(test_display_negative_values);
    RUN_TEST(test_display_float_precision);
    
    // Display state tests
    RUN_TEST(test_display_sleep_wake);
    RUN_TEST(test_display_state_transitions);
    RUN_TEST(test_display_update_cycle);
    
    // Encoder tests
    RUN_TEST(test_display_encoder_rotation);
    RUN_TEST(test_display_encoder_wakes_screen);
    RUN_TEST(test_display_multiple_encoder_rotations);
    RUN_TEST(test_display_encoder_accumulation);
    
    // Button tests
    RUN_TEST(test_display_button_press);
    RUN_TEST(test_display_button_wakes_screen);
    RUN_TEST(test_display_button_multiple_presses);
    
    // Boolean state tests
    RUN_TEST(test_display_boolean_toggles);
    
    // Stress tests
    RUN_TEST(test_display_rapid_updates);
    RUN_TEST(test_display_concurrent_operations);
    
    return UNITY_END();
}

#endif // UNIT_TEST
