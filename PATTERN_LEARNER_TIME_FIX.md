# PatternLearner NTP Time Fix

## Overview
Fixed PatternLearner to use NTP synchronized Unix timestamps instead of boot time (millis) for anomaly logging.

## Problem
The PatternLearner was using `millis() / 1000` for anomaly timestamps, which:
- Resets to zero on every device reboot
- Shows incorrect dates (starts at January 1, 1970 + boot time)
- Makes historical anomaly tracking meaningless across reboots
- Inconsistent with WaterChangeAssistant and DosingPump which use NTP time

## Solution
Changed all anomaly timestamp assignments from `millis() / 1000` to `time(nullptr)` to use NTP synchronized Unix timestamps.

## Files Modified

### src/PatternLearner.cpp
Fixed 4 timestamp-related functions:

#### 1. detectTemperatureAnomaly()
```cpp
// Before:
anomaly.timestamp = millis() / 1000;

// After:
anomaly.timestamp = time(nullptr);  // Use NTP time instead of millis
```

#### 2. detectPHAnomaly()
```cpp
// Before:
anomaly.timestamp = millis() / 1000;

// After:
anomaly.timestamp = time(nullptr);  // Use NTP time instead of millis
```

#### 3. detectTDSAnomaly()
```cpp
// Before:
anomaly.timestamp = millis() / 1000;

// After:
anomaly.timestamp = time(nullptr);  // Use NTP time instead of millis
```

#### 4. predictAnomalyLikelihood()
```cpp
// Before:
unsigned long currentTime = millis() / 1000;

// After:
unsigned long currentTime = time(nullptr);  // Use NTP time instead of millis
```

## Benefits

### 1. Persistent Timestamps
- Anomaly timestamps survive device reboots
- Historical anomaly tracking remains valid across restarts
- Can correlate anomalies with external events using real calendar dates

### 2. Consistency
- All system components now use NTP time:
  - ✅ WaterChangeAssistant (last/next change calculations)
  - ✅ DosingPump (dose history records)
  - ✅ PatternLearner (anomaly timestamps)
  - ✅ EventLogger (event timestamps - already using NTP)

### 3. Accurate Historical Analysis
- Can determine when anomalies actually occurred
- Time-based filtering works correctly (e.g., "last 24 hours")
- Seasonal pattern analysis uses real calendar time

### 4. Web Interface Display
- Anomaly timestamps display as real dates/times
- Users can understand when issues occurred
- Correlate with water changes, maintenance, feeding schedules

## Technical Details

### Struct Definition
The Anomaly struct uses `unsigned long timestamp`:
```cpp
struct Anomaly {
    unsigned long timestamp;  // Unix timestamp (seconds since 1970-01-01)
    String type;
    float actualValue;
    float expectedValue;
    float deviation;
    String severity;
};
```

### NTP Time Source
- `time(nullptr)` returns Unix timestamp from ESP32 RTC
- RTC synchronized with NTP server on WiFi connection
- Automatic DST handling based on timezone configuration
- Falls back gracefully if NTP not yet synchronized (returns 0)

### Time Comparison
The `predictAnomalyLikelihood()` function compares timestamps correctly:
```cpp
unsigned long currentTime = time(nullptr);
unsigned long lookback = 3600 * hoursAhead; // seconds

for (const auto& anomaly : anomalyHistory) {
    if (currentTime - anomaly.timestamp < lookback) {
        recentAnomalies++;
    }
}
```

## Validation

### NTP Check
Before anomaly detection begins, ensure NTP is synchronized:
```cpp
time_t now = time(nullptr);
if (now < 1000000000) {  // If time not set yet
    // Skip anomaly detection or use default behavior
}
```

### Boot Time vs Calendar Time

**Boot Time (millis):**
- ❌ Resets to 0 on every boot
- ❌ No relationship to real time
- ✅ Good for intervals and delays

**Calendar Time (time):**
- ✅ Persists across reboots
- ✅ Real-world timestamps
- ✅ Good for historical logging

## Build Information

**Compilation Status:** ✅ SUCCESS

**Firmware Build:**
- Flash usage: 61.9% (1,217,573 / 1,966,080 bytes)
- RAM usage: 16.9% (55,444 / 327,680 bytes)
- Build time: 13.58 seconds

## Testing Recommendations

1. **Verify NTP Synchronization:**
   - Check device connects to NTP server
   - Verify time displays correctly in web interface
   - Confirm timezone settings are correct

2. **Trigger Anomaly:**
   - Temporarily set water temp very high/low
   - Verify anomaly is logged with correct timestamp
   - Check timestamp shows real date/time, not boot time

3. **Test Across Reboot:**
   - Log anomaly before reboot
   - Restart device
   - Verify anomaly history still shows correct timestamp
   - Confirm time comparisons work correctly

4. **Check Web Interface:**
   - View anomaly history in web UI
   - Verify timestamps display as readable dates
   - Test filtering by time range

## Related Components

### System Time Initialization
Handled in `main.cpp` or `WiFiManager.cpp`:
```cpp
configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
```

### Time Display Format
Web interface should format timestamps:
```javascript
const date = new Date(timestamp * 1000); // Convert to milliseconds
const formatted = date.toLocaleString('en-AU', options);
```

### Anomaly History API
Ensure `/api/patterns/anomalies` returns timestamps correctly:
```json
{
  "anomalies": [
    {
      "timestamp": 1729324800,
      "type": "temperature",
      "actualValue": 28.5,
      "expectedValue": 25.0,
      "deviation": 3.2,
      "severity": "high"
    }
  ]
}
```

## Conclusion

PatternLearner now uses NTP synchronized time consistently with other system components. Anomaly timestamps are now meaningful, persistent across reboots, and provide accurate historical tracking for long-term pattern analysis and debugging.

## Next Steps

1. Upload firmware to ESP32
2. Verify NTP synchronization
3. Test anomaly detection and logging
4. Confirm timestamps display correctly in web interface
5. Validate anomaly history persists across reboots
