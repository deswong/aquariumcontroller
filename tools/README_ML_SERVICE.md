# Water Change ML Prediction Service

## Overview

This service uses machine learning to predict when your aquarium will need its next water change. It runs separately from the ESP32 controller, analyzing historical data to provide accurate predictions.

## Architecture

```
┌─────────────────┐         MQTT          ┌──────────────────┐
│                 │  ───────────────────>  │                  │
│   ESP32         │   Sensor Data          │  Python ML       │
│   Controller    │   Water Change Events  │  Service         │
│                 │  <─────────────────────│                  │
└─────────────────┘   ML Predictions       └──────────────────┘
        │                                            │
        │                                            │
        v                                            v
   [NVS Storage]                              [SQLite Database]
```

### Data Flow

1. **ESP32 → MQTT**: Publishes sensor data (temp, pH, TDS) and water change events
2. **Python Service**:
   - Subscribes to sensor data and water change events
   - Stores data in SQLite database
   - Trains ML models (Linear Regression, Random Forest, Gradient Boosting)
   - Generates predictions
3. **Python Service → MQTT**: Publishes predictions (days until next water change, confidence)
4. **ESP32**: Subscribes to predictions and displays on OLED/web interface

## Features

- **Multiple ML Models**: Automatically selects best performing model
- **Feature Engineering**: 11 features including TDS trends, pH, temperature, seasonality
- **Confidence Scoring**: Provides confidence level based on data quality and model performance
- **Historical Tracking**: Maintains prediction accuracy history
- **Real-time Updates**: Retrains when new water changes occur
- **Persistent Storage**: SQLite database for all historical data

## Installation

### Requirements

- Python 3.7+
- MQTT Broker (Mosquitto recommended)
- pip packages listed in requirements.txt

### Step 1: Install Python Dependencies

```bash
cd tools/
pip3 install -r requirements.txt
```

### Step 2: Configure MQTT Settings

```bash
cp .env.example .env
nano .env  # Edit with your settings
```

```ini
MQTT_BROKER=192.168.1.100  # Your MQTT broker IP
MQTT_PORT=1883
MQTT_USER=aquarium          # Optional
MQTT_PASSWORD=secret        # Optional
MQTT_TOPIC_PREFIX=aquarium  # Must match ESP32 config
DB_FILE=/var/lib/aquarium/aquarium_data.db
```

### Step 3: Choose Running Mode

#### Option A: Run as Persistent Service (Recommended)

1. Edit `aquarium-ml.service` and update paths:
   ```bash
   sudo nano aquarium-ml.service
   # Change YOUR_USERNAME and /path/to/aquariumcontroller
   ```

2. Install and enable service:
   ```bash
   sudo cp aquarium-ml.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable aquarium-ml
   sudo systemctl start aquarium-ml
   ```

3. Check status:
   ```bash
   sudo systemctl status aquarium-ml
   sudo journalctl -u aquarium-ml -f  # View logs
   ```

#### Option B: Run Daily via Cron

1. Make script executable:
   ```bash
   chmod +x train_and_predict.py
   ```

2. Add to crontab (runs at 2 AM daily):
   ```bash
   crontab -e
   ```
   
   Add:
   ```
   0 2 * * * cd /path/to/aquariumcontroller/tools && /usr/bin/python3 train_and_predict.py >> /var/log/aquarium-ml.log 2>&1
   ```

#### Option C: Manual Run

```bash
python3 train_and_predict.py
```

## ESP32 Integration

### Publishing Water Change Events

The ESP32 automatically publishes water change events when you complete a water change via the web interface. The event is published to:

**Topic**: `aquarium/waterchange/event`

**Payload**:
```json
{
  "startTime": 1698123456,
  "endTime": 1698125456,
  "volume": 25.0,
  "tempBefore": 25.5,
  "tempAfter": 24.8,
  "phBefore": 7.2,
  "phAfter": 7.3,
  "tdsBefore": 350,
  "tdsAfter": 320,
  "duration": 30,
  "successful": true
}
```

### Receiving Predictions

The ESP32 subscribes to predictions on:

**Topic**: `aquarium/ml/prediction`

**Payload**:
```json
{
  "predicted_days_remaining": 5.3,
  "predicted_total_cycle_days": 7.0,
  "days_since_last_change": 1.7,
  "confidence": 0.85,
  "model": "gradient_boost",
  "current_tds": 380,
  "tds_increase_rate": 15.2,
  "needs_change_soon": false,
  "timestamp": 1698123456
}
```

## ML Models

### Features Used

1. **Days since previous water change** - Historical cycle length
2. **TDS before change** - Water quality at change time
3. **TDS after change** - Effectiveness of water change
4. **TDS increase rate** - Rate of degradation (ppm/day)
5. **Volume changed** - Amount of water changed (litres)
6. **pH before/after** - pH levels
7. **Temperature before/after** - Temperature stability
8. **Day of week** - Weekly patterns (0-6)
9. **Season** - Seasonal effects (0-3)

### Models Trained

1. **Linear Regression** - Simple baseline
2. **Random Forest** - Handles non-linear relationships
3. **Gradient Boosting** - Often best performer

The service automatically selects the model with the highest R² score.

### Confidence Calculation

```
Confidence = Model R² Score × Data Quality Factor
```

- Model R² Score: 0-1 (how well model fits data)
- Data Quality Factor: min(1.0, samples / 20) - full confidence at 20+ water changes

## Database Schema

### `sensor_readings` Table
- Stores all sensor data (temp, pH, TDS, relay states)
- Auto-expires old data (optional)

### `water_changes` Table
- Complete water change history
- Before/after sensor readings
- Duration and volume

### `predictions` Table
- Historical predictions for accuracy tracking
- Allows model performance evaluation over time

## Monitoring

### Check Service Status
```bash
sudo systemctl status aquarium-ml
```

### View Live Logs
```bash
sudo journalctl -u aquarium-ml -f
```

### View Database
```bash
sqlite3 /var/lib/aquarium/aquarium_data.db
sqlite> SELECT COUNT(*) FROM water_changes;
sqlite> SELECT * FROM water_changes ORDER BY end_timestamp DESC LIMIT 5;
```

### MQTT Monitoring
```bash
# Subscribe to all aquarium topics
mosquitto_sub -h localhost -t 'aquarium/#' -v

# Subscribe only to predictions
mosquitto_sub -h localhost -t 'aquarium/ml/prediction' -v
```

## Troubleshooting

### Service Won't Start

1. Check permissions:
   ```bash
   sudo chown -R your_user:your_user /path/to/aquariumcontroller/tools
   ```

2. Check MQTT broker:
   ```bash
   mosquitto -v  # Start mosquitto in verbose mode
   ```

3. Check Python dependencies:
   ```bash
   pip3 list | grep -E "paho-mqtt|numpy|scikit-learn"
   ```

### Not Receiving Predictions

1. Verify MQTT topic prefix matches:
   ```bash
   # ESP32 config
   mqttTopicPrefix = "aquarium"
   
   # Python .env
   MQTT_TOPIC_PREFIX=aquarium
   ```

2. Check water change history:
   ```bash
   sqlite3 aquarium_data.db "SELECT COUNT(*) FROM water_changes"
   # Need at least 5 water changes
   ```

3. Test manual prediction:
   ```bash
   python3 train_and_predict.py
   # Should see "Prediction published successfully"
   ```

### Low Confidence Scores

- Need more water change history (aim for 10-20 changes)
- Irregular water change intervals reduce confidence
- Check for data quality issues (missing sensor readings)

### Predictions Seem Wrong

1. Check recent water change data:
   ```sql
   SELECT 
     datetime(end_timestamp, 'unixepoch') as date,
     tds_before, tds_after, volume_litres
   FROM water_changes 
   ORDER BY end_timestamp DESC 
   LIMIT 10;
   ```

2. Verify TDS sensor calibration
3. Allow more training data to accumulate

## Advanced Usage

### Custom Training Schedule

Edit cron for different frequency:
```bash
# Every 6 hours
0 */6 * * * python3 /path/to/train_and_predict.py

# Twice daily (6 AM and 6 PM)
0 6,18 * * * python3 /path/to/train_and_predict.py
```

### Manual Model Selection

Edit `water_change_ml_service.py` to force a specific model:
```python
# Force gradient boosting
self.best_model_name = 'gradient_boost'
self.best_model = self.models['gradient_boost']
```

### Export Data for Analysis

```bash
# Export to CSV
sqlite3 -header -csv aquarium_data.db \
  "SELECT * FROM water_changes" > water_changes.csv

sqlite3 -header -csv aquarium_data.db \
  "SELECT * FROM sensor_readings WHERE timestamp > $(date -d '30 days ago' +%s)" > sensors_30days.csv
```

### Prediction API

Query current prediction:
```bash
mosquitto_sub -h localhost -t 'aquarium/ml/prediction' -C 1
```

Returns last retained prediction immediately.

## Performance

- **Training Time**: ~100ms for 50 water changes
- **Prediction Time**: ~10ms
- **Database Size**: ~1MB per month of hourly sensor data
- **Memory Usage**: ~50MB Python service
- **CPU Usage**: <1% except during training

## Benefits Over On-Device ML

1. **Better Models**: Can use complex ML algorithms (scikit-learn)
2. **More Features**: 11 features vs limited ESP32 memory
3. **Historical Analysis**: SQLite database with full history
4. **Model Comparison**: Automatically selects best model
5. **Easy Updates**: Retrain without reflashing ESP32
6. **Low ESP32 Load**: Reduces CPU/memory usage on controller
7. **Scalable**: Add more aquariums easily

## Future Enhancements

- Multiple tank support
- Web dashboard for predictions
- Alert notifications (email/SMS)
- Automatic adjustment based on feeding schedule
- Integration with aquarium lighting schedules
- Export reports (PDF/HTML)

## Credits

Built for the ESP32 Aquarium Controller project. Uses scikit-learn for ML, Paho MQTT for communication, and SQLite for storage.
