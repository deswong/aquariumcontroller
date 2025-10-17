# NTP Time Synchronization - Feature Documentation

## Overview

The ESP32 Aquarium Controller now includes automatic time synchronization via NTP (Network Time Protocol). This ensures accurate timestamps for:
- **Dosing pump schedules** - Execute fertilizer doses at exact times
- **Pattern learning** - Learn hourly patterns based on real time
- **Event logging** - Accurate timestamps for all system events
- **History tracking** - Correct date/time for dose history

## How It Works

### Automatic Synchronization

1. **On WiFi Connect**: When the ESP32 connects to WiFi, it automatically synchronizes time with the configured NTP server
2. **On Reconnect**: After any WiFi disconnection/reconnection, time is re-synchronized
3. **Timezone Support**: Configurable GMT offset and Daylight Saving Time (DST) offset

### Configuration

#### Default Settings
- **NTP Server**: `pool.ntp.org` (global NTP pool)
- **GMT Offset**: 0 seconds (UTC timezone)
- **DST Offset**: 0 seconds (no daylight saving)

#### Timezone Examples

| Location | GMT Offset | DST Offset | Total Offset |
|----------|------------|------------|--------------|
| UTC/GMT | 0 | 0 | UTC+0 |
| US Eastern (EST) | -18000 (-5h) | 0 | UTC-5 |
| US Eastern (EDT) | -18000 (-5h) | 3600 (+1h) | UTC-4 |
| US Pacific (PST) | -28800 (-8h) | 0 | UTC-8 |
| US Pacific (PDT) | -28800 (-8h) | 3600 (+1h) | UTC-7 |
| Central Europe (CET) | 3600 (+1h) | 0 | UTC+1 |
| India (IST) | 19800 (+5.5h) | 0 | UTC+5:30 |
| Japan (JST) | 32400 (+9h) | 0 | UTC+9 |
| Australia (AEST) | 36000 (+10h) | 0 | UTC+10 |

## Web Interface

### Settings Tab

The **Settings** tab now includes a "⏰ Time & NTP Settings" section:

**Current Time Status**:
- Shows if time is synced (✅ Synced / ⚠️ Not Synced)
- Displays current local time

**Configuration**:
- **NTP Server**: Select NTP server (default: pool.ntp.org)
- **GMT Offset**: Choose your timezone from dropdown
- **Daylight Saving Time**: Enable DST if applicable

**Actions**:
- **Save Settings**: Saves configuration and triggers time sync
- **Sync Time Now**: Immediately re-synchronizes time with NTP

### Time Sync Indicator

The interface shows time synchronization status with color coding:
- 🟢 **Green "✅ Synced"**: Time successfully synchronized
- 🟠 **Orange "⚠️ Not Synced"**: Time not yet synchronized (waiting for NTP)

## REST API

### GET /api/time/status

Get current time synchronization status.

**Response**:
```json
{
  "synced": true,
  "localTime": "2024-10-17 14:23:45",
  "unixTime": 1697551425
}
```

**Fields**:
- `synced`: Boolean indicating if time is synchronized
- `localTime`: Human-readable local time string
- `unixTime`: Unix timestamp (seconds since Jan 1, 1970)

### POST /api/time/config

Configure NTP settings and trigger time synchronization.

**Request**:
```json
{
  "ntpServer": "pool.ntp.org",
  "gmtOffset": -18000,
  "dstOffset": 3600
}
```

**Parameters**:
- `ntpServer`: NTP server hostname or IP
- `gmtOffset`: GMT offset in seconds (negative for west, positive for east)
- `dstOffset`: DST offset in seconds (0 or 3600)

**Response**:
```json
{
  "status": "ok"
}
```

## Code Integration

### Configuration Storage

NTP settings stored in NVS (Non-Volatile Storage):
```cpp
struct SystemConfig {
    char ntpServer[64];      // NTP server hostname
    int gmtOffsetSec;        // GMT offset in seconds
    int daylightOffsetSec;   // DST offset in seconds
};
```

### WiFi Manager Integration

Time synchronization is handled automatically by `WiFiManager`:

```cpp
// Called automatically on WiFi connect
void WiFiManager::syncTime() {
    SystemConfig& cfg = config->getConfig();
    configTime(cfg.gmtOffsetSec, cfg.daylightOffsetSec, cfg.ntpServer);
    
    // Wait for time sync (up to 10 seconds)
    // Sets timeSynced flag when successful
}
```

### Using Time in Your Code

After time is synchronized, use standard C time functions:

```cpp
#include <time.h>

time_t now;
struct tm timeinfo;
time(&now);
localtime_r(&now, &timeinfo);

// Access time components
int hour = timeinfo.tm_hour;    // 0-23
int minute = timeinfo.tm_min;   // 0-59
int second = timeinfo.tm_sec;   // 0-59
int day = timeinfo.tm_mday;     // 1-31
int month = timeinfo.tm_mon;    // 0-11 (add 1 for display)
int year = timeinfo.tm_year;    // Years since 1900 (add 1900)
```

## Troubleshooting

### Time Not Syncing

**Symptoms**: Time shows "Not Synced" or incorrect time

**Checks**:
1. Verify WiFi is connected
2. Check NTP server is reachable
3. Ensure firewall allows NTP (UDP port 123)
4. Try different NTP server (time.google.com, time.nist.gov)

**Solution via Serial Monitor**:
```
Synchronizing time with NTP server...
NTP Server: pool.ntp.org
GMT Offset: -5 hours
DST Offset: +1 hours
Waiting for NTP sync........... Success!
Current time: 2024-10-17 14:23:45
```

### Wrong Timezone

**Symptoms**: Time is correct but shows wrong hour

**Solution**: Adjust GMT offset in settings
- Find your timezone offset (e.g., US Eastern = UTC-5 = -18000 seconds)
- Add DST offset if currently observing daylight saving time
- Save settings and sync time

### Schedules Not Triggering

**Symptoms**: Dosing pump schedule doesn't execute

**Checks**:
1. Verify time is synced: `GET /api/time/status`
2. Check schedule configuration
3. Verify timezone is correct for your location

## NTP Server Options

### Public NTP Servers

**Global Pools**:
- `pool.ntp.org` - Global NTP pool (recommended)
- `time.nist.gov` - NIST (US)
- `time.google.com` - Google
- `time.cloudflare.com` - Cloudflare

**Regional Pools**:
- `us.pool.ntp.org` - United States
- `europe.pool.ntp.org` - Europe
- `asia.pool.ntp.org` - Asia
- `oceania.pool.ntp.org` - Oceania

**Country-Specific**:
- `0.north-america.pool.ntp.org`
- `0.europe.pool.ntp.org`
- `0.asia.pool.ntp.org`

## Technical Details

### Time Synchronization Process

1. **WiFi Connection**: ESP32 connects to WiFi network
2. **NTP Request**: Sends UDP packet to NTP server on port 123
3. **NTP Response**: Receives current time from server
4. **Time Adjustment**: Sets ESP32 internal clock
5. **Verification**: Checks if year > 2020 (valid time)
6. **Status Update**: Sets `timeSynced` flag to true

### Accuracy

- **Initial Sync**: Within 1-2 seconds of actual time
- **Drift**: ESP32 clock may drift ~1 second per day
- **Re-sync**: Automatic on WiFi reconnection

### Power Loss Behavior

When power is lost:
- Time is **NOT** preserved (no RTC battery backup)
- On reboot, time starts at Unix epoch (Jan 1, 1970)
- Automatically re-syncs when WiFi reconnects
- Schedules won't execute until time is synchronized

## Integration Examples

### Check Time Sync Before Scheduling

```cpp
if (wifiMgr->isTimeSynced()) {
    // Safe to use time-based features
    dosingPump->setSchedule(scheduleConfig);
} else {
    Serial.println("WARNING: Time not synced, schedule may not work");
}
```

### Display Current Time

```cpp
String currentTime = wifiMgr->getLocalTime();
Serial.println("Current time: " + currentTime);
```

### Manual Time Sync Trigger

```cpp
// Trigger time re-sync
if (wifiMgr->isConnected()) {
    wifiMgr->syncTime();
}
```

## Benefits of NTP Time Sync

### For Dosing Pump
- ✅ Execute fertilizer doses at exact scheduled times
- ✅ Daily/weekly schedules work accurately
- ✅ Calculate "next dose in X hours" correctly
- ✅ Accurate dose history timestamps

### For Pattern Learning
- ✅ Learn hourly patterns based on real time of day
- ✅ Accurate 24-hour cycle tracking
- ✅ Seasonal detection based on correct dates
- ✅ Anomaly timestamps for debugging

### For Event Logging
- ✅ All events have accurate timestamps
- ✅ Easy to correlate events with external systems
- ✅ Debugging with correct time references
- ✅ Historical analysis with real dates/times

## Configuration via Code

### Setting NTP Configuration

```cpp
// Set custom NTP server and timezone
configMgr->setNTP(
    "time.google.com",  // NTP server
    -18000,             // GMT offset (-5 hours for US Eastern)
    3600                // DST offset (+1 hour)
);
```

### Checking Time Sync Status

```cpp
if (wifiMgr->isTimeSynced()) {
    Serial.println("Time is synchronized");
    String localTime = wifiMgr->getLocalTime();
    Serial.println("Current time: " + localTime);
} else {
    Serial.println("Time not synced yet");
}
```

## Web Interface Screenshots

### Time Settings Section
```
⏰ Time & NTP Settings

Current Time Status
┌────────────────────────────┬─────────────────────────────┐
│ Time Sync Status           │ Current Time                │
│ ✅ Synced                  │ 2024-10-17 14:23:45         │
└────────────────────────────┴─────────────────────────────┘

NTP Server: [pool.ntp.org                          ]
GMT Offset: [UTC-5 (EST)                           ▼]
Daylight Saving Time: [+1 hour                     ▼]

[Save Settings] [Sync Time Now] [Restart Device]
```

## Version History

### v1.0.0 - Initial Release
- Automatic NTP synchronization on WiFi connect
- Configurable NTP server, GMT offset, DST offset
- Web interface for time configuration
- REST API for time status and configuration
- NVS persistence of time settings
- Time sync status indicator

---

## Summary

NTP time synchronization is now a core feature of the ESP32 Aquarium Controller. It ensures:

1. ⏰ **Accurate scheduling** for dosing pump and other timed operations
2. 📊 **Correct timestamps** for pattern learning and event logging  
3. 🌍 **Timezone support** with configurable GMT and DST offsets
4. 🔄 **Automatic synchronization** on WiFi connect and reconnect
5. 🌐 **Web interface** for easy configuration and monitoring

This feature is essential for the dosing pump schedule feature and improves the overall reliability and accuracy of all time-dependent features in the system.

---

**Feature Version**: 1.0.0  
**Integration**: WiFiManager, ConfigManager, WebServer  
**Status**: Complete and ready to use
