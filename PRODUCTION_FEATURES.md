# Production Features Implementation

## Overview
This document describes the production-ready features added to the aquarium controller for enhanced reliability, maintainability, and monitoring.

## Features Implemented

### 1. Hardware Watchdog Timer
**Purpose:** Automatically reset the ESP32 if the system hangs or crashes, ensuring continuous operation.

**Implementation:**
- **File:** `src/main.cpp`
- **Timeout:** 30 seconds
- **Behavior:** If the main loop doesn't execute within 30 seconds, the ESP32 will automatically panic and reset
- **Code Location:**
  - Initialization in `setup()`: `esp_task_wdt_init(WDT_TIMEOUT, true)`
  - Reset in `loop()`: `esp_task_wdt_reset()`

**Usage:**
```cpp
#define WDT_TIMEOUT 30  // 30 second timeout
esp_task_wdt_init(WDT_TIMEOUT, true);  // Enable with panic on timeout
esp_task_wdt_add(NULL);                 // Add current thread
```

### 2. Calibration Age Tracking
**Purpose:** Track pH sensor calibration age and warn when recalibration is needed.

**Implementation:**
- **Files:** `include/PHSensor.h`, `src/PHSensor.cpp`
- **Storage:** NVS (non-volatile storage)
- **Thresholds:**
  - ⚠️ WARNING: 30 days since last calibration
  - ⚠️ EXPIRED: 60 days since last calibration
  
**Features:**
- `getDaysSinceCalibration()` - Returns days since last calibration (999 if never calibrated)
- `needsCalibration()` - Returns true if >= 30 days
- `isCalibrationExpired()` - Returns true if >= 60 days
- `getLastCalibrationTime()` - Returns timestamp of last calibration

**Automatic Alerts:**
- On startup, checks calibration age and sends MQTT alert if needed
- Visual indicators in serial console (⚠️ EXPIRED, ⚠️ WARNING, ✓ OK)

**Note:** Uses `millis()` for timing, so resets on device reboot. For absolute calendar time, an RTC module would be needed.

### 3. Event Logging System
**Purpose:** Comprehensive event logging for debugging, forensics, and monitoring.

**Implementation:**
- **Files:** `include/EventLogger.h`, `src/EventLogger.cpp`
- **Storage:** SPIFFS filesystem at `/events.log`
- **Format:** Pipe-delimited: `timestamp|level|category|message`
- **Max Size:** 1000 lines with automatic rotation (keeps most recent 50%)

**Log Levels:**
- `EVENT_INFO` - Normal operation events
- `EVENT_WARNING` - Potential issues
- `EVENT_ERROR` - Recoverable errors
- `EVENT_CRITICAL` - Critical failures requiring immediate attention

**Methods:**
```cpp
eventLogger->info("category", "message");
eventLogger->warning("category", "message");
eventLogger->error("category", "message");
eventLogger->critical("category", "message");
```

**Retrieval:**
```cpp
eventLogger->getRecentLogs(50);              // Get 50 most recent
eventLogger->getLogsByLevel(EVENT_ERROR);    // Get by level
eventLogger->getLogsByCategory("sensors");   // Get by category
```

**Categories Used:**
- `system` - System startup/shutdown
- `network` - WiFi/MQTT events
- `sensors` - Sensor initialization/errors
- `calibration` - Calibration warnings
- `temperature` - Temperature control events
- `co2` - CO2 control events

### 4. Configuration Backup/Restore
**Purpose:** Export/import system configuration for backup, migration, or recovery.

**Implementation:**
- **Files:** `include/ConfigManager.h`, `src/ConfigManager.cpp`
- **Format:** JSON with all configuration fields
- **Library:** ArduinoJson

**Methods:**
```cpp
String json = configMgr->exportToJSON();     // Export to JSON string
bool success = configMgr->importFromJSON(json);  // Import from JSON string
```

**Configuration Fields Included:**
- WiFi settings (SSID, password)
- MQTT settings (server, port, user, password, client ID, topic prefix)
- Control targets (temperature, pH)
- Safety limits (max temp, min pH)
- Pin assignments (all sensors and relays)

**API Endpoints:**
- `GET /api/config/export` - Download configuration as JSON
- `POST /api/config/import` - Upload and apply configuration

### 5. MQTT Alert System
**Purpose:** Real-time alerting to OpenHAB or other MQTT consumers for critical events.

**Implementation:**
- **Files:** `include/SystemTasks.h`, `src/SystemTasks.cpp`
- **Topic:** `{mqttTopicPrefix}/alert`
- **Retain:** Messages are retained for reliable delivery

**Function:**
```cpp
void sendMQTTAlert(const char* category, const char* message, bool critical = true)
```

**Message Format (JSON):**
```json
{
  "category": "temperature",
  "message": "Temperature emergency stop activated",
  "critical": true,
  "timestamp": 123456
}
```

**Alert Categories:**
- `temperature` - Temperature control issues
- `co2` - CO2/pH control issues
- `calibration` - Calibration warnings
- `sensors` - Sensor failures

**Automatic Triggers:**
- Emergency stop activated (temperature or pH)
- Calibration age warning (30 days)
- Calibration expired (60 days)
- Sensor initialization failures

**Integration:**
- Automatically logs to EventLogger when alert is sent
- Only sends if MQTT is connected
- Critical alerts logged as CRITICAL level, others as ERROR

### 6. Web API Endpoints
**Purpose:** Remote monitoring and management via web interface.

**New Endpoints:**

#### Event Logs
- `GET /api/logs?count=50` - Get recent event logs (default 50)
  - Returns: Array of log entries with timestamp, level, category, message
- `POST /api/logs/clear` - Clear all event logs

#### Configuration
- `GET /api/config/export` - Export configuration as JSON
- `POST /api/config/import` - Import configuration from JSON
  - Validates JSON before applying
  - Logs import event

**Example Usage:**
```bash
# Get logs
curl http://aquarium.local/api/logs?count=100

# Export config
curl http://aquarium.local/api/config/export > config_backup.json

# Import config
curl -X POST -H "Content-Type: application/json" \
  --data @config_backup.json \
  http://aquarium.local/api/config/import

# Clear logs
curl -X POST http://aquarium.local/api/logs/clear
```

## Integration Points

### Startup Sequence (main.cpp)
1. Initialize watchdog timer
2. Initialize configuration manager
3. **Initialize event logger** - Logs startup events
4. Initialize WiFi - Logs network events
5. Initialize sensors - Logs initialization status
6. **Check calibration age** - Sends alerts if needed
7. Initialize PID controllers
8. Initialize relays
9. Start FreeRTOS tasks

### Control Loop Integration
- Emergency stops trigger MQTT alerts
- All critical events logged to EventLogger
- Watchdog reset ensures loop continues

### MQTT Integration
- Alerts published to `{prefix}/alert` topic
- OpenHAB can subscribe to receive real-time notifications
- Retained messages ensure delivery

## File Structure

### New Files
```
include/EventLogger.h         - Event logging class definition
src/EventLogger.cpp           - Event logging implementation
PRODUCTION_FEATURES.md        - This documentation
```

### Modified Files
```
src/main.cpp                  - Watchdog init, EventLogger init, calibration checks
include/PHSensor.h            - Calibration age tracking fields and methods
src/PHSensor.cpp              - Calibration age implementation
include/ConfigManager.h       - Export/import method declarations
src/ConfigManager.cpp         - JSON export/import implementation
include/SystemTasks.h         - EventLogger extern, sendMQTTAlert declaration
src/SystemTasks.cpp           - MQTT alert implementation, integration
src/WebServer.cpp             - New API endpoints for logs and config
```

## Testing Recommendations

### 1. Watchdog Timer
- Test: Comment out `esp_task_wdt_reset()` in loop
- Expected: ESP32 resets after 30 seconds
- Verification: Check serial output for watchdog reset message

### 2. Calibration Age
- Test: Set calibration timestamp to 40 days ago in NVS
- Expected: Warning message on startup, MQTT alert sent
- Verification: Check serial output and MQTT broker

### 3. Event Logger
- Test: Trigger various events (sensor init, emergency stop)
- Expected: Events logged to SPIFFS
- Verification: GET /api/logs and check responses

### 4. MQTT Alerts
- Test: Trigger emergency stop
- Expected: Alert published to MQTT topic
- Verification: Subscribe to `aquarium/alert` in MQTT broker

### 5. Config Backup
- Test: Export config, modify, import
- Expected: Settings restored
- Verification: Check configuration values

## OpenHAB Integration Example

### MQTT Thing Configuration
```
Thing mqtt:topic:aquarium "Aquarium Controller" (mqtt:broker:local) {
    Channels:
        Type string : alert "System Alerts" [ 
            stateTopic="aquarium/alert",
            transformationPattern="JSONPATH:$.message"
        ]
        Type switch : critical_alert "Critical Alert" [
            stateTopic="aquarium/alert",
            transformationPattern="JSONPATH:$.critical",
            on="true",
            off="false"
        ]
}
```

### Items
```
String AquariumAlert "Alert Message" { channel="mqtt:topic:aquarium:alert" }
Switch AquariumCriticalAlert "Critical Alert" { channel="mqtt:topic:aquarium:critical_alert" }
```

### Rule Example
```
rule "Aquarium Critical Alert"
when
    Item AquariumCriticalAlert changed to ON
then
    sendNotification("your@email.com", "Aquarium Critical Alert: " + AquariumAlert.state)
end
```

## Performance Considerations

### Memory Usage
- EventLogger: ~1000 lines × ~80 bytes = ~80KB SPIFFS storage
- ConfigManager: ~1KB JSON export size
- MQTT Alerts: ~512 bytes per message (not stored)

### Processing Overhead
- Watchdog reset: < 1ms per loop iteration
- Event logging: ~5-10ms per log entry (SPIFFS write)
- MQTT alert: ~10-20ms per alert (network operation)

### Storage Management
- EventLogger auto-rotates at 1000 lines
- SPIFFS typically 1-2MB available on ESP32
- Log rotation keeps most recent 500 lines (50%)

## Future Enhancements

### Potential Additions
1. **Real-Time Clock (RTC)** - For absolute calibration timestamps
2. **SD Card Logging** - For longer-term event storage
3. **Web UI for Logs** - Real-time log viewer in web interface
4. **Email Alerts** - Direct email notifications for critical events
5. **Remote Configuration** - Push config updates via MQTT
6. **Log Export** - Download logs as CSV/JSON file
7. **Calibration Reminders** - Scheduled notifications before expiry
8. **System Health Dashboard** - Uptime, error counts, memory usage

### Known Limitations
1. Calibration age uses `millis()` - resets on reboot
2. Event logs cleared on SPIFFS format
3. No log encryption or authentication
4. Fixed log rotation threshold (1000 lines)
5. JSON config export includes passwords in plaintext

## Troubleshooting

### Watchdog Resets Unexpectedly
- Check for long-blocking operations in loop
- Verify all tasks complete within 30 seconds
- Review serial output for timing issues

### Event Logger Not Working
- Check SPIFFS mounted: `SPIFFS.begin()`
- Verify SPIFFS has free space
- Check log file permissions

### MQTT Alerts Not Received
- Verify MQTT client connected
- Check topic subscription in broker
- Verify retained message settings

### Configuration Import Fails
- Validate JSON syntax
- Check all required fields present
- Review error messages in serial output

## Conclusion

These production features significantly enhance the reliability and maintainability of the aquarium controller:

- **Watchdog** prevents system hangs
- **Calibration tracking** maintains measurement accuracy
- **Event logging** enables troubleshooting
- **Config backup** simplifies migration/recovery
- **MQTT alerts** provide real-time notifications

The system is now production-ready with comprehensive monitoring and fail-safe mechanisms.
