# Aquarium Controller REST API Documentation

## Overview

The Aquarium Controller provides a comprehensive REST API for monitoring and controlling all aspects of your aquarium system. All endpoints return JSON responses unless otherwise specified.

**Base URL**: `http://<device-ip>/api`

**Content Type**: `application/json`

## Authentication

Currently, no authentication is required (see Security section in main README).

---

## Endpoints

### 1. System Status

#### GET /api/data
Get current sensor readings and system status.

**Response:**
```json
{
  "temperature": 25.5,
  "ph": 6.8,
  "tds": 350,
  "ambientTemp": 22.0,
  "heaterState": true,
  "co2State": false,
  "uptime": 3600000,
  "freeHeap": 245000,
  "wifiRSSI": -55
}
```

**Fields:**
- `temperature`: Current tank temperature (°C)
- `ph`: Current pH value
- `tds`: Total dissolved solids (ppm)
- `ambientTemp`: Room temperature (°C)
- `heaterState`: Heater relay status (boolean)
- `co2State`: CO2 relay status (boolean)
- `uptime`: System uptime in milliseconds
- `freeHeap`: Free heap memory in bytes
- `wifiRSSI`: WiFi signal strength in dBm

---

### 2. Control Targets

#### POST /api/targets
Set target values for temperature and pH control.

**Request Body:**
```json
{
  "temperature": 25.0,
  "ph": 6.8
}
```

**Response:**
```json
{
  "success": true,
  "message": "Targets updated successfully"
}
```

**Validation:**
- `temperature`: 18-32°C
- `ph`: 5.0-9.0

---

### 3. Emergency Controls

#### POST /api/emergency
Trigger emergency stop (shuts off all relays).

**Response:**
```json
{
  "success": true,
  "message": "Emergency stop activated"
}
```

#### POST /api/emergency/clear
Clear emergency stop and resume normal operation.

**Response:**
```json
{
  "success": true,
  "message": "Emergency stop cleared"
}
```

---

### 4. pH Calibration

#### POST /api/calibrate/start
Start pH calibration process.

**Response:**
```json
{
  "success": true,
  "message": "Calibration started"
}
```

#### POST /api/calibrate/point
Add calibration point (1, 2, or 3 point calibration).

**Request Body:**
```json
{
  "phValue": 7.0,
  "pointNumber": 1
}
```

**Parameters:**
- `phValue`: Reference pH value (4.0, 6.86, 7.0, 9.18, or 10.0)
- `pointNumber`: Calibration point (1, 2, or 3)

**Response:**
```json
{
  "success": true,
  "message": "Calibration point added",
  "voltage": 1.234
}
```

#### POST /api/calibrate/save
Save calibration data to NVS.

**Response:**
```json
{
  "success": true,
  "message": "Calibration saved"
}
```

#### POST /api/calibrate/reset
Reset calibration to factory defaults.

**Response:**
```json
{
  "success": true,
  "message": "Calibration reset"
}
```

#### GET /api/calibrate/voltage
Get current pH probe voltage reading.

**Response:**
```json
{
  "voltage": 1.234,
  "adcRaw": 1523
}
```

---

### 5. Event Logs

#### GET /api/logs
Retrieve system event logs.

**Query Parameters:**
- `limit`: Maximum number of logs to return (default: 100, max: 1000)
- `level`: Filter by log level (`info`, `warning`, `error`)
- `category`: Filter by category (`system`, `network`, `sensor`, `control`)

**Example:** `/api/logs?limit=50&level=error`

**Response:**
```json
{
  "count": 50,
  "logs": [
    {
      "timestamp": 1634567890,
      "level": "error",
      "category": "sensor",
      "message": "Temperature sensor disconnected"
    }
  ]
}
```

#### DELETE /api/logs
Clear all event logs.

**Response:**
```json
{
  "success": true,
  "message": "Logs cleared"
}
```

---

### 6. System Settings

#### GET /api/settings
Get all system settings.

**Response:**
```json
{
  "wifi": {
    "ssid": "MyNetwork",
    "connected": true,
    "ip": "192.168.1.100"
  },
  "mqtt": {
    "server": "mqtt.example.com",
    "port": 1883,
    "connected": true,
    "publishIndividual": true,
    "publishJSON": false
  },
  "ntp": {
    "server": "au.pool.ntp.org",
    "gmtOffset": 36000,
    "daylightOffset": 3600,
    "timeSynced": true,
    "currentTime": "2024-10-23T15:30:45"
  },
  "tank": {
    "length": 100,
    "width": 40,
    "height": 50,
    "volume": 200
  }
}
```

#### POST /api/settings
Update system settings (requires body with specific settings to update).

**Request Body Examples:**

WiFi Settings:
```json
{
  "wifi": {
    "ssid": "NewNetwork",
    "password": "newpassword"
  }
}
```

MQTT Settings:
```json
{
  "mqtt": {
    "server": "mqtt.example.com",
    "port": 1883,
    "user": "username",
    "password": "password",
    "publishIndividual": true,
    "publishJSON": false
  }
}
```

---

### 7. Configuration Management

#### GET /api/config
Get complete system configuration.

**Response:** Full configuration JSON object.

#### GET /api/config/export
Export configuration as downloadable JSON file.

**Response:** JSON file download with `Content-Disposition: attachment`.

#### POST /api/config/save
Save current configuration to NVS.

**Response:**
```json
{
  "success": true,
  "message": "Configuration saved"
}
```

#### POST /api/config/import
Import configuration from JSON file.

**Request:** Multipart form data with `config` file field.

**Response:**
```json
{
  "success": true,
  "message": "Configuration imported successfully"
}
```

#### POST /api/config/tank
Update tank dimensions.

**Request Body:**
```json
{
  "length": 100,
  "width": 40,
  "height": 50
}
```

**Response:**
```json
{
  "success": true,
  "volume": 200,
  "message": "Tank dimensions updated"
}
```

---

### 8. Water Change Assistant

#### GET /api/waterchange/status
Get water change status and predictions.

**Response:**
```json
{
  "active": false,
  "lastChange": 1634567890,
  "nextPredicted": 1635177690,
  "daysUntilNext": 7,
  "waterChangeCount": 15,
  "patternConfidence": 0.85
}
```

#### POST /api/waterchange/start
Start water change assistant.

**Request Body:**
```json
{
  "volumeLitres": 40,
  "targetTemp": 25.0
}
```

**Response:**
```json
{
  "success": true,
  "message": "Water change started",
  "estimatedDuration": 1200
}
```

#### POST /api/waterchange/complete
Mark water change as complete.

**Response:**
```json
{
  "success": true,
  "message": "Water change recorded",
  "actualDuration": 1150
}
```

---

### 9. ML PID Status (Phase 2/3)

#### GET /api/pid/temp/health
Get temperature PID controller health metrics.

**Response:**
```json
{
  "outputStuck": false,
  "persistentHighError": false,
  "outputSaturation": false,
  "hasError": false,
  "errorMessage": "",
  "stuckOutputCount": 0,
  "saturationCount": 12,
  "lastHealthCheck": 1634567890
}
```

#### GET /api/pid/temp/kalman
Get Kalman filter status for temperature controller.

**Response:**
```json
{
  "enabled": true,
  "initialized": true,
  "state": 25.3,
  "covariance": 0.05,
  "processNoise": 0.001,
  "measurementNoise": 0.1
}
```

#### GET /api/pid/temp/feedforward
Get feed-forward model status for temperature controller.

**Response:**
```json
{
  "enabled": true,
  "tdsContribution": -0.15,
  "ambientContribution": 0.45,
  "phContribution": 0.0,
  "totalContribution": 0.30
}
```

#### GET /api/pid/temp/profiler
Get performance profiling data.

**Response:**
```json
{
  "computeTimeUs": 245,
  "maxComputeTimeUs": 1200,
  "minComputeTimeUs": 180,
  "avgComputeTimeUs": 320,
  "computeCount": 36000,
  "overrunCount": 2,
  "cpuUsagePercent": 2.4
}
```

*(Similar endpoints exist for `/api/pid/co2/*` for CO2/pH controller)*

---

### 10. System Monitoring (New)

#### GET /api/monitor/heap
Get heap memory status.

**Response:**
```json
{
  "freeHeap": 245000,
  "totalHeap": 327680,
  "minFreeHeap": 198000,
  "largestFreeBlock": 120000,
  "usagePercent": 25.2
}
```

#### GET /api/monitor/tasks
Get FreeRTOS task information.

**Response:**
```json
{
  "tasks": [
    {
      "name": "SensorTask",
      "stackSize": 6144,
      "stackFree": 4280,
      "stackUsagePercent": 30.3,
      "state": "RUN"
    }
  ]
}
```

#### GET /api/notifications
Get system notifications.

**Query Parameters:**
- `unacknowledged`: Only return unacknowledged notifications (boolean)
- `level`: Filter by level (`info`, `warning`, `error`, `critical`)

**Response:**
```json
{
  "count": 5,
  "unacknowledgedCount": 2,
  "notifications": [
    {
      "timestamp": 1634567890,
      "level": "warning",
      "category": "sensor",
      "message": "Weak WiFi signal: -78 dBm",
      "acknowledged": false
    }
  ]
}
```

#### POST /api/notifications/acknowledge
Acknowledge all notifications.

**Response:**
```json
{
  "success": true,
  "acknowledgedCount": 5
}
```

---

## Error Responses

All endpoints may return error responses in the following format:

```json
{
  "success": false,
  "error": "Error message describing what went wrong",
  "code": 400
}
```

**Common HTTP Status Codes:**
- `200 OK`: Request successful
- `400 Bad Request`: Invalid request parameters
- `404 Not Found`: Endpoint not found
- `500 Internal Server Error`: Server error
- `501 Not Implemented`: Feature not yet implemented

---

## Rate Limiting

No rate limiting is currently implemented. However, it's recommended to:
- Poll `/api/data` no more than once per second
- Avoid rapid configuration changes
- Use MQTT for real-time monitoring instead of polling

---

## Examples

### cURL Examples

Get sensor data:
```bash
curl http://192.168.1.100/api/data
```

Set temperature target:
```bash
curl -X POST http://192.168.1.100/api/targets \
  -H "Content-Type: application/json" \
  -d '{"temperature": 25.5, "ph": 6.8}'
```

Export configuration:
```bash
curl http://192.168.1.100/api/config/export \
  -o aquarium-config.json
```

### Python Example

```python
import requests

# Get current sensor data
response = requests.get('http://192.168.1.100/api/data')
data = response.json()
print(f"Temperature: {data['temperature']}°C")
print(f"pH: {data['ph']}")

# Set new targets
targets = {
    'temperature': 26.0,
    'ph': 6.9
}
response = requests.post('http://192.168.1.100/api/targets', json=targets)
print(response.json())
```

### JavaScript Example

```javascript
// Fetch sensor data
fetch('http://192.168.1.100/api/data')
  .then(response => response.json())
  .then(data => {
    console.log(`Temperature: ${data.temperature}°C`);
    console.log(`pH: ${data.ph}`);
  });

// Set new targets
fetch('http://192.168.1.100/api/targets', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    temperature: 26.0,
    ph: 6.9
  })
})
.then(response => response.json())
.then(data => console.log(data));
```

---

## MQTT Integration

The REST API complements MQTT publishing. For real-time monitoring, subscribe to MQTT topics:

- `aquarium/temperature`
- `aquarium/ph`
- `aquarium/tds`
- `aquarium/heater`
- `aquarium/co2`
- `aquarium/data` (JSON with all readings)

See main documentation for MQTT configuration details.

---

## Support

For issues or feature requests, please refer to the main README or create an issue on the GitHub repository.

**Version:** 2.0.0  
**Last Updated:** October 2025
