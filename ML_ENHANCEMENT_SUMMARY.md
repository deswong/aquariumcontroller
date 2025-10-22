# ML Service Enhancement Summary

## Overview

This document summarizes the enhancements made to the Water Change ML Prediction system, including:
1. **MariaDB database support** alongside SQLite
2. **Filter maintenance integration** into ML predictions
3. **Enhanced web dashboard** for viewing predictions
4. **MQTT event publishing** from ESP32

## Changes Made

### 1. Python ML Service (`tools/water_change_ml_service.py`)

#### Database Abstraction Layer
- Created database-agnostic architecture supporting both SQLite and MariaDB
- Added `DB_TYPE` environment variable to select database type
- Implemented `_get_placeholder()` method for SQL parameter compatibility
- Updated all database queries to work with both database types

####Filter Maintenance Table
**SQLite Schema:**
```sql
CREATE TABLE IF NOT EXISTS filter_maintenance (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    filter_type TEXT DEFAULT 'mechanical',
    days_since_last INTEGER DEFAULT 0,
    tds_before REAL,
    tds_after REAL,
    notes TEXT
)
```

**MariaDB Schema:**
```sql
CREATE TABLE IF NOT EXISTS filter_maintenance (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    filter_type VARCHAR(50) DEFAULT 'mechanical',
    days_since_last INT DEFAULT 0,
    tds_before FLOAT,
    tds_after FLOAT,
    notes TEXT,
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
```

#### Enhanced ML Features
The ML model now includes **14 features** (up from 11):
1. Days since previous water change
2. TDS before change
3. TDS after change
4. TDS increase rate
5. Volume changed
6. pH before
7. pH after
8. Temperature before
9. Temperature after
10. Day of week
11. Season (0-3)
12. **Days since last filter maintenance** ← NEW
13. **Filter maintenance in period (0/1)** ← NEW
14. **TDS change from filter maintenance** ← NEW

**Impact:** Filter maintenance now affects predictions, as clean filters reduce TDS accumulation rate.

#### New Database Methods
```python
store_filter_maintenance(fm_data: Dict)
get_filter_maintenance_history(limit: int = 50) -> List[Dict]
get_last_filter_maintenance() -> Optional[Dict]
```

#### MQTT Topic Subscription
Added subscription to `aquarium/filter/maintenance` topic.

### 2. ESP32 Water Change Assistant (`src/WaterChangeAssistant.cpp`)

#### Water Change Event Publishing
When a water change completes, the ESP32 now publishes to `aquarium/waterchange/event`:
```json
{
  "startTime": 1729612800,
  "endTime": 1729613400,
  "volume": 20.0,
  "tempBefore": 25.2,
  "tempAfter": 24.8,
  "phBefore": 6.85,
  "phAfter": 6.90,
  "tdsBefore": 380,
  "tdsAfter": 210,
  "duration": 10,
  "successful": true
}
```

#### Filter Maintenance Event Publishing
When filter maintenance is recorded, the ESP32 publishes to `aquarium/filter/maintenance`:
```json
{
  "timestamp": 1729612800,
  "filter_type": "mechanical",
  "days_since_last": 14,
  "notes": "Replaced filter media"
}
```

### 3. Web Dashboard (`data/index.html`)

#### New ML Prediction Section
Added beautiful gradient card at top of Water Change tab showing:
- **Days until next change** (large display)
- **Days since last change** (smaller text)
- **Confidence percentage** (0-100%)
- **ML model name** (linear/random_forest/gradient_boost)
- **TDS increase rate** (ppm/day)
- **Filter maintenance status** (if within 999 days)
- **Status message** with color coding:
  - 🟢 Green: Water quality stable
  - 🟡 Yellow: Change approaching or recommended soon
  - 🔴 Red: Error or insufficient data
- **Last update timestamp**

#### JavaScript Functions
```javascript
async function updateMLPrediction()
```
- Fetches from `/api/ml/prediction`
- Updates all ML prediction display elements
- Handles errors gracefully (no prediction, stale data, insufficient samples)
- Auto-updates every 60 seconds

### 4. ESP32 Web API (`src/WebServer.cpp`)

#### New Endpoint: `/api/ml/prediction`
Returns ML prediction data from `MLPredictionReceiver`:
```json
{
  "predicted_days_remaining": 5.3,
  "predicted_total_cycle_days": 7.5,
  "days_since_last_change": 2.2,
  "confidence": 0.87,
  "model": "gradient_boost",
  "current_tds": 325,
  "tds_increase_rate": 15.23,
  "needs_change_soon": false,
  "valid": true,
  "stale": false,
  "timestamp": 1729612800
}
```

**Error Response (404):**
```json
{
  "error": "no_prediction",
  "valid": false,
  "stale": true,
  "message": "No valid prediction available. ML service may not be running."
}
```

### 5. Configuration Files

#### `tools/requirements.txt`
Added:
```
mysql-connector-python>=8.0.0
```

#### `tools/.env.example`
Added:
```bash
# Database Configuration
DB_TYPE=sqlite  # or 'mariadb'

# SQLite Configuration (used if DB_TYPE=sqlite)
DB_FILE=aquarium_data.db

# MariaDB Configuration (used if DB_TYPE=mariadb)
DB_HOST=localhost
DB_PORT=3306
DB_NAME=aquarium
DB_USER=aquarium
DB_PASSWORD=
```

## MQTT Topic Structure

### ESP32 → Python Service
| Topic | Description | Frequency |
|-------|-------------|-----------|
| `aquarium/data` | Sensor readings (temp, pH, TDS, heater, CO2) | Every 30s |
| `aquarium/waterchange/event` | Water change completion event | On completion |
| `aquarium/waterchange/history` | Bulk historical data (for initialization) | On request |
| `aquarium/filter/maintenance` | Filter maintenance event | On maintenance |

### Python Service → ESP32
| Topic | Description | Frequency |
|-------|-------------|-----------|
| `aquarium/ml/prediction` | ML prediction results (retained) | After training |

## Database Comparison

### SQLite (Default)
**Pros:**
- Zero configuration
- File-based (portable)
- Perfect for single-tank systems
- No external dependencies

**Cons:**
- Limited concurrent access
- No remote access
- Single-file size limits

**Use case:** Home aquarium, single ESP32, running Python service on same machine

### MariaDB
**Pros:**
- Excellent for multiple tanks
- Remote access from anywhere
- Better for large datasets (>100k records)
- Built-in backups and replication
- SQL tools for analysis

**Cons:**
- Requires MySQL/MariaDB server installation
- More complex setup
- Needs network connectivity

**Use case:** Multiple tanks, fish farm, remote monitoring, advanced analytics

## Setup Instructions

### For SQLite (Default)
```bash
cd tools/
cp .env.example .env
# Edit .env: DB_TYPE=sqlite
pip3 install -r requirements.txt
python3 water_change_ml_service.py
```

### For MariaDB
```bash
# 1. Install MariaDB
sudo apt install mariadb-server

# 2. Create database and user
sudo mysql -u root -p
```
```sql
CREATE DATABASE aquarium;
CREATE USER 'aquarium'@'localhost' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON aquarium.* TO 'aquarium'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

```bash
# 3. Configure ML service
cd tools/
cp .env.example .env
# Edit .env:
#   DB_TYPE=mariadb
#   DB_HOST=localhost
#   DB_NAME=aquarium
#   DB_USER=aquarium
#   DB_PASSWORD=your_password

# 4. Install dependencies and run
pip3 install -r requirements.txt
python3 water_change_ml_service.py
```

## Web Dashboard Usage

1. Navigate to **Water Change** tab
2. View **AI Water Change Prediction** card at top
3. Monitor:
   - Days until next change
   - Prediction confidence
   - TDS increase rate
   - Filter maintenance status
4. Prediction updates automatically every 60 seconds

## How Filter Maintenance Affects Predictions

### Before Filter Maintenance
- TDS increases at rate: ~15 ppm/day
- Predicted next change: 7 days

### After Filter Maintenance
- TDS increase rate drops to: ~10 ppm/day (cleaner filter removes more waste)
- Predicted next change extends to: ~10 days
- ML model learns this pattern over time

### Key Insights
- **Dirty filters** → Faster TDS increase → More frequent water changes needed
- **Clean filters** → Slower TDS increase → Can extend water change intervals
- **Regular maintenance** → More predictable patterns → Higher confidence predictions

## Monitoring

### Check ML Service Status
```bash
# If running as systemd service
sudo systemctl status aquarium-ml
sudo journalctl -u aquarium-ml -f

# If running manually
# View console output
```

### Check Database
```bash
# SQLite
sqlite3 aquarium_data.db
sqlite> SELECT COUNT(*) FROM water_changes;
sqlite> SELECT COUNT(*) FROM filter_maintenance;
sqlite> SELECT * FROM predictions ORDER BY prediction_timestamp DESC LIMIT 5;

# MariaDB
mysql -u aquarium -p aquarium
mysql> SELECT COUNT(*) FROM water_changes;
mysql> SELECT COUNT(*) FROM filter_maintenance;
mysql> SELECT * FROM predictions ORDER BY prediction_timestamp DESC LIMIT 5;
```

### Check MQTT Traffic
```bash
# Subscribe to all aquarium topics
mosquitto_sub -h localhost -t 'aquarium/#' -v

# View specific topics
mosquitto_sub -h localhost -t 'aquarium/ml/prediction' -v
mosquitto_sub -h localhost -t 'aquarium/waterchange/event' -v
mosquitto_sub -h localhost -t 'aquarium/filter/maintenance' -v
```

## Troubleshooting

### No Prediction Showing on Web Dashboard

**Check 1:** Is ML service running?
```bash
sudo systemctl status aquarium-ml
# or check console if running manually
```

**Check 2:** Is MQTT broker accessible?
```bash
mosquitto_sub -h YOUR_MQTT_HOST -t 'aquarium/ml/prediction' -v
```

**Check 3:** Enough historical data?
```bash
sqlite3 aquarium_data.db "SELECT COUNT(*) FROM water_changes"
# Need at least 5 water changes
```

**Check 4:** Check ESP32 serial output
```
[MQTT] Message on topic: aquarium/ml/prediction
[ML] Prediction received: 6.3 days remaining (confidence: 85%)
```

**Check 5:** View API endpoint directly
```bash
curl http://YOUR_ESP32_IP/api/ml/prediction
```

### Prediction Stale (>24 hours old)

**Cause:** ML service stopped or can't connect to MQTT

**Solution:**
```bash
# Restart ML service
sudo systemctl restart aquarium-ml

# Or run manually to see errors
python3 tools/water_change_ml_service.py
```

### Low Confidence (<50%)

**Causes:**
1. Insufficient training data (<10 water changes)
2. Irregular water change patterns
3. Missing filter maintenance data

**Solutions:**
1. Perform more water changes consistently
2. Establish regular schedule
3. Record filter maintenance when performed

### MariaDB Connection Errors

**Check MariaDB is running:**
```bash
sudo systemctl status mariadb
```

**Test connection:**
```bash
mysql -h YOUR_DB_HOST -u aquarium -p aquarium
```

**Check firewall (if remote):**
```bash
sudo ufw allow 3306/tcp
```

**Verify credentials in `.env`:**
```bash
cat tools/.env | grep DB_
```

## Performance Considerations

### Database Size Growth

**SQLite:**
- ~1 KB per sensor reading
- ~500 bytes per water change
- ~300 bytes per filter maintenance
- ~200 bytes per prediction

**Example:** 1 year of data
- Sensor readings: 30s interval = 1,051,200 readings = ~1 GB
- Water changes: 52 changes = ~26 KB
- Filter maintenance: 26 events = ~8 KB
- Predictions: 365 predictions = ~73 KB

**Total: ~1 GB** (mostly sensor data)

### Optimization Tips
1. Reduce sensor logging frequency (60s instead of 30s)
2. Periodically archive old sensor data
3. Keep only recent sensor data (e.g., last 30 days)
4. Never delete water change or filter maintenance history

### Query Performance
- SQLite: Excellent for <1M records
- MariaDB: Excellent for any size with proper indexing

## Future Enhancements

### Possible Additions
1. **Prediction history chart** - View accuracy over time
2. **What-if scenarios** - "If I clean filter, when is next change?"
3. **Multiple models** - Per-season models for better accuracy
4. **Anomaly detection** - Alert if TDS increases abnormally fast
5. **Mobile app** - View predictions on phone
6. **Multi-tank support** - Separate models per tank

### Experimental Features
- **Feeding schedule integration** - More feeding = faster TDS increase
- **Plant growth phase** - Adjust for high-growth vs low-growth periods
- **Livestock changes** - Track fish additions/removals
- **Water source quality** - Factor in tap water TDS variations

## Summary

The ML prediction system now provides:
- ✅ **Accurate predictions** based on historical patterns
- ✅ **Filter maintenance awareness** for better forecasting
- ✅ **Database flexibility** (SQLite or MariaDB)
- ✅ **Beautiful web dashboard** for easy monitoring
- ✅ **Automatic retraining** on every water change/maintenance
- ✅ **MQTT-based architecture** for scalability

**Minimum requirements for predictions:**
- 5 water changes in history
- Regular maintenance schedule
- ML service running and connected to MQTT
- ESP32 connected to MQTT broker

**Optimal setup:**
- 20+ water changes in history
- Regular filter maintenance (every 2-4 weeks)
- MariaDB for multi-tank or advanced analytics
- Systemd service for automatic startup
- Regular monitoring via web dashboard

Enjoy your AI-powered aquarium maintenance predictions! 🐠🤖
