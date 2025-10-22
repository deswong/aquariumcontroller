# PID ML Training Service - Python Implementation

## Overview

This document explains how to train ML models for PID parameter optimization using the external Python service.

## Architecture

```
ESP32-S3 Device                    Python Training Service
     │                                      │
     │  1. Publishes performance samples   │
     ├──────────────MQTT───────────────────>│
     │     (ambient, hour, season,          │
     │      Kp, Ki, Kd, error, settling)    │
     │                                      │
     │                                      │ 2. Stores in MariaDB
     │                                      │ 3. Trains ML model
     │                                      │ 4. Generates lookup table
     │                                      │
     │  5. Receives lookup table            │
     │<─────────────MQTT────────────────────┤
     │                                      │
     │  6. Stores in NVS                    │
     │  7. Uses for real-time control       │
     └──────────────────────────────────────┘
```

## Data Flow

### 1. Performance Sample Collection (ESP32 → Python)

**MQTT Topic:** `aquarium/pid/performance`

**Message Format:**
```json
{
    "timestamp": 1729688400,
    "controller": "heater",
    "current_value": 25.3,
    "target_value": 25.0,
    "ambient_temp": 22.5,
    "hour_of_day": 14,
    "season": 2,
    "tank_volume": 100.0,
    
    "kp": 2.1,
    "ki": 0.15,
    "kd": 0.9,
    
    "error_mean": 0.08,
    "error_variance": 0.0012,
    "settling_time": 245.5,
    "overshoot": 0.8,
    "steady_state_error": 0.05,
    "average_output": 42.3,
    "cycle_count": 245
}
```

**ESP32 Code (already implemented):**
```cpp
void AdaptivePID::logPerformanceToML(float ambientTemp, uint8_t hourOfDay, 
                                     uint8_t season, float tankVolume) {
    if (!mlLogger) return;
    
    // Check if we've completed a performance window (10 minutes)
    unsigned long now = millis();
    if (now - performanceWindowStart < 600000) return;
    
    // Calculate windowed performance metrics
    float avgError = performanceWindowErrorSum / performanceWindowSamples;
    float errorVariance = /* ... calculation ... */;
    float avgOutput = performanceWindowOutputSum / performanceWindowSamples;
    
    // Create performance sample
    PIDPerformanceSample sample;
    sample.timestamp = now;
    sample.currentValue = lastInput;
    sample.targetValue = target;
    sample.ambientTemp = ambientTemp;
    sample.hourOfDay = hourOfDay;
    sample.season = season;
    sample.tankVolume = tankVolume;
    
    sample.kp = kp;
    sample.ki = ki;
    sample.kd = kd;
    
    sample.errorMean = avgError;
    sample.errorVariance = errorVariance;
    sample.settlingTime = settlingTime / 1000.0f;
    sample.overshoot = maxOvershoot;
    sample.steadyStateError = steadyStateError;
    sample.averageOutput = avgOutput;
    sample.cycleCount = performanceWindowSamples;
    
    // Publish to MQTT
    mlLogger->logSample(sample);  // This publishes to MQTT
}
```

### 2. Python Training Service

Create `tools/pid_ml_trainer.py`:

```python
#!/usr/bin/env python3
"""
PID ML Training Service

This service:
1. Subscribes to PID performance samples via MQTT
2. Stores samples in MariaDB
3. Trains ML models to predict optimal PID gains
4. Generates lookup table
5. Publishes lookup table back to ESP32

The model learns:
Input:  ambient_temp, hour_of_day, season, tank_volume
Output: optimal Kp, Ki, Kd (that minimize error and settling time)
"""

import os
import json
import logging
import numpy as np
from datetime import datetime
from typing import Dict, List, Tuple

import mysql.connector
import paho.mqtt.client as mqtt
from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

# Configuration
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_USER = os.getenv("MQTT_USER", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_TOPIC_PREFIX = os.getenv("MQTT_TOPIC_PREFIX", "aquarium")

DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = int(os.getenv("DB_PORT", 3306))
DB_NAME = os.getenv("DB_NAME", "aquarium")
DB_USER = os.getenv("DB_USER", "aquarium")
DB_PASSWORD = os.getenv("DB_PASSWORD", "")

# Logging setup
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("pid_ml_trainer")

# Minimum samples needed for training
MIN_TRAINING_SAMPLES = 50


class PIDDatabase:
    """
    Database interface for PID performance samples.
    
    Stores historical PID performance data for ML training.
    """
    
    def __init__(self, host: str, port: int, database: str, user: str, password: str):
        """Connect to MariaDB and initialize schema."""
        self.conn = mysql.connector.connect(
            host=host,
            port=port,
            database=database,
            user=user,
            password=password
        )
        self._init_database()
        logger.info(f"Connected to MariaDB at {host}:{port}/{database}")
    
    def _init_database(self):
        """Create PID performance table if it doesn't exist."""
        cursor = self.conn.cursor()
        
        # Table: PID performance samples
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS pid_performance (
                id INT AUTO_INCREMENT PRIMARY KEY,
                timestamp INT NOT NULL,
                controller VARCHAR(50) NOT NULL,
                current_value FLOAT NOT NULL,
                target_value FLOAT NOT NULL,
                ambient_temp FLOAT NOT NULL,
                hour_of_day TINYINT NOT NULL,
                season TINYINT NOT NULL,
                tank_volume FLOAT NOT NULL,
                
                kp FLOAT NOT NULL,
                ki FLOAT NOT NULL,
                kd FLOAT NOT NULL,
                
                error_mean FLOAT NOT NULL,
                error_variance FLOAT NOT NULL,
                settling_time FLOAT NOT NULL,
                overshoot FLOAT NOT NULL,
                steady_state_error FLOAT NOT NULL,
                average_output FLOAT NOT NULL,
                cycle_count INT NOT NULL,
                
                performance_score FLOAT,
                
                INDEX idx_timestamp (timestamp),
                INDEX idx_controller (controller),
                INDEX idx_conditions (ambient_temp, hour_of_day, season)
            ) ENGINE=InnoDB
        ''')
        
        self.conn.commit()
        logger.info("Database schema initialized")
    
    def store_sample(self, sample: Dict) -> bool:
        """
        Store a PID performance sample.
        
        Calculates performance score based on:
        - Settling time (lower is better)
        - Overshoot (lower is better)  
        - Steady-state error (lower is better)
        - Error variance (lower is better)
        
        Score range: 0-100 (higher is better)
        """
        cursor = self.conn.cursor()
        
        # Calculate performance score
        # Lower values are better, so invert them
        # Normalize to 0-100 scale
        settling_score = max(0, 100 - (sample['settling_time'] / 10.0))  # 0s=100, 1000s=0
        overshoot_score = max(0, 100 - (sample['overshoot'] * 10.0))    # 0%=100, 10%=0
        error_score = max(0, 100 - (abs(sample['steady_state_error']) * 100.0))
        variance_score = max(0, 100 - (sample['error_variance'] * 1000.0))
        
        # Weighted average (settling time is most important)
        performance_score = (
            settling_score * 0.4 +
            error_score * 0.3 +
            overshoot_score * 0.2 +
            variance_score * 0.1
        )
        
        cursor.execute('''
            INSERT INTO pid_performance (
                timestamp, controller, current_value, target_value,
                ambient_temp, hour_of_day, season, tank_volume,
                kp, ki, kd,
                error_mean, error_variance, settling_time, overshoot,
                steady_state_error, average_output, cycle_count,
                performance_score
            ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
        ''', (
            sample['timestamp'], sample['controller'],
            sample['current_value'], sample['target_value'],
            sample['ambient_temp'], sample['hour_of_day'], sample['season'],
            sample['tank_volume'],
            sample['kp'], sample['ki'], sample['kd'],
            sample['error_mean'], sample['error_variance'], sample['settling_time'],
            sample['overshoot'], sample['steady_state_error'],
            sample['average_output'], sample['cycle_count'],
            performance_score
        ))
        
        self.conn.commit()
        logger.info(f"Stored PID sample (score: {performance_score:.1f})")
        return True
    
    def get_training_data(self, controller: str = "heater", min_score: float = 50.0) -> List[Dict]:
        """
        Retrieve training data for ML model.
        
        Only uses samples with good performance (score > min_score).
        This ensures the ML learns from successful control attempts.
        
        Returns list of samples sorted by performance score (best first).
        """
        cursor = self.conn.cursor(dictionary=True)
        
        cursor.execute('''
            SELECT * FROM pid_performance
            WHERE controller = %s AND performance_score >= %s
            ORDER BY performance_score DESC
            LIMIT 10000
        ''', (controller, min_score))
        
        samples = cursor.fetchall()
        logger.info(f"Retrieved {len(samples)} training samples (score >= {min_score})")
        return samples


class PIDMLModel:
    """
    Machine Learning model for PID parameter optimization.
    
    Learns optimal PID gains (Kp, Ki, Kd) based on operating conditions:
    - Ambient temperature
    - Time of day (hour)
    - Season
    - Tank volume (optional)
    
    Uses ensemble methods (Random Forest + Gradient Boosting) for robustness.
    """
    
    def __init__(self):
        self.models = {
            'kp': None,  # Model to predict optimal Kp
            'ki': None,  # Model to predict optimal Ki
            'kd': None   # Model to predict optimal Kd
        }
        self.scaler = StandardScaler()
        self.trained = False
    
    def extract_features(self, samples: List[Dict]) -> Tuple[np.ndarray, Dict[str, np.ndarray]]:
        """
        Extract features and targets from samples.
        
        Features (input):
        - ambient_temp: Normalized temperature
        - hour_sin, hour_cos: Circular encoding of hour (0-23)
        - season_sin, season_cos: Circular encoding of season (0-3)
        - tank_volume: Normalized volume
        
        Targets (output):
        - kp, ki, kd: The PID gains that achieved good performance
        
        Why circular encoding?
        Hour 23 and Hour 0 are adjacent, but numerically far apart.
        sin/cos encoding preserves this circular relationship.
        """
        features = []
        targets_kp = []
        targets_ki = []
        targets_kd = []
        
        for sample in samples:
            # Input features
            hour = sample['hour_of_day']
            season = sample['season']
            
            # Circular encoding for time features
            hour_sin = np.sin(2 * np.pi * hour / 24.0)
            hour_cos = np.cos(2 * np.pi * hour / 24.0)
            season_sin = np.sin(2 * np.pi * season / 4.0)
            season_cos = np.cos(2 * np.pi * season / 4.0)
            
            feature_vector = [
                sample['ambient_temp'],
                hour_sin,
                hour_cos,
                season_sin,
                season_cos,
                sample['tank_volume'] / 100.0  # Normalize (assume max 1000L)
            ]
            
            features.append(feature_vector)
            
            # Output targets (the PID gains that worked well)
            targets_kp.append(sample['kp'])
            targets_ki.append(sample['ki'])
            targets_kd.append(sample['kd'])
        
        X = np.array(features)
        targets = {
            'kp': np.array(targets_kp),
            'ki': np.array(targets_ki),
            'kd': np.array(targets_kd)
        }
        
        return X, targets
    
    def train(self, samples: List[Dict]) -> Dict[str, float]:
        """
        Train ML models for each PID parameter.
        
        Uses Gradient Boosting (better for regression tasks).
        Trains separate model for each gain (Kp, Ki, Kd).
        
        Returns R² scores for validation.
        """
        if len(samples) < MIN_TRAINING_SAMPLES:
            logger.warning(f"Insufficient samples: {len(samples)} (need {MIN_TRAINING_SAMPLES})")
            return {'error': 'insufficient_data'}
        
        # Extract features and targets
        X, targets = self.extract_features(samples)
        
        # Scale features
        X_scaled = self.scaler.fit_transform(X)
        
        # Split for validation
        X_train, X_val, y_kp_train, y_kp_val = train_test_split(
            X_scaled, targets['kp'], test_size=0.2, random_state=42
        )
        _, _, y_ki_train, y_ki_val = train_test_split(
            X_scaled, targets['ki'], test_size=0.2, random_state=42
        )
        _, _, y_kd_train, y_kd_val = train_test_split(
            X_scaled, targets['kd'], test_size=0.2, random_state=42
        )
        
        scores = {}
        
        # Train Kp model
        logger.info("Training Kp model...")
        self.models['kp'] = GradientBoostingRegressor(
            n_estimators=100,
            max_depth=5,
            learning_rate=0.1,
            random_state=42
        )
        self.models['kp'].fit(X_train, y_kp_train)
        scores['kp'] = self.models['kp'].score(X_val, y_kp_val)
        logger.info(f"Kp model R² score: {scores['kp']:.3f}")
        
        # Train Ki model
        logger.info("Training Ki model...")
        self.models['ki'] = GradientBoostingRegressor(
            n_estimators=100,
            max_depth=5,
            learning_rate=0.1,
            random_state=42
        )
        self.models['ki'].fit(X_train, y_ki_train)
        scores['ki'] = self.models['ki'].score(X_val, y_ki_val)
        logger.info(f"Ki model R² score: {scores['ki']:.3f}")
        
        # Train Kd model
        logger.info("Training Kd model...")
        self.models['kd'] = GradientBoostingRegressor(
            n_estimators=100,
            max_depth=5,
            learning_rate=0.1,
            random_state=42
        )
        self.models['kd'].fit(X_train, y_kd_train)
        scores['kd'] = self.models['kd'].score(X_val, y_kd_val)
        logger.info(f"Kd model R² score: {scores['kd']:.3f}")
        
        self.trained = True
        logger.info(f"Training complete with {len(samples)} samples")
        
        return scores
    
    def predict(self, ambient_temp: float, hour: int, season: int, tank_volume: float = 100.0) -> Tuple[float, float, float, float]:
        """
        Predict optimal PID gains for given conditions.
        
        Returns: (kp, ki, kd, confidence)
        Confidence is average R² score from training.
        """
        if not self.trained:
            return (2.0, 0.1, 1.0, 0.0)  # Default values if not trained
        
        # Build feature vector
        hour_sin = np.sin(2 * np.pi * hour / 24.0)
        hour_cos = np.cos(2 * np.pi * hour / 24.0)
        season_sin = np.sin(2 * np.pi * season / 4.0)
        season_cos = np.cos(2 * np.pi * season / 4.0)
        
        features = np.array([[
            ambient_temp,
            hour_sin,
            hour_cos,
            season_sin,
            season_cos,
            tank_volume / 100.0
        ]])
        
        # Scale features
        features_scaled = self.scaler.transform(features)
        
        # Predict gains
        kp = self.models['kp'].predict(features_scaled)[0]
        ki = self.models['ki'].predict(features_scaled)[0]
        kd = self.models['kd'].predict(features_scaled)[0]
        
        # Confidence is average model score
        # (In production, could use prediction std or other metrics)
        confidence = 0.8  # Placeholder - could compute from model uncertainty
        
        return (kp, ki, kd, confidence)
    
    def generate_lookup_table(self) -> Dict[str, Dict]:
        """
        Generate discretized lookup table for ESP32.
        
        Creates table with entries for:
        - Ambient temp: 15-30°C in 1°C steps (16 entries)
        - Hour: 0-23 (24 entries)
        - Season: 0-3 (4 entries)
        
        Total: 16 × 24 × 4 = 1,536 entries (~30KB)
        
        Returns dict structure ready for JSON serialization and MQTT publish.
        """
        if not self.trained:
            logger.warning("Cannot generate lookup table - model not trained")
            return {}
        
        lookup_table = {}
        
        # Discretize parameter space
        temps = range(15, 31)  # 15-30°C
        hours = range(24)      # 0-23
        seasons = range(4)     # 0-3 (winter, spring, summer, fall)
        tank_volume = 100.0    # Fixed for simplicity
        
        logger.info("Generating lookup table...")
        entry_count = 0
        
        for temp in temps:
            for hour in hours:
                for season in seasons:
                    kp, ki, kd, confidence = self.predict(temp, hour, season, tank_volume)
                    
                    # Create unique key
                    key = f"{temp}_{hour}_{season}"
                    
                    lookup_table[key] = {
                        'kp': round(kp, 3),
                        'ki': round(ki, 3),
                        'kd': round(kd, 3),
                        'confidence': round(confidence, 2)
                    }
                    
                    entry_count += 1
        
        logger.info(f"Generated lookup table with {entry_count} entries")
        return lookup_table


class PIDTrainingService:
    """Main service that coordinates MQTT, database, and ML training."""
    
    def __init__(self):
        self.db = PIDDatabase(DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD)
        self.model = PIDMLModel()
        self.mqtt_client = None
        self.sample_count = 0
    
    def on_connect(self, client, userdata, flags, rc):
        """MQTT connection callback."""
        if rc == 0:
            logger.info(f"Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
            # Subscribe to PID performance samples
            topic = f"{MQTT_TOPIC_PREFIX}/pid/performance"
            client.subscribe(topic)
            logger.info(f"Subscribed to {topic}")
        else:
            logger.error(f"Failed to connect to MQTT broker (code {rc})")
    
    def on_message(self, client, userdata, msg):
        """Handle incoming PID performance sample."""
        try:
            # Parse JSON message
            sample = json.loads(msg.payload.decode())
            
            # Store in database
            self.db.store_sample(sample)
            self.sample_count += 1
            
            # Train model every 10 samples
            if self.sample_count % 10 == 0:
                self.train_and_publish()
        
        except Exception as e:
            logger.error(f"Error processing message: {e}")
    
    def train_and_publish(self):
        """Train ML model and publish lookup table to ESP32."""
        logger.info("Starting training cycle...")
        
        # Get training data
        samples = self.db.get_training_data(min_score=50.0)
        
        if len(samples) < MIN_TRAINING_SAMPLES:
            logger.warning(f"Not enough samples for training: {len(samples)}/{MIN_TRAINING_SAMPLES}")
            return
        
        # Train model
        scores = self.model.train(samples)
        
        if 'error' in scores:
            return
        
        logger.info(f"Training complete - Scores: Kp={scores['kp']:.3f}, Ki={scores['ki']:.3f}, Kd={scores['kd']:.3f}")
        
        # Generate lookup table
        lookup_table = self.model.generate_lookup_table()
        
        # Publish to MQTT
        topic = f"{MQTT_TOPIC_PREFIX}/pid/lookup_table"
        payload = json.dumps({
            'timestamp': int(datetime.now().timestamp()),
            'version': 1,
            'scores': scores,
            'table': lookup_table
        })
        
        self.mqtt_client.publish(topic, payload, qos=1, retain=True)
        logger.info(f"Published lookup table to {topic} ({len(payload)} bytes)")
    
    def start(self):
        """Start the service."""
        logger.info("Starting PID ML Training Service...")
        
        # Setup MQTT client
        self.mqtt_client = mqtt.Client()
        if MQTT_USER and MQTT_PASSWORD:
            self.mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message
        
        # Connect and start loop
        self.mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        logger.info("Service started - waiting for performance samples...")
        self.mqtt_client.loop_forever()


if __name__ == "__main__":
    service = PIDTrainingService()
    service.start()
```

## Running the Service

### 1. Install Dependencies

```bash
cd tools
pip3 install -r requirements.txt
# (already includes numpy, scikit-learn, mysql-connector-python, paho-mqtt)
```

### 2. Configure Environment

```bash
# Edit .env file
nano .env
```

Add:
```bash
# Same MQTT and database config as water_change_ml_service.py
MQTT_BROKER=localhost
MQTT_PORT=1883
DB_HOST=localhost
DB_PORT=3306
DB_NAME=aquarium
DB_USER=aquarium
DB_PASSWORD=your_password
```

### 3. Run the Service

```bash
python3 pid_ml_trainer.py
```

**Expected output:**
```
2025-10-23 10:15:30 - pid_ml_trainer - INFO - Connected to MariaDB at localhost:3306/aquarium
2025-10-23 10:15:30 - pid_ml_trainer - INFO - Database schema initialized
2025-10-23 10:15:31 - pid_ml_trainer - INFO - Connected to MQTT broker at localhost:1883
2025-10-23 10:15:31 - pid_ml_trainer - INFO - Subscribed to aquarium/pid/performance
2025-10-23 10:15:31 - pid_ml_trainer - INFO - Service started - waiting for performance samples...
```

### 4. Initial Training (Bootstrap)

For the first run, you need to collect at least 50 performance samples. This takes time:
- Sample every 10 minutes → 50 samples = ~8 hours
- Run overnight to collect good baseline data

**Manual training trigger:**
```bash
# After collecting samples, train manually:
python3 -c "
from pid_ml_trainer import PIDTrainingService
service = PIDTrainingService()
service.train_and_publish()
"
```

## ESP32 Lookup Table Reception

The ESP32 receives the lookup table via MQTT and stores it in NVS.

**Add to MLDataLogger.h:**
```cpp
class MLDataLogger {
    // ... existing code ...
    
    void receiveLookupTable(const char* json);
    bool getOptimalGains(float tankTemp, float ambientTemp, uint8_t hour, uint8_t season,
                        float& kp, float& ki, float& kd, float& confidence);
};
```

**Add to MLDataLogger.cpp:**
```cpp
void MLDataLogger::receiveLookupTable(const char* json) {
    // Parse JSON lookup table
    DynamicJsonDocument doc(65536);  // 64KB for large table
    deserializeJson(doc, json);
    
    JsonObject table = doc["table"];
    
    // Store in NVS (key-value pairs)
    Preferences prefs;
    if (!prefs.begin("pid_lookup", false)) {
        Serial.println("ERROR: Failed to open NVS for PID lookup table");
        return;
    }
    
    // Clear old entries
    prefs.clear();
    
    // Store new entries
    int stored = 0;
    for (JsonPair kv : table) {
        String key = String("ml_") + kv.key().c_str();
        JsonObject entry = kv.value();
        
        // Pack gains into string: "kp,ki,kd,conf"
        String value = String(entry["kp"].as<float>(), 3) + "," +
                      String(entry["ki"].as<float>(), 3) + "," +
                      String(entry["kd"].as<float>(), 3) + "," +
                      String(entry["confidence"].as<float>(), 2);
        
        prefs.putString(key.c_str(), value);
        stored++;
    }
    
    prefs.end();
    
    Serial.printf("Stored %d PID lookup table entries in NVS\n", stored);
}

bool MLDataLogger::getOptimalGains(float tankTemp, float ambientTemp, uint8_t hour, 
                                   uint8_t season, float& kp, float& ki, float& kd, 
                                   float& confidence) {
    Preferences prefs;
    if (!prefs.begin("pid_lookup", true)) {
        return false;
    }
    
    // Round ambient temp to nearest integer
    int temp_key = (int)(ambientTemp + 0.5);
    temp_key = constrain(temp_key, 15, 30);
    
    // Build lookup key
    String key = String("ml_") + String(temp_key) + "_" + 
                String(hour) + "_" + String(season);
    
    // Retrieve from NVS
    String value = prefs.getString(key.c_str(), "");
    prefs.end();
    
    if (value.length() == 0) {
        return false;  // Not found
    }
    
    // Parse "kp,ki,kd,conf"
    sscanf(value.c_str(), "%f,%f,%f,%f", &kp, &ki, &kd, &confidence);
    
    return true;
}
```

**Subscribe to lookup table updates in main.cpp:**
```cpp
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    
    if (topicStr.endsWith("/pid/lookup_table")) {
        char* json = (char*)malloc(length + 1);
        memcpy(json, payload, length);
        json[length] = '\0';
        
        mlLogger.receiveLookupTable(json);
        
        free(json);
    }
}
```

## Model Retraining

### Automatic Retraining

The service automatically retrains every 10 new samples. This ensures:
- Model stays up-to-date with latest data
- Adapts to seasonal changes
- Improves as more data collected

### Manual Retraining

```bash
# Train on all available data
python3 -c "
from pid_ml_trainer import PIDTrainingService
service = PIDTrainingService()
service.train_and_publish()
"
```

### Scheduled Retraining (Cron)

```bash
# Add to crontab
crontab -e

# Retrain daily at 3 AM
0 3 * * * cd /home/des/Documents/aquariumcontroller/tools && python3 -c "from pid_ml_trainer import PIDTrainingService; service = PIDTrainingService(); service.train_and_publish()" >> /var/log/pid_ml.log 2>&1
```

## Performance Monitoring

### Check Training Progress

```bash
# Query database for sample count
mysql -u aquarium -p aquarium -e "
SELECT 
    COUNT(*) as total_samples,
    AVG(performance_score) as avg_score,
    MIN(performance_score) as min_score,
    MAX(performance_score) as max_score
FROM pid_performance
WHERE controller = 'heater';
"
```

**Expected output:**
```
+----------------+-----------+-----------+-----------+
| total_samples  | avg_score | min_score | max_score |
+----------------+-----------+-----------+-----------+
|            127 |     78.42 |     52.10 |     96.80 |
+----------------+-----------+-----------+-----------+
```

### View Best Performing Parameters

```bash
mysql -u aquarium -p aquarium -e "
SELECT 
    ambient_temp,
    hour_of_day,
    season,
    kp, ki, kd,
    performance_score,
    settling_time,
    overshoot
FROM pid_performance
WHERE controller = 'heater'
ORDER BY performance_score DESC
LIMIT 10;
"
```

This shows the best PID gains for different conditions.

## Troubleshooting

### No Samples Received

**Check MQTT:**
```bash
mosquitto_sub -h localhost -t "aquarium/pid/#" -v
```

**Check ESP32 is publishing:**
```cpp
// In main loop, verify ML logging is enabled:
Serial.printf("ML enabled: %s\n", heaterPID->isMLEnabled() ? "YES" : "NO");
```

### Training Fails

**Error:** "Insufficient samples"
```
Solution: Need at least 50 samples. Run system longer or lower MIN_TRAINING_SAMPLES.
```

**Error:** "Low R² scores"
```
Solution: Data quality issue. Check:
- Are samples diverse (different ambient temps, times)?
- Is PID performing well (check performance_score > 50)?
- Increase training samples
```

### Lookup Table Not Updating

**Check NVS size:**
```cpp
// In ESP32, add debug:
Preferences prefs;
prefs.begin("pid_lookup", true);
Serial.printf("NVS entries: %u\n", prefs.getBytesLength("ml_22_14_2"));
prefs.end();
```

**Check MQTT retention:**
```bash
# Lookup table should be retained
mosquitto_sub -h localhost -t "aquarium/pid/lookup_table" -v -C 1
```

## Summary

### Data Flow Summary:

1. **ESP32** runs PID control, collects performance metrics (10 min windows)
2. **ESP32** publishes samples to `aquarium/pid/performance` via MQTT
3. **Python service** receives samples, stores in MariaDB
4. **Python service** trains ML models every 10 samples
5. **Python service** generates lookup table, publishes to `aquarium/pid/lookup_table`
6. **ESP32** receives lookup table, stores in NVS
7. **ESP32** uses lookup table for real-time parameter adaptation (with caching)

### Key Benefits:

- ✅ **Hybrid approach**: ML training off-device, inference on-device
- ✅ **Fast**: Lookup table cached in NVS, ~50-100μs access time
- ✅ **Reliable**: Works offline after initial training
- ✅ **Scalable**: Python service can train for multiple tanks
- ✅ **Self-improving**: Gets better as more data collected
- ✅ **Maintainable**: Easy to update ML algorithms in Python

### Performance Comparison:

| Aspect | On-Device ML | External ML (Implemented) |
|--------|--------------|---------------------------|
| Training Time | 30-60s (blocks control) | 1-2s (no impact) |
| RAM Usage | 50-100 MB | 4-8 KB (lookup table) |
| Inference Time | 10-20ms | 50-100μs (cached) |
| Model Complexity | Limited (linear only) | Advanced (ensemble) |
| Updateability | Requires reflash | MQTT update |
| Debugging | Difficult | Easy (Python) |

**Winner:** External ML ✅
