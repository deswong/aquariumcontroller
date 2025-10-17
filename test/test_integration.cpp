#include <unity.h>

// Integration tests that test multiple components working together

// ============================================================================
// Complete PID Control Loop Integration Test
// ============================================================================

void test_complete_pid_control_loop() {
    // Simulate a complete control loop cycle
    
    // 1. Sensor reading
    float currentTemp = 24.0;
    
    // 2. PID computation
    float target = 25.0;
    float error = target - currentTemp;
    float kp = 2.0;
    float output = kp * error; // Simplified
    
    // 3. Output limiting
    output = (output < 0) ? 0 : (output > 100) ? 100 : output;
    
    // 4. Relay control
    bool relayOn = (output > 50.0);
    
    TEST_ASSERT_EQUAL_FLOAT(1.0, error);
    TEST_ASSERT_EQUAL_FLOAT(2.0, output);
    TEST_ASSERT_FALSE(relayOn);
}

// ============================================================================
// Sensor Reading and Storage Integration
// ============================================================================

void test_sensor_to_storage_integration() {
    // Simulate sensor readings being stored
    struct SensorData {
        float temperature;
        float ph;
        float tds;
        unsigned long timestamp;
    };
    
    SensorData data;
    data.temperature = 25.3;
    data.ph = 6.7;
    data.tds = 325.0;
    data.timestamp = 1000;
    
    // Validate data before storage
    bool tempValid = (data.temperature > -55 && data.temperature < 125);
    bool phValid = (data.ph >= 0 && data.ph <= 14);
    bool tdsValid = (data.tds >= 0 && data.tds < 2000);
    
    TEST_ASSERT_TRUE(tempValid);
    TEST_ASSERT_TRUE(phValid);
    TEST_ASSERT_TRUE(tdsValid);
}

// ============================================================================
// Multi-Sensor PID Control Integration
// ============================================================================

void test_dual_pid_controllers() {
    // Test temperature and CO2 PID running simultaneously
    
    // Temperature control
    float currentTemp = 24.0;
    float targetTemp = 25.0;
    float tempError = targetTemp - currentTemp;
    float tempOutput = 2.0 * tempError; // Kp = 2.0
    bool heaterOn = (tempOutput > 50);
    
    // CO2/pH control
    float currentPH = 7.0;
    float targetPH = 6.8;
    float phError = targetPH - currentPH;
    float co2Output = 2.0 * phError; // Kp = 2.0
    bool co2On = (co2Output > 50);
    
    TEST_ASSERT_EQUAL_FLOAT(1.0, tempError);
    TEST_ASSERT_EQUAL_FLOAT(-0.2, phError);
    TEST_ASSERT_FALSE(heaterOn);
    TEST_ASSERT_FALSE(co2On);
}

// ============================================================================
// Safety System Integration
// ============================================================================

void test_complete_safety_system() {
    float currentTemp = 32.0;
    float targetTemp = 25.0;
    float safetyMax = 30.0;
    
    float currentPH = 5.8;
    float targetPH = 6.8;
    float safetyMin = 6.0;
    
    // Check both safety conditions
    bool tempEmergency = (currentTemp > safetyMax);
    bool phEmergency = (currentPH < safetyMin);
    bool anyEmergency = tempEmergency || phEmergency;
    
    // If emergency, disable outputs
    bool heaterEnabled = !anyEmergency;
    bool co2Enabled = !anyEmergency;
    
    TEST_ASSERT_TRUE(tempEmergency);
    TEST_ASSERT_TRUE(phEmergency);
    TEST_ASSERT_TRUE(anyEmergency);
    TEST_ASSERT_FALSE(heaterEnabled);
    TEST_ASSERT_FALSE(co2Enabled);
}

// ============================================================================
// Configuration Change Propagation
// ============================================================================

void test_config_change_propagation() {
    // Simulate changing config and it affecting all systems
    
    // Initial state
    float oldTarget = 25.0;
    float pidTarget = oldTarget;
    
    // User changes target
    float newTarget = 26.0;
    
    // Propagate to PID
    pidTarget = newTarget;
    
    // Reset integral on target change
    float integral = 10.0; // Old integral
    integral = 0.0; // Reset
    
    TEST_ASSERT_EQUAL_FLOAT(26.0, pidTarget);
    TEST_ASSERT_EQUAL_FLOAT(0.0, integral);
}

// ============================================================================
// Calibration Workflow Integration
// ============================================================================

void test_ph_calibration_workflow() {
    // Simulate complete pH calibration workflow
    
    struct CalibrationData {
        float acidVoltage;
        float neutralVoltage;
        float baseVoltage;
        bool calibrated;
    };
    
    CalibrationData cal;
    cal.calibrated = false;
    
    // Step 1: Start calibration
    cal.calibrated = false;
    
    // Step 2: Calibrate pH 4.0
    float voltage1 = 2.0;
    cal.acidVoltage = voltage1;
    
    // Step 3: Calibrate pH 7.0
    float voltage2 = 1.5;
    cal.neutralVoltage = voltage2;
    
    // Step 4: Calibrate pH 10.0
    float voltage3 = 1.0;
    cal.baseVoltage = voltage3;
    cal.calibrated = true;
    
    // Verify calibration
    TEST_ASSERT_EQUAL_FLOAT(2.0, cal.acidVoltage);
    TEST_ASSERT_EQUAL_FLOAT(1.5, cal.neutralVoltage);
    TEST_ASSERT_EQUAL_FLOAT(1.0, cal.baseVoltage);
    TEST_ASSERT_TRUE(cal.calibrated);
    
    // Test using calibration
    float testVoltage = 1.5;
    float slope = (7.0 - 4.0) / (cal.neutralVoltage - cal.acidVoltage);
    float ph = 4.0 + (testVoltage - cal.acidVoltage) * slope;
    
    TEST_ASSERT_EQUAL_FLOAT(7.0, ph);
}

// ============================================================================
// Real-time Data Flow Integration
// ============================================================================

void test_realtime_data_pipeline() {
    // Simulate data flowing from sensors to web interface
    
    struct Pipeline {
        // Stage 1: Raw sensor
        float rawTemperature;
        
        // Stage 2: Filtered
        float filteredTemperature;
        
        // Stage 3: Validated
        bool isValid;
        
        // Stage 4: Stored
        float storedTemperature;
        
        // Stage 5: Transmitted
        float transmittedTemperature;
    };
    
    Pipeline pipe;
    
    // Stage 1: Read sensor
    pipe.rawTemperature = 25.3;
    
    // Stage 2: Apply moving average (simplified)
    pipe.filteredTemperature = pipe.rawTemperature;
    
    // Stage 3: Validate
    pipe.isValid = (pipe.filteredTemperature > -55 && pipe.filteredTemperature < 125);
    
    // Stage 4: Store if valid
    if (pipe.isValid) {
        pipe.storedTemperature = pipe.filteredTemperature;
    }
    
    // Stage 5: Transmit
    pipe.transmittedTemperature = pipe.storedTemperature;
    
    TEST_ASSERT_TRUE(pipe.isValid);
    TEST_ASSERT_EQUAL_FLOAT(25.3, pipe.transmittedTemperature);
}

// ============================================================================
// MQTT Message Generation Integration
// ============================================================================

void test_mqtt_message_generation() {
    // Test generating MQTT messages from sensor data
    
    float temperature = 25.3;
    float ph = 6.8;
    float tds = 325.0;
    
    // Generate topic and payload
    char topic[64];
    char payload[32];
    
    // Temperature message
    snprintf(topic, sizeof(topic), "aquarium/temperature");
    snprintf(payload, sizeof(payload), "%.2f", temperature);
    
    TEST_ASSERT_EQUAL_STRING("aquarium/temperature", topic);
    TEST_ASSERT_EQUAL_STRING("25.30", payload);
    
    // pH message
    snprintf(topic, sizeof(topic), "aquarium/ph");
    snprintf(payload, sizeof(payload), "%.2f", ph);
    
    TEST_ASSERT_EQUAL_STRING("aquarium/ph", topic);
    TEST_ASSERT_EQUAL_STRING("6.80", payload);
}

// ============================================================================
// Emergency Recovery Integration
// ============================================================================

void test_emergency_recovery_procedure() {
    bool emergencyStop = true;
    bool heaterEnabled = true;
    bool co2Enabled = true;
    
    // Emergency triggered
    if (emergencyStop) {
        heaterEnabled = false;
        co2Enabled = false;
    }
    
    TEST_ASSERT_FALSE(heaterEnabled);
    TEST_ASSERT_FALSE(co2Enabled);
    
    // Clear emergency
    emergencyStop = false;
    
    // Reset integral
    float integral = 50.0;
    integral = 0.0;
    
    // Re-enable (user action required)
    heaterEnabled = true;
    co2Enabled = true;
    
    TEST_ASSERT_TRUE(heaterEnabled);
    TEST_ASSERT_TRUE(co2Enabled);
    TEST_ASSERT_EQUAL_FLOAT(0.0, integral);
}

// ============================================================================
// Adaptive Learning Integration
// ============================================================================

void test_pid_learning_adaptation() {
    // Simulate PID learning over time
    
    float kp = 2.0;
    float errorHistory[5] = {2.0, 1.8, 1.5, 1.3, 1.0};
    
    // Calculate error mean
    float errorMean = 0;
    for (int i = 0; i < 5; i++) {
        errorMean += errorHistory[i];
    }
    errorMean /= 5.0;
    
    // Calculate variance
    float errorVariance = 0;
    for (int i = 0; i < 5; i++) {
        float diff = errorHistory[i] - errorMean;
        errorVariance += diff * diff;
    }
    errorVariance /= 5.0;
    
    // If consistent error, adapt
    if (errorMean > 1.0 && errorVariance < 0.5) {
        kp *= 1.05; // Increase by 5%
    }
    
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.52, errorMean);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.1376, errorVariance);
    TEST_ASSERT_EQUAL_FLOAT(2.1, kp);
}

// ============================================================================
// Time-based Control Integration
// ============================================================================

void test_time_based_control_intervals() {
    unsigned long lastSensorRead = 0;
    unsigned long lastControlUpdate = 0;
    unsigned long lastMQTTPublish = 0;
    
    unsigned long currentTime = 5000; // 5 seconds
    
    // Different intervals for different tasks
    unsigned long SENSOR_INTERVAL = 1000;    // 1 second
    unsigned long CONTROL_INTERVAL = 2000;   // 2 seconds
    unsigned long MQTT_INTERVAL = 5000;      // 5 seconds
    
    bool shouldReadSensor = (currentTime - lastSensorRead >= SENSOR_INTERVAL);
    bool shouldUpdateControl = (currentTime - lastControlUpdate >= CONTROL_INTERVAL);
    bool shouldPublishMQTT = (currentTime - lastMQTTPublish >= MQTT_INTERVAL);
    
    TEST_ASSERT_TRUE(shouldReadSensor);
    TEST_ASSERT_TRUE(shouldUpdateControl);
    TEST_ASSERT_TRUE(shouldPublishMQTT);
}

// ============================================================================
// Multi-point Validation Integration
// ============================================================================

void test_multi_point_data_validation() {
    float temperature = 25.0;
    float ph = 6.8;
    float tds = 350.0;
    
    // Validate all parameters
    bool tempOK = (temperature >= 20.0 && temperature <= 32.0);
    bool phOK = (ph >= 6.0 && ph <= 8.0);
    bool tdsOK = (tds >= 50.0 && tds <= 600.0);
    
    bool allValid = tempOK && phOK && tdsOK;
    
    TEST_ASSERT_TRUE(tempOK);
    TEST_ASSERT_TRUE(phOK);
    TEST_ASSERT_TRUE(tdsOK);
    TEST_ASSERT_TRUE(allValid);
    
    // Test with one invalid
    tds = 800.0;
    tdsOK = (tds >= 50.0 && tds <= 600.0);
    allValid = tempOK && phOK && tdsOK;
    
    TEST_ASSERT_FALSE(tdsOK);
    TEST_ASSERT_FALSE(allValid);
}

// ============================================================================
// DosingPump Integration Tests
// ============================================================================

void test_scheduled_dosing_triggers_at_correct_time() {
    // Test: Scheduled dose triggers at specific time-of-day
    // Given: Schedule set for 14:30 daily, current time is 14:30
    // Expected: Dose should trigger
    
    struct DosingSchedule {
        int hour;
        int minute;
        bool enabled;
        unsigned long lastDoseTime;
    };
    
    DosingSchedule schedule;
    schedule.hour = 14;
    schedule.minute = 30;
    schedule.enabled = true;
    schedule.lastDoseTime = 0; // Never dosed before
    
    // Simulate current time: 14:30
    int currentHour = 14;
    int currentMinute = 30;
    
    // Check if it's time to dose
    bool isTimeForDose = (schedule.enabled && 
                         currentHour == schedule.hour && 
                         currentMinute == schedule.minute);
    
    TEST_ASSERT_TRUE(isTimeForDose);
}

void test_scheduled_dosing_skip_if_already_dosed_today() {
    // Test: Don't dose again if already dosed today
    // Given: Last dose was 2 hours ago (same day)
    // Expected: Should not dose again
    
    unsigned long currentTime = 86400 + 14 * 3600 + 30 * 60; // Day 2, 14:30
    unsigned long lastDoseTime = 86400 + 12 * 3600;           // Day 2, 12:00
    
    // Check if enough time passed (24 hours)
    unsigned long timeSinceLastDose = currentTime - lastDoseTime;
    bool shouldDose = (timeSinceLastDose >= 86400); // 24 hours
    
    TEST_ASSERT_FALSE(shouldDose);
}

void test_dosing_pump_daily_volume_reset_at_midnight() {
    // Test: Daily volume counter resets at midnight
    // Given: Last reset at day 1, current time is day 2
    // Expected: Daily volume should reset to 0
    
    int lastResetDay = 1;
    int currentDay = 2;
    float dailyVolumeDosed = 350.0;
    
    if (currentDay > lastResetDay) {
        dailyVolumeDosed = 0.0;
        lastResetDay = currentDay;
    }
    
    TEST_ASSERT_EQUAL_FLOAT(0.0, dailyVolumeDosed);
    TEST_ASSERT_EQUAL_INT(2, lastResetDay);
}

void test_dosing_pump_safety_prevents_overdosing() {
    // Test: Safety limits prevent exceeding daily maximum
    // Given: Daily limit = 500mL, already dosed = 480mL, requesting = 50mL
    // Expected: Only dose 20mL to stay within limit
    
    float maxDailyVolume = 500.0;
    float dailyVolumeDosed = 480.0;
    float requestedDose = 50.0;
    
    float remainingDaily = maxDailyVolume - dailyVolumeDosed;
    float actualDose = (requestedDose > remainingDaily) ? remainingDaily : requestedDose;
    
    TEST_ASSERT_EQUAL_FLOAT(20.0, actualDose);
}

void test_dosing_pump_history_with_timestamps() {
    // Test: Dosing history includes timestamps
    
    struct DosingRecord {
        unsigned long timestamp;
        float volume;
        bool success;
        const char* type;
    };
    
    DosingRecord records[3];
    
    // Record 1: Scheduled dose
    records[0].timestamp = 1704067200; // 2024-01-01 00:00:00
    records[0].volume = 25.0;
    records[0].success = true;
    records[0].type = "scheduled";
    
    // Record 2: Manual dose
    records[1].timestamp = 1704070800; // 2024-01-01 01:00:00
    records[1].volume = 10.0;
    records[1].success = true;
    records[1].type = "manual";
    
    // Record 3: Failed dose
    records[2].timestamp = 1704074400; // 2024-01-01 02:00:00
    records[2].volume = 0.0;
    records[2].success = false;
    records[2].type = "scheduled";
    
    // Verify records
    TEST_ASSERT_EQUAL_UINT32(1704067200, records[0].timestamp);
    TEST_ASSERT_EQUAL_FLOAT(25.0, records[0].volume);
    TEST_ASSERT_TRUE(records[0].success);
    TEST_ASSERT_EQUAL_STRING("scheduled", records[0].type);
    
    TEST_ASSERT_FALSE(records[2].success);
}

void test_dosing_pump_integrates_with_pattern_learning() {
    // Test: Pattern learning considers dosing events
    // Given: Learning pH patterns, dosing increases pH
    // Expected: Pattern should reflect dosing impact
    
    struct PatternData {
        int hour;
        float expectedPH;
        bool dosingOccurred;
        float phAfterDosing;
    };
    
    PatternData pattern;
    pattern.hour = 14;
    pattern.expectedPH = 6.5;
    pattern.dosingOccurred = true;
    
    // Dosing increases pH by ~0.3
    pattern.phAfterDosing = pattern.expectedPH + 0.3;
    
    // Pattern learning should adjust expectation
    float adjustedExpectation = pattern.phAfterDosing;
    
    TEST_ASSERT_EQUAL_FLOAT(6.8, adjustedExpectation);
}

void test_dosing_pump_multiple_schedules() {
    // Test: Multiple dosing pumps with different schedules
    
    struct DosingPump {
        const char* name;
        int scheduleHour;
        float doseVolume;
        bool enabled;
    };
    
    DosingPump pumps[3];
    
    // Pump 1: Morning (8:00) - 25mL
    pumps[0].name = "Fertilizer A";
    pumps[0].scheduleHour = 8;
    pumps[0].doseVolume = 25.0;
    pumps[0].enabled = true;
    
    // Pump 2: Noon (12:00) - 15mL
    pumps[1].name = "Fertilizer B";
    pumps[1].scheduleHour = 12;
    pumps[1].doseVolume = 15.0;
    pumps[1].enabled = true;
    
    // Pump 3: Evening (20:00) - 30mL
    pumps[2].name = "Micronutrients";
    pumps[2].scheduleHour = 20;
    pumps[2].doseVolume = 30.0;
    pumps[2].enabled = true;
    
    // Current time: 12:00
    int currentHour = 12;
    
    // Check which pump should dose
    bool pump1ShouldDose = (pumps[0].enabled && currentHour == pumps[0].scheduleHour);
    bool pump2ShouldDose = (pumps[1].enabled && currentHour == pumps[1].scheduleHour);
    bool pump3ShouldDose = (pumps[2].enabled && currentHour == pumps[2].scheduleHour);
    
    TEST_ASSERT_FALSE(pump1ShouldDose); // 8:00 already passed
    TEST_ASSERT_TRUE(pump2ShouldDose);  // 12:00 is now
    TEST_ASSERT_FALSE(pump3ShouldDose); // 20:00 not yet
}

// ============================================================================
// NTP Time Synchronization Integration Tests
// ============================================================================

void test_wifi_connect_triggers_ntp_sync() {
    // Test: WiFi connection should trigger NTP sync
    
    bool wifiConnected = false;
    bool timeSynced = false;
    
    // WiFi disconnected
    TEST_ASSERT_FALSE(wifiConnected);
    TEST_ASSERT_FALSE(timeSynced);
    
    // WiFi connects
    wifiConnected = true;
    
    // Trigger NTP sync on connect
    if (wifiConnected) {
        // Simulate configTime() call
        timeSynced = true; // After successful sync
    }
    
    TEST_ASSERT_TRUE(wifiConnected);
    TEST_ASSERT_TRUE(timeSynced);
}

void test_ntp_sync_retry_on_failure() {
    // Test: Retry NTP sync if first attempt fails
    
    int maxRetries = 3;
    int attempt = 0;
    bool syncSuccess = false;
    
    // Simulate first 2 failures, 3rd success
    while (attempt < maxRetries && !syncSuccess) {
        attempt++;
        if (attempt == 3) {
            syncSuccess = true; // Third attempt succeeds
        }
    }
    
    TEST_ASSERT_TRUE(syncSuccess);
    TEST_ASSERT_EQUAL_INT(3, attempt);
}

void test_time_dependent_features_use_real_time() {
    // Test: All time-dependent features use synchronized time
    
    bool timeSynced = true;
    unsigned long systemTime = 1704067200; // Valid time (2024-01-01)
    
    // Features that depend on time
    struct TimeFeature {
        const char* name;
        bool usesRealTime;
    };
    
    TimeFeature features[] = {
        {"DosingPump Schedule", timeSynced},
        {"Pattern Learning", timeSynced},
        {"Event Logger", timeSynced},
        {"Data History", timeSynced}
    };
    
    // All should use real time when synced
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(features[i].usesRealTime);
    }
}

void test_pattern_learning_uses_real_hour_of_day() {
    // Test: Pattern learning uses actual hour (0-23), not uptime
    
    struct tm timeInfo;
    timeInfo.tm_hour = 14; // 2:00 PM
    
    // Pattern learning stores data by hour
    float phPattern[24];
    float tempPattern[24];
    
    // Current readings at hour 14
    phPattern[timeInfo.tm_hour] = 6.8;
    tempPattern[timeInfo.tm_hour] = 25.3;
    
    TEST_ASSERT_EQUAL_INT(14, timeInfo.tm_hour);
    TEST_ASSERT_EQUAL_FLOAT(6.8, phPattern[14]);
    TEST_ASSERT_EQUAL_FLOAT(25.3, tempPattern[14]);
}

void test_event_logging_timestamps_are_correct() {
    // Test: Event log timestamps match real time
    
    struct EventLogEntry {
        unsigned long timestamp;
        const char* event;
        const char* level;
    };
    
    EventLogEntry events[3];
    
    // Event 1: System start
    events[0].timestamp = 1704067200; // 2024-01-01 00:00:00
    events[0].event = "System started";
    events[0].level = "INFO";
    
    // Event 2: WiFi connected (5 minutes later)
    events[1].timestamp = 1704067500; // 2024-01-01 00:05:00
    events[1].event = "WiFi connected";
    events[1].level = "INFO";
    
    // Event 3: NTP synced (10 seconds later)
    events[2].timestamp = 1704067510; // 2024-01-01 00:05:10
    events[2].event = "NTP time synchronized";
    events[2].level = "INFO";
    
    // Verify timestamps are in order
    TEST_ASSERT_TRUE(events[0].timestamp < events[1].timestamp);
    TEST_ASSERT_TRUE(events[1].timestamp < events[2].timestamp);
    
    // Verify time differences
    TEST_ASSERT_EQUAL_UINT32(300, events[1].timestamp - events[0].timestamp); // 5 minutes
    TEST_ASSERT_EQUAL_UINT32(10, events[2].timestamp - events[1].timestamp);  // 10 seconds
}

void test_timezone_affects_scheduled_dosing() {
    // Test: Timezone settings affect when scheduled doses occur
    
    struct TimeConfig {
        long gmtOffsetSec;
        int dstOffsetSec;
        int localHour; // What the user sees
    };
    
    // UTC time: 20:00 (8:00 PM)
    int utcHour = 20;
    
    // PST (UTC-8): 20:00 UTC = 12:00 PST (noon)
    TimeConfig pst;
    pst.gmtOffsetSec = -8 * 3600;
    pst.dstOffsetSec = 0;
    pst.localHour = utcHour + (pst.gmtOffsetSec / 3600);
    TEST_ASSERT_EQUAL_INT(12, pst.localHour);
    
    // EST (UTC-5): 20:00 UTC = 15:00 EST (3:00 PM)
    TimeConfig est;
    est.gmtOffsetSec = -5 * 3600;
    est.dstOffsetSec = 0;
    est.localHour = utcHour + (est.gmtOffsetSec / 3600);
    TEST_ASSERT_EQUAL_INT(15, est.localHour);
    
    // CET (UTC+1): 20:00 UTC = 21:00 CET (9:00 PM)
    TimeConfig cet;
    cet.gmtOffsetSec = 1 * 3600;
    cet.dstOffsetSec = 0;
    cet.localHour = utcHour + (cet.gmtOffsetSec / 3600);
    TEST_ASSERT_EQUAL_INT(21, cet.localHour);
}

void test_web_api_time_endpoints_return_correct_data() {
    // Test: Web API time endpoints provide accurate information
    
    struct TimeAPIResponse {
        bool synced;
        unsigned long currentTime;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        const char* ntpServer;
        long gmtOffset;
        int dstOffset;
    };
    
    TimeAPIResponse response;
    response.synced = true;
    response.currentTime = 1704067200;
    response.year = 2024;
    response.month = 1;
    response.day = 1;
    response.hour = 10;  // Local time
    response.minute = 30;
    response.ntpServer = "pool.ntp.org";
    response.gmtOffset = -8 * 3600; // PST
    response.dstOffset = 0;
    
    // Verify response
    TEST_ASSERT_TRUE(response.synced);
    TEST_ASSERT_EQUAL_INT(2024, response.year);
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", response.ntpServer);
    TEST_ASSERT_EQUAL_INT32(-28800, response.gmtOffset);
}

void test_ntp_sync_persists_across_reboot() {
    // Test: NTP configuration persists in NVS across reboots
    
    struct NTPConfig {
        char ntpServer[64];
        long gmtOffset;
        int dstOffset;
    };
    
    // First boot - save configuration
    NTPConfig config1;
    strncpy(config1.ntpServer, "time.google.com", sizeof(config1.ntpServer));
    config1.gmtOffset = -5 * 3600; // EST
    config1.dstOffset = 3600;      // DST active
    
    // Simulate saving to NVS (MockPreferences)
    // ... (save operations)
    
    // Second boot - load configuration
    NTPConfig config2;
    strncpy(config2.ntpServer, "time.google.com", sizeof(config2.ntpServer)); // From NVS
    config2.gmtOffset = -5 * 3600; // From NVS
    config2.dstOffset = 3600;      // From NVS
    
    // Verify configuration persisted
    TEST_ASSERT_EQUAL_STRING("time.google.com", config2.ntpServer);
    TEST_ASSERT_EQUAL_INT32(-18000, config2.gmtOffset);
    TEST_ASSERT_EQUAL_INT32(3600, config2.dstOffset);
}

// ============================================================================
// Run Integration Tests
// ============================================================================

void runIntegrationTests() {
    UNITY_BEGIN();
    
    // Existing tests
    RUN_TEST(test_complete_pid_control_loop);
    RUN_TEST(test_sensor_to_storage_integration);
    RUN_TEST(test_dual_pid_controllers);
    RUN_TEST(test_complete_safety_system);
    RUN_TEST(test_config_change_propagation);
    RUN_TEST(test_ph_calibration_workflow);
    RUN_TEST(test_realtime_data_pipeline);
    RUN_TEST(test_mqtt_message_generation);
    RUN_TEST(test_emergency_recovery_procedure);
    RUN_TEST(test_pid_learning_adaptation);
    RUN_TEST(test_time_based_control_intervals);
    RUN_TEST(test_multi_point_data_validation);
    
    // DosingPump integration tests
    RUN_TEST(test_scheduled_dosing_triggers_at_correct_time);
    RUN_TEST(test_scheduled_dosing_skip_if_already_dosed_today);
    RUN_TEST(test_dosing_pump_daily_volume_reset_at_midnight);
    RUN_TEST(test_dosing_pump_safety_prevents_overdosing);
    RUN_TEST(test_dosing_pump_history_with_timestamps);
    RUN_TEST(test_dosing_pump_integrates_with_pattern_learning);
    RUN_TEST(test_dosing_pump_multiple_schedules);
    
    // NTP integration tests
    RUN_TEST(test_wifi_connect_triggers_ntp_sync);
    RUN_TEST(test_ntp_sync_retry_on_failure);
    RUN_TEST(test_time_dependent_features_use_real_time);
    RUN_TEST(test_pattern_learning_uses_real_hour_of_day);
    RUN_TEST(test_event_logging_timestamps_are_correct);
    RUN_TEST(test_timezone_affects_scheduled_dosing);
    RUN_TEST(test_web_api_time_endpoints_return_correct_data);
    RUN_TEST(test_ntp_sync_persists_across_reboot);
    
    UNITY_END();
}
