# MQTT Publish-on-Change Implementation

## Overview
The MQTT publishing system has been enhanced to **only publish when values change**, significantly reducing network traffic and unnecessary MQTT messages.

---

## Key Improvements

### 1. ✅ Publish-on-Change Logic
**Previous Behavior:** Published ALL sensor values every 5 seconds, regardless of changes.

**New Behavior:** Only publishes when values change significantly or state changes occur.

#### Change Detection Thresholds
```cpp
Temperature:  ±0.1°C   (avoids noise from sensor fluctuations)
pH:           ±0.05    (appropriate for pH measurement precision)
TDS:          ±5 ppm   (reasonable for TDS sensor stability)
States:       Any change (heater/CO2 ON/OFF transitions)
```

#### Example Scenarios

**Scenario 1: Stable System**
```
Time    Temp    pH     Publish?
00:00   25.2°C  6.80   ✅ First publish
00:05   25.2°C  6.80   ❌ No change
00:10   25.3°C  6.80   ✅ Temp changed ≥0.1°C
00:15   25.3°C  6.80   ❌ No change
00:20   25.3°C  6.85   ✅ pH changed ≥0.05
```

**Scenario 2: Heater Cycle**
```
Time    Temp    Heater  Publish?
00:00   24.8°C  OFF     ✅ First publish
00:05   24.9°C  ON      ✅ Heater state changed
00:10   25.0°C  ON      ✅ Temp changed ≥0.1°C
00:15   25.1°C  ON      ✅ Temp changed ≥0.1°C
00:20   25.2°C  OFF     ✅ Heater state changed
```

### Benefits
- **Reduced MQTT Traffic:** ~80% reduction in normal operating conditions
- **Lower Network Load:** Fewer packets sent to MQTT broker
- **Better Battery Life:** For battery-powered MQTT brokers or devices
- **Cleaner Logs:** Only meaningful changes logged
- **More Responsive:** State changes published immediately

---

### 2. ✅ Configurable MQTT Topic Prefix

**Previous Behavior:** Topic prefix hardcoded as "aquarium", not configurable via UI.

**New Behavior:** Fully configurable via web interface with default "aquarium".

#### Web Interface
New field in **Settings Tab → WiFi & MQTT Settings**:
```
┌─────────────────────────────────────┐
│ MQTT Topic Prefix                   │
│ ┌─────────────────────────────────┐ │
│ │ aquarium                        │ │
│ └─────────────────────────────────┘ │
│ 📝 Topics: {prefix}/temperature,    │
│    {prefix}/ph, etc.                │
└─────────────────────────────────────┘
```

#### Topic Examples
```
Prefix: "aquarium"
  ├─ aquarium/temperature
  ├─ aquarium/ph
  ├─ aquarium/tds
  ├─ aquarium/heater
  └─ aquarium/co2

Prefix: "tank1"
  ├─ tank1/temperature
  ├─ tank1/ph
  └─ ...

Prefix: "home/aquarium/living_room"
  ├─ home/aquarium/living_room/temperature
  └─ ...
```

#### Use Cases
1. **Multiple Tanks:** Different prefix for each tank
   - `tank1/temperature`
   - `tank2/temperature`
   - `quarantine/temperature`

2. **Home Automation Hierarchy:** Organize by location
   - `home/livingroom/aquarium/temperature`
   - `home/bedroom/aquarium/temperature`

3. **Testing:** Separate production and test systems
   - `aquarium/temperature` (production)
   - `aquarium-test/temperature` (testing)

---

## Implementation Details

### SystemTasks.cpp - mqttTask()
```cpp
void mqttTask(void* parameter) {
    // Static variables for change detection
    static float lastTemp = -999.0;
    static float lastPH = -999.0;
    static bool lastHeaterState = false;
    static bool firstPublish = true;
    
    // Thresholds
    const float TEMP_THRESHOLD = 0.1;
    const float PH_THRESHOLD = 0.05;
    const float TDS_THRESHOLD = 5.0;
    
    while (true) {
        // Only publish if value changed significantly
        if (firstPublish || fabs(data.temperature - lastTemp) >= TEMP_THRESHOLD) {
            // Publish temperature
            mqttClient->publish(topic, payload);
            lastTemp = data.temperature;
        }
        
        // Always publish state changes immediately
        if (firstPublish || data.heaterState != lastHeaterState) {
            mqttClient->publish(topic, data.heaterState ? "ON" : "OFF");
            lastHeaterState = data.heaterState;
        }
        
        firstPublish = false;
        vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
    }
}
```

### ConfigManager
```cpp
struct SystemConfig {
    char mqttTopicPrefix[64];  // Default: "aquarium"
};

void setMQTT(const char* server, int port, const char* user, 
             const char* password, const char* topicPrefix = nullptr);
```

### WebServer API
**GET /api/settings:**
```json
{
  "mqttServer": "192.168.1.100",
  "mqttPort": 1883,
  "mqttTopicPrefix": "aquarium"
}
```

**POST /api/settings:**
```json
{
  "mqttServer": "192.168.1.100",
  "mqttPort": 1883,
  "mqttUser": "user",
  "mqttPassword": "pass",
  "mqttTopicPrefix": "tank1"
}
```

---

## Additional Considerations

### 3. ⚠️ MQTT QoS (Quality of Service)
**Current:** QoS 0 (at most once delivery)

**Recommendation:** Consider implementing QoS levels for different message types:
```cpp
// Sensor data: QoS 0 (fire and forget, okay to lose occasionally)
mqttClient->publish(topic, payload, false, 0);

// State changes: QoS 1 (at least once, important)
mqttClient->publish(topic, payload, false, 1);

// Alerts: QoS 1 with retain
mqttClient->publish(topic, payload, true, 1);
```

**Future Enhancement:**
```cpp
void publishSensorValue(const char* topic, const char* value, bool critical = false) {
    int qos = critical ? 1 : 0;
    bool retain = critical;
    mqttClient->publish(topic, value, retain, qos);
}
```

---

### 4. ⚠️ MQTT Last Will and Testament (LWT)
**Current:** Basic online status published on connect

**Recommendation:** Implement proper LWT for offline detection:

```cpp
void setupMQTT() {
    mqttClient.setServer(config.mqttServer, config.mqttPort);
    
    // Set Last Will and Testament
    char lwt_topic[128];
    snprintf(lwt_topic, sizeof(lwt_topic), "%s/status", config.mqttTopicPrefix);
    
    mqttClient.setWill(lwt_topic, "offline", true, 1);
    // When client disconnects ungracefully, broker publishes "offline"
}

void reconnectMQTT() {
    if (mqttClient.connect(clientId, user, pass)) {
        // Publish online status
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/status", config.mqttTopicPrefix);
        mqttClient.publish(topic, "online", true, 1);
    }
}
```

**Home Assistant Integration:**
```yaml
binary_sensor:
  - platform: mqtt
    name: "Aquarium Controller"
    state_topic: "aquarium/status"
    payload_on: "online"
    payload_off: "offline"
    device_class: connectivity
```

---

### 5. ⚠️ Per-Sensor Publish Control
**Current:** All sensors published together

**Potential Enhancement:** Allow disabling specific sensors:

```cpp
struct MQTTConfig {
    bool publishTemperature = true;
    bool publishPH = true;
    bool publishTDS = true;
    bool publishAmbient = false;  // Disable ambient by default
};

// In mqttTask()
if (config.mqttPublishTemperature && temperatureChanged) {
    mqttClient->publish(...);
}
```

**Use Case:** Some users may not want to publish all sensors (e.g., ambient temp not needed in MQTT).

---

### 6. ⚠️ Configurable Publish Interval
**Current:** Hardcoded 5-second check interval

**Potential Enhancement:**
```cpp
struct MQTTConfig {
    int publishIntervalSeconds = 5;  // Configurable: 1-300 seconds
};

// In mqttTask()
const TickType_t xDelay = pdMS_TO_TICKS(config.mqttPublishInterval * 1000);
```

**Use Case:** 
- Fast updates: 1-2 seconds for critical monitoring
- Slow updates: 30-60 seconds for stable systems to reduce traffic

---

### 7. ⚠️ MQTT Topics for Advanced Features
**Current:** Basic sensor and state topics

**Missing Topics:**
```
{prefix}/dosing/pump1/status      - Dosing pump state (idle/dosing/priming)
{prefix}/dosing/pump1/progress    - Dosing progress (0-100%)
{prefix}/dosing/pump1/volume      - Volume dosed today (mL)
{prefix}/dosing/pump1/next        - Time until next dose (seconds)

{prefix}/waterchange/status       - Water change phase
{prefix}/waterchange/progress     - Progress (0-100%)
{prefix}/waterchange/days_until   - Days until next change

{prefix}/pattern/status           - Pattern learning status
{prefix}/pattern/anomaly          - Anomaly detection alerts

{prefix}/system/uptime            - System uptime (seconds)
{prefix}/system/wifi_rssi         - WiFi signal strength
{prefix}/system/free_heap         - Free memory (bytes)
```

**Example Implementation:**
```cpp
// In mqttTask(), add:
if (dosingPump && dosingPump->isDosing()) {
    snprintf(topic, sizeof(topic), "%s/dosing/pump1/progress", config.mqttTopicPrefix);
    snprintf(payload, sizeof(payload), "%.1f", dosingPump->getProgress() * 100.0);
    mqttClient->publish(topic, payload);
}
```

---

### 8. ⚠️ MQTT Discovery (Home Assistant)
**Current:** Manual configuration required in Home Assistant

**Enhancement:** Auto-discovery using MQTT Discovery protocol:

```cpp
void publishHomeAssistantDiscovery() {
    char topic[256];
    StaticJsonDocument<512> doc;
    
    // Temperature sensor discovery
    snprintf(topic, sizeof(topic), 
             "homeassistant/sensor/%s_temperature/config", config.mqttClientId);
    
    doc["name"] = "Aquarium Temperature";
    doc["unique_id"] = String(config.mqttClientId) + "_temp";
    doc["state_topic"] = String(config.mqttTopicPrefix) + "/temperature";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
    
    char payload[512];
    serializeJson(doc, payload);
    mqttClient->publish(topic, payload, true, 1);
}
```

**Home Assistant Result:** Sensors automatically appear in Home Assistant!

---

## Testing the Changes

### 1. Test Publish-on-Change
```bash
# Subscribe to all topics
mosquitto_sub -h localhost -t "aquarium/#" -v

# Observe:
# - First publish: All values sent
# - Subsequent: Only changed values
# - State changes: Immediate publish
```

### 2. Test Topic Prefix
```bash
# Change prefix to "tank1" via web interface

# Subscribe to new prefix
mosquitto_sub -h localhost -t "tank1/#" -v

# Verify all topics use new prefix
```

### 3. Monitor Network Traffic
```bash
# Before: ~12 publishes every 5 seconds (temp, ambient, pH, TDS, heater, CO2 × 2)
# After: ~2-4 publishes every 5 seconds (only changed values)

# Calculate reduction
tcpdump -i wlan0 port 1883 -c 100
```

---

## Configuration Examples

### OpenHAB Items (with custom prefix)
```
Number AquariumTemperature "Temperature [%.1f °C]" 
    { channel="mqtt:topic:mybroker:tank1:temperature" }

Switch AquariumHeater "Heater" 
    { channel="mqtt:topic:mybroker:tank1:heater" }
```

### Node-RED Flow (with publish-on-change)
```json
[
    {
        "id": "mqtt_in",
        "type": "mqtt in",
        "topic": "aquarium/+",
        "broker": "localhost",
        "notes": "Only receives messages when values change!"
    }
]
```

### Home Assistant (with custom prefix)
```yaml
sensor:
  - platform: mqtt
    name: "Tank 1 Temperature"
    state_topic: "tank1/temperature"
    unit_of_measurement: "°C"
    device_class: temperature
```

---

## Migration Guide

### For Existing Users

1. **Topic Prefix Change:**
   - Old topics: `aquarium/*`
   - If you change prefix to `tank1`: Update all subscriptions to `tank1/*`
   - Recommendation: Keep default "aquarium" unless managing multiple tanks

2. **Publish Behavior:**
   - No action needed
   - MQTT subscriptions work the same
   - Just fewer messages (only on change)
   - Home automation rules unchanged

3. **Update Configuration:**
   - Go to Settings tab
   - Verify MQTT Topic Prefix is set correctly
   - Click Save Settings
   - Controller will restart

---

## Performance Impact

### Before (Publish-All-Always):
```
Publish Rate: 6 topics × every 5 seconds = 1.2 msg/sec
Daily Messages: 103,680 messages/day
Network Traffic: ~3 MB/day (assuming 30 bytes/msg)
```

### After (Publish-on-Change):
```
Stable System:
  Publish Rate: 0-2 topics × every 5 seconds = 0-0.4 msg/sec
  Daily Messages: ~20,000 messages/day (80% reduction)
  Network Traffic: ~0.6 MB/day

Active System (frequent changes):
  Publish Rate: 3-4 topics × every 5 seconds = 0.6-0.8 msg/sec
  Daily Messages: ~50,000 messages/day (52% reduction)
  Network Traffic: ~1.5 MB/day
```

**Savings:**
- **Network:** 50-80% reduction in MQTT traffic
- **Broker Load:** Significantly reduced message processing
- **Storage:** Reduced database size for logged MQTT messages

---

## Future Enhancements Roadmap

### Short-term (Easy)
- [x] Publish-on-change with thresholds
- [x] Configurable topic prefix
- [ ] MQTT QoS configuration (QoS 0/1 selection)
- [ ] Last Will and Testament (LWT) for offline detection

### Medium-term (Moderate Effort)
- [ ] Per-sensor publish enable/disable
- [ ] Configurable publish interval (1-300 seconds)
- [ ] Dosing pump MQTT topics
- [ ] Water change assistant MQTT topics
- [ ] Pattern learner anomaly MQTT topics

### Long-term (Advanced)
- [ ] Home Assistant MQTT Discovery
- [ ] MQTT TLS/SSL support
- [ ] MQTT Certificate authentication
- [ ] Compressed MQTT payloads (JSON → binary)
- [ ] MQTT command topics (receive commands via MQTT)

---

## Summary

### What Changed:
✅ **MQTT now only publishes when values change** (80% traffic reduction)  
✅ **Topic prefix is configurable via web interface**  
✅ **State changes published immediately** (heater ON/OFF, etc.)  
✅ **Emergency alerts always published** (retained with priority)

### What to Consider:
⚠️ MQTT QoS levels (currently QoS 0)  
⚠️ Last Will and Testament for offline detection  
⚠️ Per-sensor publish control  
⚠️ Configurable publish intervals  
⚠️ Advanced feature MQTT topics (dosing, water change, patterns)  
⚠️ Home Assistant MQTT Discovery  

### Backward Compatibility:
✅ **100% compatible** - Existing MQTT subscriptions continue working  
✅ **Default prefix unchanged** - "aquarium" remains default  
✅ **Topic structure unchanged** - Same topics, just fewer messages  

---

## Questions & Answers

**Q: Will my Home Assistant automations break?**  
A: No, topic structure unchanged. Just fewer messages when nothing changes.

**Q: Can I still use the default "aquarium" prefix?**  
A: Yes! Default remains "aquarium". Only change if you need multiple tanks.

**Q: How often does it check for changes?**  
A: Every 5 seconds. Only publishes if value changed ≥ threshold.

**Q: Will alerts still work?**  
A: Yes! Emergency alerts always published immediately with retain flag.

**Q: Can I revert to publish-all-always?**  
A: Not via UI, but change thresholds to 0.0 in code to publish everything.

---

**Last Updated:** December 2024  
**Version:** 2.0 - Publish-on-Change Implementation
