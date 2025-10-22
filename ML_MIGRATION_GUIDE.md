# Water Change ML Migration Guide

## Overview

This guide explains how to migrate from the on-device WaterChangePredictor to the external ML service architecture.

## What's Changing

### Before (Old System)
- ML calculations ran on ESP32
- Limited to simple algorithms due to memory constraints
- Predictions updated hourly on-device
- Historical data stored in NVS (limited space)

### After (New System)
- ML calculations run on separate Python service
- Can use sophisticated ML algorithms (scikit-learn)
- Predictions calculated daily or on water change events
- Full historical database in SQLite
- ESP32 just displays predictions received via MQTT

## Architecture Comparison

### Old Architecture
```
ESP32
├── Sensors (temp, pH, TDS)
├── WaterChangePredictor.cpp (ML on-device)
│   ├── Simple linear regression
│   ├── Limited history (20 records in NVS)
│   └── CPU/memory intensive
└── Display prediction
```

### New Architecture
```
ESP32                          Python Service
├── Sensors                    ├── Subscribe to sensor data
├── Publish sensor data ──────>├── Subscribe to water change events
├── Publish WC events ─────────>├── SQLite database (unlimited history)
├── Subscribe to predictions <─├── ML models (Linear, RF, GB)
└── Display prediction         └── Publish predictions
```

## Migration Steps

### Step 1: Install Python ML Service

See `tools/README_ML_SERVICE.md` for detailed instructions.

Quick version:
```bash
cd tools/
pip3 install -r requirements.txt
cp .env.example .env
# Edit .env with your MQTT settings
python3 train_and_predict.py  # Test run
```

### Step 2: Migrate Historical Data

The Python service needs historical water change data. You have two options:

#### Option A: Start Fresh (Recommended)
Just start using the new system. After 5-10 water changes, predictions will become accurate.

#### Option B: Export from ESP32 NVS
If you have significant history on the ESP32, you can export it:

1. Add this temporary function to your ESP32 code:
```cpp
void exportWaterChangeHistory() {
    if (!waterChangeAssistant || !mqttClient) return;
    
    auto history = waterChangeAssistant->getRecentHistory(50);
    
    StaticJsonDocument<4096> doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& record : history) {
        JsonObject obj = array.createNestedObject();
        obj["startTime"] = record.startTimestamp;
        obj["endTime"] = record.endTimestamp;
        obj["volume"] = record.volumeChanged;
        obj["tempBefore"] = record.tempBefore;
        obj["tempAfter"] = record.tempAfter;
        obj["phBefore"] = record.phBefore;
        obj["phAfter"] = record.phAfter;
        obj["tdsBefore"] = record.tdsBefore;
        obj["tdsAfter"] = record.tdsAfter;
        obj["duration"] = record.durationMinutes;
        obj["successful"] = record.completedSuccessfully;
    }
    
    char buffer[4096];
    serializeJson(doc, buffer);
    
    mqttClient->publish("aquarium/waterchange/history", buffer);
    Serial.println("Water change history exported to MQTT");
}
```

2. Call this function once from `setup()` or add a web API endpoint
3. The Python service will receive and store the history

### Step 3: Update ESP32 Code

#### A. Add ML Prediction Receiver

Already created:
- `include/MLPredictionReceiver.h`
- `src/MLPredictionReceiver.cpp`

#### B. Update SystemTasks.h

Add to globals:
```cpp
#include "MLPredictionReceiver.h"

extern MLPredictionReceiver* mlPredictor;
```

#### C. Update SystemTasks.cpp

Add MQTT callback for predictions:
```cpp
#include "MLPredictionReceiver.h"

MLPredictionReceiver* mlPredictor = nullptr;

// Add this function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    payload[length] = '\0';  // Null terminate
    char* payloadStr = (char*)payload;
    
    Serial.printf("[MQTT] Message on topic: %s\n", topic);
    
    // Handle ML predictions
    if (topicStr.endsWith("/ml/prediction")) {
        if (mlPredictor) {
            mlPredictor->processMessage(payloadStr);
        }
    }
}
```

#### D. Update main.cpp

Initialize and subscribe:
```cpp
#include "MLPredictionReceiver.h"

// In setup(), after MQTT connect:
void setup() {
    // ... existing setup code ...
    
    // Initialize ML prediction receiver
    mlPredictor = new MLPredictionReceiver();
    mlPredictor->begin(&mqttClientObj);
    
    // Set MQTT callback
    mqttClientObj.setCallback(mqttCallback);
    
    // Subscribe to ML predictions
    if (mqttClientObj.connected()) {
        String predTopic = String(configMgr->getConfig().mqttTopicPrefix) + "/ml/prediction";
        mqttClientObj.subscribe(predTopic.c_str());
        Serial.printf("Subscribed to: %s\n", predTopic.c_str());
    }
    
    // ... rest of setup ...
}

// In loop(), add MQTT loop:
void loop() {
    // ... existing loop code ...
    
    // Process MQTT messages
    if (mqttClient && mqttClient->connected()) {
        mqttClient->loop();
    }
    
    // ... rest of loop ...
}
```

#### E. Publish Water Change Events

In `WaterChangeAssistant::endWaterChange()`, add:
```cpp
bool WaterChangeAssistant::endWaterChange(float temp, float ph, float tds) {
    // ... existing code ...
    
    // Publish water change event to MQTT for ML service
    if (mqttClient && mqttClient->connected()) {
        StaticJsonDocument<512> doc;
        doc["startTime"] = record.startTimestamp;
        doc["endTime"] = record.endTimestamp;
        doc["volume"] = record.volumeChanged;
        doc["tempBefore"] = record.tempBefore;
        doc["tempAfter"] = record.tempAfter;
        doc["phBefore"] = record.phBefore;
        doc["phAfter"] = record.phAfter;
        doc["tdsBefore"] = record.tdsBefore;
        doc["tdsAfter"] = record.tdsAfter;
        doc["duration"] = record.durationMinutes;
        doc["successful"] = record.completedSuccessfully;
        
        char buffer[512];
        serializeJson(doc, buffer);
        
        String topic = String(configMgr->getConfig().mqttTopicPrefix) + "/waterchange/event";
        mqttClient->publish(topic.c_str(), buffer);
        
        Serial.println("[WC] Water change event published to MQTT");
    }
    
    // ... rest of function ...
}
```

### Step 4: Update Web Interface

Modify `/api/waterchange/status` in WebServer.cpp:

```cpp
// Old code (commented out):
// if (wcPredictor) {
//     JsonObject wc = doc.createNestedObject("water_change_prediction");
//     wc["days_until_change"] = wcPredictor->getPredictedDaysUntilChange();
//     wc["confidence_percent"] = wcPredictor->getConfidenceLevel();
// }

// New code:
if (mlPredictor && mlPredictor->isPredictionValid()) {
    JsonObject ml = doc.createNestedObject("ml_prediction");
    ml["days_remaining"] = mlPredictor->getPredictedDaysRemaining();
    ml["confidence_percent"] = mlPredictor->getConfidencePercent();
    ml["model"] = mlPredictor->getModelName();
    ml["tds_rate"] = mlPredictor->getTDSIncreaseRate();
    ml["needs_change_soon"] = mlPredictor->needsWaterChangeSoon();
    ml["valid"] = mlPredictor->isPredictionValid();
    ml["stale"] = mlPredictor->isPredictionStale();
}
```

### Step 5: Remove Old Code (Optional)

Once the new system is working, you can remove:

1. **Files to delete:**
   - `include/WaterChangePredictor.h`
   - `src/WaterChangePredictor.cpp`

2. **References to remove:**
   - Remove `#include "WaterChangePredictor.h"` from SystemTasks
   - Remove `wcPredictor` global variable
   - Remove initialization in main.cpp
   - Update web interface to only show ML predictions

3. **Benefits:**
   - Free ~40KB flash
   - Free ~8KB RAM
   - Reduce CPU usage
   - Simpler ESP32 codebase

## Testing the Migration

### 1. Verify MQTT Connection

On your MQTT broker machine:
```bash
# Subscribe to see what ESP32 publishes
mosquitto_sub -h localhost -t 'aquarium/#' -v

# You should see:
# aquarium/data - Every 30 seconds (sensor data)
# aquarium/waterchange/event - When water change completes
```

### 2. Verify Python Service

```bash
# Check service is running
sudo systemctl status aquarium-ml

# View logs
sudo journalctl -u aquarium-ml -f

# You should see:
# "Connected to MQTT broker"
# "Subscribed to topics"
# "Sensor data stored: TDS=350, pH=7.2"
```

### 3. Perform Test Water Change

1. Do a water change via web interface
2. Complete the water change
3. Check Python service logs:
   ```
   [INFO] Water change event received and stored
   [INFO] Retraining model with new water change data...
   [INFO] Training complete: {'model': 'gradient_boost', 'score': 0.892}
   [INFO] Prediction: 6.3 days remaining (confidence: 85%)
   [INFO] Prediction published successfully
   ```

4. Check ESP32 serial output:
   ```
   [MQTT] Message on topic: aquarium/ml/prediction
   [ML] Prediction received: 6.3 days remaining (confidence: 85%)
   [ML] Model: gradient_boost, TDS rate: 12.45 ppm/day
   ```

### 4. Verify Display

1. Check OLED display - should show predicted days
2. Check web interface - should show prediction with confidence
3. Both should match the MQTT prediction

## Troubleshooting

### ESP32 Not Receiving Predictions

**Check MQTT subscription:**
```cpp
// In main.cpp setup(), verify:
Serial.printf("Subscribed to: %s\n", predTopic.c_str());
// Should print: "Subscribed to: aquarium/ml/prediction"
```

**Check callback is registered:**
```cpp
mqttClientObj.setCallback(mqttCallback);
// Must be BEFORE connecting
```

**Check MQTT loop is called:**
```cpp
// In loop():
mqttClient->loop();  // MUST be called regularly
```

### Python Service Not Publishing

**Check MQTT connection:**
```bash
# View service logs
sudo journalctl -u aquarium-ml -n 50

# Should see:
# "Connected to MQTT broker at 192.168.1.100:1883"
```

**Check sufficient history:**
```bash
sqlite3 aquarium_data.db "SELECT COUNT(*) FROM water_changes"
# Need at least 5 records
```

**Manually trigger:**
```bash
python3 train_and_predict.py
# Should see prediction published
```

### Predictions Not Accurate

**Needs more training data:**
- Minimum: 5 water changes
- Good: 10-15 water changes
- Excellent: 20+ water changes

**Check data quality:**
```sql
sqlite3 aquarium_data.db
SELECT 
  datetime(end_timestamp, 'unixepoch') as date,
  volume_litres, 
  tds_before, 
  tds_after,
  (end_timestamp - LAG(end_timestamp) OVER (ORDER BY end_timestamp)) / 86400.0 as days_since_last
FROM water_changes 
ORDER BY end_timestamp DESC 
LIMIT 10;
```

Look for:
- Regular intervals (not erratic)
- Consistent TDS changes
- Reasonable volumes

## Rollback Plan

If you need to revert to the old system:

1. Stop Python service:
   ```bash
   sudo systemctl stop aquarium-ml
   sudo systemctl disable aquarium-ml
   ```

2. Re-enable WaterChangePredictor in ESP32:
   - Uncomment old code
   - Remove ML prediction receiver code
   - Reflash ESP32

3. Old system resumes immediately

## Benefits Summary

### Performance
- **ESP32 Flash**: Save ~40KB
- **ESP32 RAM**: Save ~8KB
- **ESP32 CPU**: Reduce usage by ~5%
- **Prediction Quality**: Improve by ~20-30%

### Maintainability
- ML updates without reflashing ESP32
- Easy to add new features
- Better debugging tools
- Historical data analysis

### Scalability
- Multiple tanks easily supported
- Can add more ML features
- Integration with other services
- Cloud deployment possible

## Next Steps

After migration is complete:

1. **Monitor for 1 week** - Verify predictions are stable
2. **Collect more data** - More water changes = better predictions
3. **Tune parameters** - Adjust ML models if needed
4. **Add features** - Consider feeding schedule, lighting integration
5. **Scale up** - Add more tanks if desired

## Support

If you encounter issues:

1. Check logs (ESP32 serial + Python service)
2. Verify MQTT topics match
3. Ensure sufficient training data
4. Review troubleshooting section
5. Check database contents

The new system should provide better predictions with less ESP32 load!
