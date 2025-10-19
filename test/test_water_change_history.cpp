#include <unity.h>
#include "test_common.h"
#include <map>
#include <vector>
#include <time.h>

// Mock NVS storage
static std::map<std::string, uint32_t> mockNVS_uint32;
static std::map<std::string, float> mockNVS_float;

// Water change record structure
struct WaterChangeRecord {
    time_t timestamp;
    float volume;
    float tempBefore;
    float tempAfter;
    float phBefore;
    float phAfter;
    float tdsBefore;
    float tdsAfter;
    uint32_t duration;
    bool completed;
    
    WaterChangeRecord() : timestamp(0), volume(0), tempBefore(0), tempAfter(0),
                         phBefore(0), phAfter(0), tdsBefore(0), tdsAfter(0),
                         duration(0), completed(false) {}
};

// Mock WaterChangeHistory class
class MockWaterChangeHistory {
private:
    static const int MAX_HISTORY = 10;
    std::vector<WaterChangeRecord> records;
    
public:
    MockWaterChangeHistory() {
        records.reserve(MAX_HISTORY);
    }
    
    bool addRecord(const WaterChangeRecord& record) {
        if (records.size() >= MAX_HISTORY) {
            records.erase(records.begin()); // Remove oldest
        }
        records.push_back(record);
        return saveToNVS();
    }
    
    bool saveToNVS() {
        for (size_t i = 0; i < records.size(); i++) {
            std::string prefix = "wc_" + std::to_string(i) + "_";
            mockNVS_uint32[prefix + "ts"] = records[i].timestamp;
            mockNVS_float[prefix + "vol"] = records[i].volume;
            mockNVS_float[prefix + "tb"] = records[i].tempBefore;
            mockNVS_float[prefix + "ta"] = records[i].tempAfter;
            mockNVS_float[prefix + "pb"] = records[i].phBefore;
            mockNVS_float[prefix + "pa"] = records[i].phAfter;
            mockNVS_float[prefix + "tdsb"] = records[i].tdsBefore;
            mockNVS_float[prefix + "tdsa"] = records[i].tdsAfter;
            mockNVS_uint32[prefix + "dur"] = records[i].duration;
            mockNVS_uint32[prefix + "ok"] = records[i].completed ? 1 : 0;
        }
        mockNVS_uint32["wc_count"] = records.size();
        return true;
    }
    
    bool loadFromNVS() {
        records.clear();
        if (mockNVS_uint32.find("wc_count") == mockNVS_uint32.end()) {
            return false;
        }
        
        size_t count = mockNVS_uint32["wc_count"];
        for (size_t i = 0; i < count && i < MAX_HISTORY; i++) {
            std::string prefix = "wc_" + std::to_string(i) + "_";
            WaterChangeRecord record;
            
            if (mockNVS_uint32.find(prefix + "ts") != mockNVS_uint32.end()) {
                record.timestamp = mockNVS_uint32[prefix + "ts"];
                record.volume = mockNVS_float[prefix + "vol"];
                record.tempBefore = mockNVS_float[prefix + "tb"];
                record.tempAfter = mockNVS_float[prefix + "ta"];
                record.phBefore = mockNVS_float[prefix + "pb"];
                record.phAfter = mockNVS_float[prefix + "pa"];
                record.tdsBefore = mockNVS_float[prefix + "tdsb"];
                record.tdsAfter = mockNVS_float[prefix + "tdsa"];
                record.duration = mockNVS_uint32[prefix + "dur"];
                record.completed = (mockNVS_uint32[prefix + "ok"] == 1);
                
                records.push_back(record);
            }
        }
        return true;
    }
    
    size_t getRecordCount() const {
        return records.size();
    }
    
    const WaterChangeRecord* getRecord(size_t index) const {
        if (index < records.size()) {
            return &records[index];
        }
        return nullptr;
    }
    
    const WaterChangeRecord* getLatestRecord() const {
        if (records.empty()) {
            return nullptr;
        }
        return &records.back();
    }
    
    float getAverageTDSBefore(size_t count = 5) const {
        if (records.empty()) return 0.0;
        
        size_t n = (count < records.size()) ? count : records.size();
        float sum = 0.0;
        
        for (size_t i = records.size() - n; i < records.size(); i++) {
            sum += records[i].tdsBefore;
        }
        
        return sum / n;
    }
    
    float getAverageTDSAfter(size_t count = 5) const {
        if (records.empty()) return 0.0;
        
        size_t n = (count < records.size()) ? count : records.size();
        float sum = 0.0;
        
        for (size_t i = records.size() - n; i < records.size(); i++) {
            sum += records[i].tdsAfter;
        }
        
        return sum / n;
    }
    
    void clear() {
        records.clear();
    }
};

// Test functions
void test_water_change_record_basic() {
    WaterChangeRecord record;
    
    // Test default values
    TEST_ASSERT_EQUAL_UINT32(0, record.timestamp);
    TEST_ASSERT_EQUAL_FLOAT(0.0, record.volume);
    TEST_ASSERT_EQUAL_FLOAT(0.0, record.tdsBefore);
    TEST_ASSERT_EQUAL_FLOAT(0.0, record.tdsAfter);
    TEST_ASSERT_FALSE(record.completed);
}

void test_water_change_add_record() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Create a record
    WaterChangeRecord record;
    record.timestamp = 1705320000; // 2024-01-15 12:00:00 UTC
    record.volume = 50.0;
    record.tempBefore = 25.0;
    record.tempAfter = 24.5;
    record.phBefore = 7.2;
    record.phAfter = 7.0;
    record.tdsBefore = 350.0;
    record.tdsAfter = 280.0;
    record.duration = 1800; // 30 minutes
    record.completed = true;
    
    // Add record
    TEST_ASSERT_TRUE(history.addRecord(record));
    TEST_ASSERT_EQUAL_size_t(1, history.getRecordCount());
}

void test_water_change_tds_tracking() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Add record with TDS values
    WaterChangeRecord record;
    record.timestamp = 1705320000;
    record.volume = 50.0;
    record.tdsBefore = 400.0;
    record.tdsAfter = 300.0;
    record.completed = true;
    
    history.addRecord(record);
    
    // Retrieve and verify
    const WaterChangeRecord* retrieved = history.getLatestRecord();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_FLOAT(400.0, retrieved->tdsBefore);
    TEST_ASSERT_EQUAL_FLOAT(300.0, retrieved->tdsAfter);
}

void test_water_change_history_persistence() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    // Create history and add records
    MockWaterChangeHistory history1;
    
    WaterChangeRecord record1;
    record1.timestamp = 1705320000;
    record1.volume = 50.0;
    record1.tdsBefore = 380.0;
    record1.tdsAfter = 290.0;
    record1.completed = true;
    
    WaterChangeRecord record2;
    record2.timestamp = 1705925600; // 7 days later
    record2.volume = 55.0;
    record2.tdsBefore = 410.0;
    record2.tdsAfter = 305.0;
    record2.completed = true;
    
    history1.addRecord(record1);
    history1.addRecord(record2);
    
    // Create new history and load
    MockWaterChangeHistory history2;
    TEST_ASSERT_TRUE(history2.loadFromNVS());
    TEST_ASSERT_EQUAL_size_t(2, history2.getRecordCount());
    
    // Verify first record
    const WaterChangeRecord* loaded1 = history2.getRecord(0);
    TEST_ASSERT_NOT_NULL(loaded1);
    TEST_ASSERT_EQUAL_UINT32(1705320000, loaded1->timestamp);
    TEST_ASSERT_EQUAL_FLOAT(50.0, loaded1->volume);
    TEST_ASSERT_EQUAL_FLOAT(380.0, loaded1->tdsBefore);
    TEST_ASSERT_EQUAL_FLOAT(290.0, loaded1->tdsAfter);
    
    // Verify second record
    const WaterChangeRecord* loaded2 = history2.getRecord(1);
    TEST_ASSERT_NOT_NULL(loaded2);
    TEST_ASSERT_EQUAL_UINT32(1705925600, loaded2->timestamp);
    TEST_ASSERT_EQUAL_FLOAT(55.0, loaded2->volume);
    TEST_ASSERT_EQUAL_FLOAT(410.0, loaded2->tdsBefore);
    TEST_ASSERT_EQUAL_FLOAT(305.0, loaded2->tdsAfter);
}

void test_water_change_average_tds() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Add 5 records with varying TDS
    float tdsBeforeValues[] = {350.0, 380.0, 370.0, 390.0, 360.0};
    float tdsAfterValues[] = {280.0, 290.0, 285.0, 295.0, 275.0};
    
    for (int i = 0; i < 5; i++) {
        WaterChangeRecord record;
        record.timestamp = 1705320000 + (i * 604800); // Weekly
        record.volume = 50.0;
        record.tdsBefore = tdsBeforeValues[i];
        record.tdsAfter = tdsAfterValues[i];
        record.completed = true;
        history.addRecord(record);
    }
    
    // Calculate expected averages
    float expectedAvgBefore = (350.0 + 380.0 + 370.0 + 390.0 + 360.0) / 5.0; // 370.0
    float expectedAvgAfter = (280.0 + 290.0 + 285.0 + 295.0 + 275.0) / 5.0; // 285.0
    
    TEST_ASSERT_EQUAL_FLOAT(expectedAvgBefore, history.getAverageTDSBefore(5));
    TEST_ASSERT_EQUAL_FLOAT(expectedAvgAfter, history.getAverageTDSAfter(5));
}

void test_water_change_max_history() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Add more than max records (11 records, max is 10)
    for (int i = 0; i < 11; i++) {
        WaterChangeRecord record;
        record.timestamp = 1705320000 + (i * 604800);
        record.volume = 50.0 + i;
        record.tdsBefore = 350.0 + (i * 10);
        record.tdsAfter = 280.0 + (i * 5);
        record.completed = true;
        history.addRecord(record);
    }
    
    // Should only keep 10 records (oldest dropped)
    TEST_ASSERT_EQUAL_size_t(10, history.getRecordCount());
    
    // First record should now be the second one we added
    const WaterChangeRecord* first = history.getRecord(0);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_FLOAT(51.0, first->volume); // volume was 50.0 + 1
}

void test_water_change_incomplete_record() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Add incomplete record (change was interrupted)
    WaterChangeRecord record;
    record.timestamp = 1705320000;
    record.volume = 25.0; // Only half completed
    record.tdsBefore = 380.0;
    record.tdsAfter = 0.0; // Not measured yet
    record.completed = false;
    
    history.addRecord(record);
    
    // Verify
    const WaterChangeRecord* retrieved = history.getLatestRecord();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_FALSE(retrieved->completed);
    TEST_ASSERT_EQUAL_FLOAT(0.0, retrieved->tdsAfter);
}

void test_water_change_tds_reduction() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Add record with significant TDS reduction
    WaterChangeRecord record;
    record.timestamp = 1705320000;
    record.volume = 50.0;
    record.tdsBefore = 450.0;
    record.tdsAfter = 320.0;
    record.completed = true;
    
    history.addRecord(record);
    
    // Calculate reduction
    const WaterChangeRecord* retrieved = history.getLatestRecord();
    TEST_ASSERT_NOT_NULL(retrieved);
    
    float reduction = retrieved->tdsBefore - retrieved->tdsAfter;
    float reductionPercent = (reduction / retrieved->tdsBefore) * 100.0;
    
    TEST_ASSERT_EQUAL_FLOAT(130.0, reduction);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 28.89, reductionPercent); // ~28.89% reduction
}

void test_water_change_edge_cases() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Test zero TDS (RO water change)
    WaterChangeRecord record1;
    record1.tdsBefore = 350.0;
    record1.tdsAfter = 0.0; // Pure RO water
    record1.completed = true;
    history.addRecord(record1);
    
    // Test very high TDS
    WaterChangeRecord record2;
    record2.tdsBefore = 1500.0; // Very high
    record2.tdsAfter = 800.0;
    record2.completed = true;
    history.addRecord(record2);
    
    TEST_ASSERT_EQUAL_size_t(2, history.getRecordCount());
    
    const WaterChangeRecord* r1 = history.getRecord(0);
    const WaterChangeRecord* r2 = history.getRecord(1);
    
    TEST_ASSERT_EQUAL_FLOAT(0.0, r1->tdsAfter);
    TEST_ASSERT_EQUAL_FLOAT(1500.0, r2->tdsBefore);
}

void test_water_change_timestamp_validation() {
    mockNVS_uint32.clear();
    mockNVS_float.clear();
    
    MockWaterChangeHistory history;
    
    // Add record with valid timestamp (2024-01-15)
    WaterChangeRecord validRecord;
    validRecord.timestamp = 1705320000;
    validRecord.volume = 50.0;
    validRecord.tdsBefore = 380.0;
    validRecord.tdsAfter = 290.0;
    validRecord.completed = true;
    
    history.addRecord(validRecord);
    
    // Verify timestamp is stored correctly
    const WaterChangeRecord* retrieved = history.getLatestRecord();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_UINT32(1705320000, retrieved->timestamp);
    
    // Timestamp should be > year 2020
    // 2020-01-01 00:00:00 UTC = 1577836800
    TEST_ASSERT_GREATER_THAN(1577836800, retrieved->timestamp);
}

void setUp(void) {
    commonSetUp();
    mockNVS_uint32.clear();
    mockNVS_float.clear();
}

void tearDown(void) {
    commonTearDown();
}

// Arduino setup function for ESP32 testing
void setup(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_water_change_record_basic);
    RUN_TEST(test_water_change_add_record);
    RUN_TEST(test_water_change_tds_tracking);
    RUN_TEST(test_water_change_history_persistence);
    RUN_TEST(test_water_change_average_tds);
    RUN_TEST(test_water_change_max_history);
    RUN_TEST(test_water_change_incomplete_record);
    RUN_TEST(test_water_change_tds_reduction);
    RUN_TEST(test_water_change_edge_cases);
    RUN_TEST(test_water_change_timestamp_validation);
    
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
    
    RUN_TEST(test_water_change_record_basic);
    RUN_TEST(test_water_change_add_record);
    RUN_TEST(test_water_change_tds_tracking);
    RUN_TEST(test_water_change_history_persistence);
    RUN_TEST(test_water_change_average_tds);
    RUN_TEST(test_water_change_max_history);
    RUN_TEST(test_water_change_incomplete_record);
    RUN_TEST(test_water_change_tds_reduction);
    RUN_TEST(test_water_change_edge_cases);
    RUN_TEST(test_water_change_timestamp_validation);
    
    return UNITY_END();
}
#endif
