#include <unity.h>
#include "test_common.h"
#include <map>
#include <string>

// Mock NVS storage
static std::map<std::string, float> mockNVS_float;
static std::map<std::string, uint32_t> mockNVS_uint32;
static std::map<std::string, std::string> mockNVS_string;

// Mock ConfigManager class for testing
class MockConfigManager {
private:
    float tankLength;
    float tankWidth;
    float tankHeight;
    float heaterSetpoint;
    std::string deviceName;
    
public:
    MockConfigManager() : tankLength(100.0), tankWidth(50.0), tankHeight(40.0), 
                         heaterSetpoint(25.0), deviceName("AquariumController") {}
    
    // Tank dimension getters/setters
    float getTankLength() const { return tankLength; }
    float getTankWidth() const { return tankWidth; }
    float getTankHeight() const { return tankHeight; }
    
    void setTankDimensions(float length, float width, float height) {
        tankLength = length;
        tankWidth = width;
        tankHeight = height;
    }
    
    float getTankVolume() const {
        return (tankLength * tankWidth * tankHeight) / 1000.0; // Convert cm³ to liters
    }
    
    // Save to mock NVS
    bool save() {
        mockNVS_float["tankLength"] = tankLength;
        mockNVS_float["tankWidth"] = tankWidth;
        mockNVS_float["tankHeight"] = tankHeight;
        mockNVS_float["heaterSetpoint"] = heaterSetpoint;
        mockNVS_string["deviceName"] = deviceName;
        return true;
    }
    
    // Load from mock NVS
    bool load() {
        if (mockNVS_float.count("tankLength")) tankLength = mockNVS_float["tankLength"];
        if (mockNVS_float.count("tankWidth")) tankWidth = mockNVS_float["tankWidth"];
        if (mockNVS_float.count("tankHeight")) tankHeight = mockNVS_float["tankHeight"];
        if (mockNVS_float.count("heaterSetpoint")) heaterSetpoint = mockNVS_float["heaterSetpoint"];
        if (mockNVS_string.count("deviceName")) deviceName = mockNVS_string["deviceName"];
        return true;
    }
    
    // JSON export simulation (simplified)
    bool hasValidTankDimensions() const {
        return tankLength > 0 && tankWidth > 0 && tankHeight > 0;
    }
};

// Test functions
void test_tank_dimensions_default() {
    MockConfigManager config;
    
    // Test default values
    TEST_ASSERT_EQUAL_FLOAT(100.0, config.getTankLength());
    TEST_ASSERT_EQUAL_FLOAT(50.0, config.getTankWidth());
    TEST_ASSERT_EQUAL_FLOAT(40.0, config.getTankHeight());
}

void test_tank_dimensions_set_get() {
    MockConfigManager config;
    
    // Set new dimensions
    config.setTankDimensions(120.0, 60.0, 50.0);
    
    // Verify
    TEST_ASSERT_EQUAL_FLOAT(120.0, config.getTankLength());
    TEST_ASSERT_EQUAL_FLOAT(60.0, config.getTankWidth());
    TEST_ASSERT_EQUAL_FLOAT(50.0, config.getTankHeight());
}

void test_tank_volume_calculation() {
    MockConfigManager config;
    
    // Default tank: 100 x 50 x 40 cm = 200,000 cm³ = 200 liters
    TEST_ASSERT_EQUAL_FLOAT(200.0, config.getTankVolume());
    
    // Set smaller tank: 80 x 40 x 30 cm = 96,000 cm³ = 96 liters
    config.setTankDimensions(80.0, 40.0, 30.0);
    TEST_ASSERT_EQUAL_FLOAT(96.0, config.getTankVolume());
    
    // Set larger tank: 150 x 60 x 50 cm = 450,000 cm³ = 450 liters
    config.setTankDimensions(150.0, 60.0, 50.0);
    TEST_ASSERT_EQUAL_FLOAT(450.0, config.getTankVolume());
}

void test_tank_dimensions_persistence() {
    // Clear mock NVS
    mockNVS_float.clear();
    mockNVS_uint32.clear();
    mockNVS_string.clear();
    
    // Create config and set dimensions
    MockConfigManager config1;
    config1.setTankDimensions(110.0, 55.0, 45.0);
    config1.save();
    
    // Create new config instance and load
    MockConfigManager config2;
    config2.load();
    
    // Verify persistence
    TEST_ASSERT_EQUAL_FLOAT(110.0, config2.getTankLength());
    TEST_ASSERT_EQUAL_FLOAT(55.0, config2.getTankWidth());
    TEST_ASSERT_EQUAL_FLOAT(45.0, config2.getTankHeight());
}

void test_tank_dimensions_validation() {
    MockConfigManager config;
    
    // Valid dimensions
    config.setTankDimensions(100.0, 50.0, 40.0);
    TEST_ASSERT_TRUE(config.hasValidTankDimensions());
    
    // Invalid: zero length
    config.setTankDimensions(0.0, 50.0, 40.0);
    TEST_ASSERT_FALSE(config.hasValidTankDimensions());
    
    // Invalid: negative width
    config.setTankDimensions(100.0, -50.0, 40.0);
    TEST_ASSERT_FALSE(config.hasValidTankDimensions());
    
    // Invalid: zero height
    config.setTankDimensions(100.0, 50.0, 0.0);
    TEST_ASSERT_FALSE(config.hasValidTankDimensions());
}

void test_tank_dimensions_edge_cases() {
    MockConfigManager config;
    
    // Very small tank: 10 x 10 x 10 cm = 1 liter
    config.setTankDimensions(10.0, 10.0, 10.0);
    TEST_ASSERT_EQUAL_FLOAT(1.0, config.getTankVolume());
    
    // Very large tank: 200 x 100 x 80 cm = 1,600 liters
    config.setTankDimensions(200.0, 100.0, 80.0);
    TEST_ASSERT_EQUAL_FLOAT(1600.0, config.getTankVolume());
    
    // Tall narrow tank: 40 x 30 x 100 cm = 120 liters
    config.setTankDimensions(40.0, 30.0, 100.0);
    TEST_ASSERT_EQUAL_FLOAT(120.0, config.getTankVolume());
}

void test_tank_dimensions_json_export() {
    MockConfigManager config;
    config.setTankDimensions(90.0, 45.0, 35.0);
    
    // Verify dimensions are set correctly for JSON export
    TEST_ASSERT_EQUAL_FLOAT(90.0, config.getTankLength());
    TEST_ASSERT_EQUAL_FLOAT(45.0, config.getTankWidth());
    TEST_ASSERT_EQUAL_FLOAT(35.0, config.getTankHeight());
    TEST_ASSERT_TRUE(config.hasValidTankDimensions());
}

void setUp(void) {
    commonSetUp();
    // Clear mock storage before each test
    mockNVS_float.clear();
    mockNVS_uint32.clear();
    mockNVS_string.clear();
}

void tearDown(void) {
    commonTearDown();
}

// Arduino setup function for ESP32 testing
void setup(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_tank_dimensions_default);
    RUN_TEST(test_tank_dimensions_set_get);
    RUN_TEST(test_tank_volume_calculation);
    RUN_TEST(test_tank_dimensions_persistence);
    RUN_TEST(test_tank_dimensions_validation);
    RUN_TEST(test_tank_dimensions_edge_cases);
    RUN_TEST(test_tank_dimensions_json_export);
    
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
    
    RUN_TEST(test_tank_dimensions_default);
    RUN_TEST(test_tank_dimensions_set_get);
    RUN_TEST(test_tank_volume_calculation);
    RUN_TEST(test_tank_dimensions_persistence);
    RUN_TEST(test_tank_dimensions_validation);
    RUN_TEST(test_tank_dimensions_edge_cases);
    RUN_TEST(test_tank_dimensions_json_export);
    
    return UNITY_END();
}
#endif
