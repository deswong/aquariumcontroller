# ML Service Quick Start Guide

## Prerequisites
- Python 3.7+
- MQTT Broker (Mosquitto recommended)
- ESP32 with aquarium controller firmware

## Installation (5 Minutes)

### 1. Install Dependencies
```bash
cd tools/
pip3 install -r requirements.txt
```

### 2. Configure
```bash
cp .env.example .env
nano .env
```

Edit these values:
```bash
MQTT_BROKER=192.168.1.100    # Your MQTT broker IP
DB_TYPE=sqlite               # or 'mariadb'
```

### 3. Run Service
```bash
# Test run (foreground)
python3 water_change_ml_service.py

# Or install as system service
sudo cp aquarium-ml.service /etc/systemd/system/
sudo systemctl enable aquarium-ml
sudo systemctl start aquarium-ml
```

### 4. View on Web Dashboard
Open your ESP32's web interface → **Water Change** tab
You'll see the **AI Water Change Prediction** card at the top.

## What You'll See

### Initial State (0-4 water changes)
```
⚠️ Insufficient data for prediction. 
Perform 3 more water changes.
```

### After 5+ Water Changes
```
🤖 AI Water Change Prediction
┌─────────────────────────────────┐
│ Days Until Next Change: 6.3     │
│ Confidence: 85%                 │
│ Model: gradient_boost           │
│ TDS Rate: 12.45 ppm/day         │
│ ✅ Water quality stable         │
└─────────────────────────────────┘
```

## How It Works

1. **ESP32 publishes** sensor data every 30s via MQTT
2. **Python service subscribes** and stores data in database
3. **On water change completion**, ESP32 publishes event
4. **Python service retrains** ML model with new data
5. **Prediction is published** back to ESP32 via MQTT
6. **ESP32 receives prediction** and shows on web dashboard
7. **Web interface auto-updates** every 60 seconds

## Quick Commands

### Check Service Status
```bash
sudo systemctl status aquarium-ml
sudo journalctl -u aquarium-ml -f
```

### View Database
```bash
sqlite3 aquarium_data.db
sqlite> SELECT * FROM water_changes ORDER BY end_timestamp DESC LIMIT 5;
sqlite> SELECT * FROM filter_maintenance ORDER BY timestamp DESC LIMIT 5;
sqlite> SELECT * FROM predictions ORDER BY prediction_timestamp DESC LIMIT 5;
```

### Monitor MQTT
```bash
# View all messages
mosquitto_sub -h localhost -t 'aquarium/#' -v

# View predictions only
mosquitto_sub -h localhost -t 'aquarium/ml/prediction' -v
```

### Force Retrain
```bash
# Stop service
sudo systemctl stop aquarium-ml

# Run training script
python3 train_and_predict.py

# Restart service
sudo systemctl start aquarium-ml
```

## Troubleshooting

### Problem: "No prediction available"
**Solution:** Perform at least 5 water changes first.

### Problem: "Prediction stale"
**Solution:** Check if ML service is running:
```bash
sudo systemctl restart aquarium-ml
```

### Problem: Low confidence (<50%)
**Solution:** Perform more water changes on a regular schedule.

### Problem: Can't connect to MQTT
**Solution:** Check MQTT broker is running:
```bash
sudo systemctl status mosquitto
```

## MariaDB Setup (Optional)

Only needed if you have multiple tanks or want advanced analytics.

```bash
# 1. Install
sudo apt install mariadb-server

# 2. Create database
sudo mysql -u root -p
```
```sql
CREATE DATABASE aquarium;
CREATE USER 'aquarium'@'localhost' IDENTIFIED BY 'password123';
GRANT ALL PRIVILEGES ON aquarium.* TO 'aquarium'@'localhost';
EXIT;
```

```bash
# 3. Update .env
nano tools/.env
```
```bash
DB_TYPE=mariadb
DB_HOST=localhost
DB_NAME=aquarium
DB_USER=aquarium
DB_PASSWORD=password123
```

```bash
# 4. Restart service
sudo systemctl restart aquarium-ml
```

## Tips for Best Results

1. **Be consistent** - Perform water changes on a regular schedule
2. **Record filter maintenance** - Click "Record Maintenance" button when cleaning
3. **Wait for data** - Predictions improve after 10-20 water changes
4. **Check confidence** - >80% = very accurate, 60-80% = good, <60% = needs more data
5. **Update regularly** - Restart service weekly to ensure stability

## Key Features

✅ **Auto-learns** your maintenance patterns
✅ **Filter-aware** predictions
✅ **Multiple ML models** (chooses best automatically)
✅ **Web dashboard** integration
✅ **SQLite or MariaDB** support
✅ **Zero configuration** required
✅ **Automatic retraining** on every water change

## Next Steps

1. Perform 5 water changes to get initial predictions
2. Record filter maintenance when you clean filters
3. Monitor accuracy via web dashboard
4. After 20+ water changes, expect 80-90% confidence

Need help? Check `ML_ENHANCEMENT_SUMMARY.md` for detailed documentation.
