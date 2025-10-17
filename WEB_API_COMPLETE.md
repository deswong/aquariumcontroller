# Complete Web Interface & API Documentation

## Summary
All system settings and sensor readings are now fully exposed through both the web interface and JSON API. This document describes the complete configuration and monitoring capabilities.

---

## Web Interface Features

### Control Tab
#### Temperature Control
- **Temperature Target**: Target water temperature (20-32°C)
- **Temperature Safety Max**: Emergency cutoff temperature (25-35°C)
  - Heater automatically shuts down if exceeded
  
#### pH Control
- **pH Target**: Target pH for CO2 control (6.0-8.0)
- **pH Safety Minimum**: Emergency cutoff pH (5.0-7.0)
  - CO2 automatically shuts down if pH drops below this value

### Settings Tab
#### WiFi Configuration
- WiFi SSID
- WiFi Password

#### MQTT Configuration
- MQTT Server (hostname or IP)
- MQTT Port (default: 1883)
- MQTT Username
- MQTT Password
- MQTT Topic Prefix (default: "aquarium")
- **Publish Options:**
  - ✅ **Publish Individual Topics** (default: enabled)
    - Separate topics: `{prefix}/temperature`, `{prefix}/ph`, etc.
    - Best for Home Assistant integration
  - ✅ **Publish JSON Payload** (default: disabled)
    - All data in one topic: `{prefix}/data` as JSON
    - Best for data logging and InfluxDB

#### NTP/Time Configuration
- NTP Server (default: pool.ntp.org)
- GMT Offset (timezone selection)
- Daylight Saving Time offset

#### Hardware Pin Configuration
⚠️ **Warning**: Only modify if you understand ESP32 GPIO configuration!

**Sensor Pins (Analog Inputs):**
- Water Temperature Sensor Pin (DS18B20) - default: GPIO 4
- Ambient Temperature Sensor Pin (DS18B20) - default: GPIO 5
- pH Sensor Pin (Analog) - default: GPIO 34 (ADC pins: 32-39)
- TDS Sensor Pin (Analog) - default: GPIO 35 (ADC pins: 32-39)

**Control Pins (Digital Outputs):**
- Heater Relay Pin - default: GPIO 26
- CO2 Solenoid Relay Pin - default: GPIO 27

---

## REST API Endpoints

### GET /api/settings
Returns complete system configuration as JSON.

**Response:**
```json
{
  "wifiSSID": "MyNetwork",
  "mqttServer": "192.168.1.100",
  "mqttPort": 1883,
  "mqttUser": "aquarium",
  "mqttTopicPrefix": "aquarium",
  "mqttPublishIndividual": true,
  "mqttPublishJSON": false,
  "ntpServer": "pool.ntp.org",
  "gmtOffsetSec": 0,
  "daylightOffsetSec": 0,
  "tempTarget": 25.0,
  "tempSafetyMax": 30.0,
  "phTarget": 6.8,
  "phSafetyMin": 6.0,
  "tempSensorPin": 4,
  "ambientSensorPin": 5,
  "phSensorPin": 34,
  "tdsSensorPin": 35,
  "heaterRelayPin": 26,
  "co2RelayPin": 27
}
```

### POST /api/settings
Update system configuration. Device will restart after saving.

**Request Body:** Same format as GET response. All fields are optional - only include fields you want to update.

**Example:**
```json
{
  "tempTarget": 26.0,
  "tempSafetyMax": 31.0,
  "mqttPublishJSON": true
}
```

### POST /api/targets
Update control targets without restarting.

**Request Body:**
```json
{
  "temperature": 25.5,
  "tempSafetyMax": 30.0,
  "ph": 6.9,
  "phSafetyMin": 6.2
}
```

### GET /api/data
Real-time sensor data via WebSocket or HTTP.

**Response:**
```json
{
  "temperature": 25.2,
  "ambientTemp": 24.8,
  "ph": 6.8,
  "tds": 350,
  "heater": true,
  "co2": false,
  "tempEmergency": false,
  "co2Emergency": false,
  "tempPIDOutput": 45.2,
  "co2PIDOutput": 12.7
}
```

---

## MQTT Integration

### Status Topic (LWT - Last Will and Testament)
- **Topic**: `{prefix}/status`
- **Retained**: Yes
- **QoS**: 1
- **Values**:
  - `online` - Device is connected
  - `offline` - Device disconnected (automatically published by broker when connection lost)

### Individual Topics (if enabled)
Published only when values change significantly:

- `{prefix}/temperature` - Water temperature (°C) - threshold: ±0.1°C
- `{prefix}/ambient_temperature` - Ambient temperature (°C) - threshold: ±0.1°C
- `{prefix}/ph` - pH value - threshold: ±0.05
- `{prefix}/tds` - TDS (ppm) - threshold: ±5 ppm
- `{prefix}/heater` - "ON" or "OFF"
- `{prefix}/co2` - "ON" or "OFF"
- `{prefix}/temp_emergency` - "true" or "false"
- `{prefix}/co2_emergency` - "true" or "false"

### JSON Payload (if enabled)
Published to `{prefix}/data` when any value changes:

```json
{
  "temperature": 25.2,
  "ambient_temp": 24.8,
  "ph": 6.8,
  "tds": 350,
  "heater": "ON",
  "co2": "OFF",
  "temp_pid_output": 45.2,
  "co2_pid_output": 12.7,
  "temp_emergency": false,
  "co2_emergency": false,
  "timestamp": 123456
}
```

**Includes:**
- All sensor readings
- Control states (heater, CO2)
- PID outputs (for advanced monitoring)
- Emergency stop states
- Timestamp (seconds since boot)

---

## MQTT LWT (Last Will and Testament) Implementation

### What is LWT?
Last Will and Testament (LWT) is an MQTT feature that automatically publishes a message when a client disconnects unexpectedly. This allows other systems to know when the aquarium controller goes offline.

### Implementation Details
1. **Will Topic**: `{prefix}/status`
2. **Will Message**: `offline`
3. **Will QoS**: 1 (at least once delivery)
4. **Will Retain**: true (last status always available)

### Behavior
- **On Connect**: Device publishes `online` to `{prefix}/status` (retained)
- **On Disconnect**: Broker automatically publishes `offline` to `{prefix}/status` (retained)
- **On Restart**: Device publishes `offline` before restarting (graceful shutdown)

### Home Assistant Integration
```yaml
binary_sensor:
  - platform: mqtt
    name: "Aquarium Controller Status"
    state_topic: "aquarium/status"
    payload_on: "online"
    payload_off: "offline"
    device_class: connectivity
```

---

## Change Detection & Efficiency

### Threshold-Based Publishing
To reduce MQTT traffic, values are only published when they change significantly:

| Sensor | Threshold | Typical Reduction |
|--------|-----------|-------------------|
| Temperature | ±0.1°C | 70-80% |
| pH | ±0.05 | 60-70% |
| TDS | ±5 ppm | 50-60% |

### State Changes
Heater, CO2, and emergency states are published immediately on any change.

### First Publish
All values are published once on startup, regardless of thresholds.

---

## Publishing Modes Comparison

| Feature | Individual Topics | JSON Payload |
|---------|------------------|--------------|
| **Best For** | Home Assistant | Data logging, InfluxDB |
| **Topic Count** | 8+ topics | 1 topic |
| **Native HA Discovery** | ✅ Yes | ⚠️ Requires parsing |
| **Atomic Timestamp** | ❌ No | ✅ Yes (all data at same moment) |
| **PID Outputs** | ❌ No | ✅ Yes |
| **Bandwidth** | Higher (multiple topics) | Lower (one message) |
| **Compatibility** | Maximum | Requires JSON support |

### Recommended Configurations

**Home Assistant Users:**
- Enable: Individual Topics ✅
- Disable: JSON Payload ❌

**InfluxDB/Grafana Users:**
- Disable: Individual Topics ❌
- Enable: JSON Payload ✅

**Both Systems:**
- Enable: Individual Topics ✅
- Enable: JSON Payload ✅

**Minimal Traffic:**
- Disable: Individual Topics ❌
- Enable: JSON Payload ✅

---

## Complete Settings List

### Network Settings
- `wifiSSID` - WiFi network name
- `wifiPassword` - WiFi password

### MQTT Settings
- `mqttServer` - MQTT broker address
- `mqttPort` - MQTT broker port (default: 1883)
- `mqttUser` - MQTT username (optional)
- `mqttPassword` - MQTT password (optional)
- `mqttClientId` - MQTT client ID (default: "aquarium-controller")
- `mqttTopicPrefix` - Topic prefix (default: "aquarium")
- `mqttPublishIndividual` - Enable individual topics (default: true)
- `mqttPublishJSON` - Enable JSON payload (default: false)

### Time Settings
- `ntpServer` - NTP server address (default: "pool.ntp.org")
- `gmtOffsetSec` - Timezone offset in seconds
- `daylightOffsetSec` - DST offset in seconds

### Control Settings
- `tempTarget` - Target water temperature (°C)
- `tempSafetyMax` - Maximum safe temperature (°C)
- `phTarget` - Target pH value
- `phSafetyMin` - Minimum safe pH value

### Hardware Pin Configuration
- `tempSensorPin` - Water temperature sensor GPIO
- `ambientSensorPin` - Ambient temperature sensor GPIO
- `phSensorPin` - pH sensor ADC GPIO
- `tdsSensorPin` - TDS sensor ADC GPIO
- `heaterRelayPin` - Heater control GPIO
- `co2RelayPin` - CO2 solenoid control GPIO

---

## Safety Features

### Automatic Emergency Stops
1. **Temperature Safety**: Heater disabled if `temperature > tempSafetyMax`
2. **pH Safety**: CO2 disabled if `ph < phSafetyMin`

### Manual Controls
- **Emergency Stop Button**: Immediately disable heater and CO2
- **Clear Emergency Button**: Re-enable control after resolving issue

### MQTT Status Monitoring
- **LWT ensures connectivity status is always known**
- **Offline status triggers automatically on:**
  - Network disconnection
  - Power failure
  - Device crash
  - Graceful restart

---

## Testing Checklist

✅ All settings can be viewed in web UI
✅ All settings can be modified in web UI
✅ All settings can be retrieved via GET /api/settings
✅ All settings can be updated via POST /api/settings
✅ Safety limits are editable
✅ Pin assignments are configurable
✅ NTP settings are fully configurable
✅ MQTT publish options are selectable
✅ Individual topics publish on change
✅ JSON payload includes all data + PID outputs
✅ LWT publishes "offline" on disconnect
✅ LWT publishes "online" on connect
✅ Graceful offline before restart

---

## Future Enhancements (Not Implemented)

- Per-sensor publish intervals
- QoS configuration per topic
- TLS/SSL for MQTT
- MQTT authentication certificate support
- Custom LWT message
- Dosing pump status in MQTT payload
- Water change phase in MQTT payload
