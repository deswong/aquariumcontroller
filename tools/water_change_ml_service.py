#!/usr/bin/env python3
"""
Aquarium Water Change ML Predictor Service

This service:
1. Subscribes to MQTT for sensor data, water change events, and filter maintenance
2. Collects historical data
3. Trains ML models to predict next water change (including filter maintenance impact)
4. Publishes predictions back to MQTT for ESP32 to display

Run daily via cron or as a persistent service.
Supports both SQLite and MariaDB databases.
"""

import json
import logging
import os
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

import mysql.connector
import numpy as np
import paho.mqtt.client as mqtt
from sklearn.ensemble import GradientBoostingRegressor, RandomForestRegressor
from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import StandardScaler

# Configuration
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_TOPIC_PREFIX = os.getenv("MQTT_TOPIC_PREFIX", "aquarium")

# Topics
TOPIC_SENSOR_DATA = f"{MQTT_TOPIC_PREFIX}/data"
TOPIC_WC_HISTORY = f"{MQTT_TOPIC_PREFIX}/waterchange/history"
TOPIC_WC_EVENT = f"{MQTT_TOPIC_PREFIX}/waterchange/event"
TOPIC_FILTER_MAINTENANCE = f"{MQTT_TOPIC_PREFIX}/filter/maintenance"
TOPIC_PREDICTION = f"{MQTT_TOPIC_PREFIX}/ml/prediction"

# MariaDB Configuration
DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = int(os.getenv("DB_PORT", "3306"))
DB_NAME = os.getenv("DB_NAME", "aquarium")
DB_USER = os.getenv("DB_USER", "aquarium")
DB_PASSWORD = os.getenv("DB_PASSWORD", "")

# ML Parameters
MIN_TRAINING_SAMPLES = 5  # Minimum water changes needed for prediction
PREDICTION_CONFIDENCE_THRESHOLD = 0.6

# Logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class AquariumDatabase:
    """MariaDB database for storing aquarium sensor data and water change history"""
    
    def __init__(self, host: str = "localhost", port: int = 3306, 
                 database: str = "aquarium", user: str = "aquarium", password: str = ""):
        self.db_config = {
            'host': host,
            'port': port,
            'database': database,
            'user': user,
            'password': password
        }
        self.conn = None
        self._init_database()
    
    def _init_database(self):
        """Initialize MariaDB database and create tables"""
        self.conn = mysql.connector.connect(**self.db_config)
        cursor = self.conn.cursor()
        
        # Sensor readings table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_readings (
                id INT AUTO_INCREMENT PRIMARY KEY,
                timestamp BIGINT NOT NULL,
                temperature FLOAT,
                ambient_temp FLOAT,
                ph FLOAT,
                tds FLOAT,
                heater_state TINYINT,
                co2_state TINYINT,
                INDEX idx_timestamp (timestamp)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ''')
        
        # Water change events table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS water_changes (
                id INT AUTO_INCREMENT PRIMARY KEY,
                start_timestamp BIGINT NOT NULL,
                end_timestamp BIGINT NOT NULL,
                volume_litres FLOAT NOT NULL,
                temp_before FLOAT,
                temp_after FLOAT,
                ph_before FLOAT,
                ph_after FLOAT,
                tds_before FLOAT,
                tds_after FLOAT,
                duration_minutes INT,
                completed TINYINT DEFAULT 1,
                INDEX idx_end_timestamp (end_timestamp)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ''')
        
        # Filter maintenance table
        cursor.execute('''
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
        ''')
        
        # Predictions table (for tracking accuracy)
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS predictions (
                id INT AUTO_INCREMENT PRIMARY KEY,
                prediction_timestamp BIGINT NOT NULL,
                predicted_days FLOAT,
                confidence FLOAT,
                model_type VARCHAR(50),
                actual_days FLOAT,
                error FLOAT,
                INDEX idx_prediction_timestamp (prediction_timestamp)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ''')
        
        self.conn.commit()
        logger.info("MariaDB database initialized")
    
    def store_sensor_reading(self, data: Dict):
        """Store a sensor reading"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO sensor_readings 
            (timestamp, temperature, ambient_temp, ph, tds, heater_state, co2_state)
            VALUES (%s, %s, %s, %s, %s, %s, %s)
        ''', (
            data.get('timestamp', int(datetime.now().timestamp())),
            data.get('temperature'),
            data.get('ambient_temp'),
            data.get('ph'),
            data.get('tds'),
            1 if data.get('heater') == 'ON' else 0,
            1 if data.get('co2') == 'ON' else 0
        ))
        self.conn.commit()
    
    def store_water_change(self, wc_data: Dict):
        """Store a water change event"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO water_changes 
            (start_timestamp, end_timestamp, volume_litres, 
             temp_before, temp_after, ph_before, ph_after, 
             tds_before, tds_after, duration_minutes, completed)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
        ''', (
            wc_data.get('startTime'),
            wc_data.get('endTime'),
            wc_data.get('volume'),
            wc_data.get('tempBefore'),
            wc_data.get('tempAfter'),
            wc_data.get('phBefore'),
            wc_data.get('phAfter'),
            wc_data.get('tdsBefore'),
            wc_data.get('tdsAfter'),
            wc_data.get('duration'),
            1 if wc_data.get('successful', True) else 0
        ))
        self.conn.commit()
        logger.info(f"Water change stored: {wc_data.get('volume')}L at {datetime.fromtimestamp(wc_data.get('endTime'))}")
    
    def store_filter_maintenance(self, fm_data: Dict):
        """Store a filter maintenance event"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO filter_maintenance 
            (timestamp, filter_type, days_since_last, tds_before, tds_after, notes)
            VALUES (%s, %s, %s, %s, %s, %s)
        ''', (
            fm_data.get('timestamp', int(datetime.now().timestamp())),
            fm_data.get('filter_type', 'mechanical'),
            fm_data.get('days_since_last', 0),
            fm_data.get('tds_before'),
            fm_data.get('tds_after'),
            fm_data.get('notes', '')
        ))
        self.conn.commit()
        logger.info(f"Filter maintenance stored at {datetime.fromtimestamp(fm_data.get('timestamp', int(datetime.now().timestamp())))}")
    
    def get_water_change_history(self, limit: int = 50) -> List[Dict]:
        """Get recent water change history"""
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT * FROM water_changes 
            WHERE completed = 1
            ORDER BY end_timestamp DESC 
            LIMIT %s
        ''', (limit,))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    def get_filter_maintenance_history(self, limit: int = 50) -> List[Dict]:
        """Get recent filter maintenance history"""
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT * FROM filter_maintenance 
            ORDER BY timestamp DESC 
            LIMIT %s
        ''', (limit,))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    def get_last_filter_maintenance(self) -> Optional[Dict]:
        """Get the most recent filter maintenance"""
        history = self.get_filter_maintenance_history(limit=1)
        return history[0] if history else None
    
    def get_sensor_readings_between(self, start_ts: int, end_ts: int) -> List[Dict]:
        """Get sensor readings between two timestamps"""
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT * FROM sensor_readings 
            WHERE timestamp BETWEEN %s AND %s
            ORDER BY timestamp ASC
        ''', (start_ts, end_ts))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    def get_recent_sensor_readings(self, hours: int = 24) -> List[Dict]:
        """Get sensor readings from the last N hours"""
        end_ts = int(datetime.now().timestamp())
        start_ts = end_ts - (hours * 3600)
        return self.get_sensor_readings_between(start_ts, end_ts)
    
    def store_prediction(self, days: float, confidence: float, model_type: str):
        """Store a prediction for future accuracy tracking"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO predictions 
            (prediction_timestamp, predicted_days, confidence, model_type)
            VALUES (%s, %s, %s, %s)
        ''', (int(datetime.now().timestamp()), days, confidence, model_type))
        self.conn.commit()
    
    def update_prediction_actual(self, prediction_id: int, actual_days: float):
        """Update a prediction with actual outcome"""
        cursor = self.conn.cursor()
        cursor.execute('''
            UPDATE predictions 
            SET actual_days = ?, error = ABS(predicted_days - ?)
            WHERE id = ?
        ''', (actual_days, actual_days, prediction_id))
        self.conn.commit()
    
    def close(self):
        """Close database connection"""
        if self.conn:
            self.conn.close()


class WaterChangePredictor:
    """ML-based water change predictor"""
    
    def __init__(self, db: AquariumDatabase):
        self.db = db
        self.models = {
            'linear': LinearRegression(),
            'random_forest': RandomForestRegressor(n_estimators=50, random_state=42),
            'gradient_boost': GradientBoostingRegressor(n_estimators=50, random_state=42)
        }
        self.scaler = StandardScaler()
        self.best_model_name = None
        self.best_model = None
    
    def extract_features(self, history: List[Dict]) -> Tuple[np.ndarray, np.ndarray]:
        """Extract features from water change history with filter maintenance impact
        
        Features:
        - Days since previous water change
        - TDS before change
        - TDS after change
        - TDS increase rate
        - Volume changed
        - pH before
        - pH after
        - Temperature before
        - Temperature after
        - Day of week
        - Season (encoded as 0-3)
        - Days since last filter maintenance
        - Filter maintenance in period (0 or 1)
        - TDS change from filter maintenance
        
        Target: Days until next water change
        """
        features = []
        targets = []
        filter_history = self.db.get_filter_maintenance_history(limit=100)
        
        # History should be in chronological order (oldest first) when passed here
        for i in range(len(history) - 1):
            current = history[i]  # Current water change
            next_change = history[i + 1]  # Next (future) water change
            
            # Calculate days until next change (target variable)
            days_until_next = (next_change['end_timestamp'] - current['end_timestamp']) / 86400.0
            
            if days_until_next <= 0:
                continue  # Skip invalid data
            
            # Calculate days since previous change
            if i > 0:
                prev = history[i - 1]
                days_since_prev = (current['end_timestamp'] - prev['end_timestamp']) / 86400.0
            else:
                days_since_prev = 7.0  # Default assumption for first entry
            
            # Calculate TDS increase rate from current water change
            tds_increase = current['tds_before'] - current['tds_after']
            if days_since_prev > 0:
                tds_rate = tds_increase / days_since_prev
            else:
                tds_rate = 0
            
            # Extract datetime features from current change (this is what we're predicting FROM)
            dt = datetime.fromtimestamp(current['end_timestamp'])
            day_of_week = dt.weekday()
            season = (dt.month % 12) // 3  # 0=Winter, 1=Spring, 2=Summer, 3=Fall
            
            # Calculate filter maintenance features
            days_since_filter = 999  # Default: no filter maintenance
            filter_in_period = 0
            tds_change_from_filter = 0
            
            for fm in filter_history:
                fm_ts = fm['timestamp']
                # Check if filter maintenance occurred between current and next_change
                if current['end_timestamp'] <= fm_ts <= next_change['end_timestamp']:
                    filter_in_period = 1
                    if fm.get('tds_before') and fm.get('tds_after'):
                        tds_change_from_filter = fm['tds_before'] - fm['tds_after']
                
                # Calculate days since most recent filter maintenance at time of current change
                if fm_ts <= current['end_timestamp']:
                    days_since_filter = (current['end_timestamp'] - fm_ts) / 86400.0
                    break  # Found most recent, stop looking
            
            feature_vector = [
                days_since_prev,
                current.get('tds_before', 0) or 0,
                current.get('tds_after', 0) or 0,
                tds_rate,
                current.get('volume_litres', 0) or 0,
                current.get('ph_before', 7.0) or 7.0,
                current.get('ph_after', 7.0) or 7.0,
                current.get('temp_before', 25.0) or 25.0,
                current.get('temp_after', 25.0) or 25.0,
                day_of_week,
                season,
                days_since_filter,
                filter_in_period,
                tds_change_from_filter
            ]
            
            features.append(feature_vector)
            targets.append(days_until_next)
        
        return np.array(features), np.array(targets)
    
    def train(self) -> Dict[str, float]:
        """Train models and select the best one
        
        Returns:
            Dict with model scores and selected model
        """
        history = self.db.get_water_change_history(limit=100)
        
        if len(history) < MIN_TRAINING_SAMPLES:
            logger.warning(f"Insufficient data for training: {len(history)} samples (need {MIN_TRAINING_SAMPLES})")
            return {'error': 'insufficient_data', 'samples': len(history)}
        
        # Reverse to chronological order (oldest first)
        history.reverse()
        
        X, y = self.extract_features(history)
        
        if len(X) < MIN_TRAINING_SAMPLES:
            logger.warning(f"Insufficient valid features: {len(X)} samples")
            return {'error': 'insufficient_features', 'samples': len(X)}
        
        # Scale features
        X_scaled = self.scaler.fit_transform(X)
        
        # Train each model and track scores
        scores = {}
        for name, model in self.models.items():
            try:
                model.fit(X_scaled, y)
                score = model.score(X_scaled, y)
                scores[name] = score
                logger.info(f"Model {name} R² score: {score:.3f}")
            except Exception as e:
                logger.error(f"Error training {name}: {e}")
                scores[name] = 0.0
        
        # Select best model
        self.best_model_name = max(scores, key=scores.get)
        self.best_model = self.models[self.best_model_name]
        
        logger.info(f"Best model selected: {self.best_model_name} (R²={scores[self.best_model_name]:.3f})")
        
        return {
            'model': self.best_model_name,
            'score': scores[self.best_model_name],
            'all_scores': scores,
            'training_samples': len(X)
        }
    
    def predict(self, current_data: Optional[Dict] = None) -> Dict:
        """Predict days until next water change
        
        Args:
            current_data: Current sensor readings (optional, will fetch if not provided)
        
        Returns:
            Dict with prediction, confidence, and metadata
        """
        if not self.best_model:
            return {
                'error': 'no_trained_model',
                'predicted_days': None,
                'confidence': 0.0
            }
        
        history = self.db.get_water_change_history(limit=2)
        
        if len(history) < 1:
            return {
                'error': 'no_history',
                'predicted_days': None,
                'confidence': 0.0
            }
        
        last_change = history[0]
        
        # Get current sensor data
        if not current_data:
            recent_readings = self.db.get_recent_sensor_readings(hours=1)
            if recent_readings:
                current_data = recent_readings[-1]
            else:
                current_data = {}
        
        # Calculate current TDS increase rate
        days_since_last = (datetime.now().timestamp() - last_change['end_timestamp']) / 86400.0
        current_tds = current_data.get('tds', last_change.get('tds_after', 0))
        tds_increase = current_tds - last_change.get('tds_after', 0)
        tds_rate = tds_increase / days_since_last if days_since_last > 0 else 0
        
        # Build feature vector for current state
        dt = datetime.now()
        day_of_week = dt.weekday()
        season = (dt.month % 12) // 3
        
        # Use historical average for days_since_prev if available
        if len(history) >= 2:
            prev_change = history[1]
            days_since_prev = (last_change['end_timestamp'] - prev_change['end_timestamp']) / 86400.0
        else:
            days_since_prev = 7.0
        
        # Calculate filter maintenance features
        last_filter = self.db.get_last_filter_maintenance()
        days_since_filter = 999
        filter_in_period = 0
        tds_change_from_filter = 0
        
        if last_filter:
            days_since_filter = (datetime.now().timestamp() - last_filter['timestamp']) / 86400.0
            
            # Check if filter maintenance occurred since last water change
            if last_filter['timestamp'] > last_change['end_timestamp']:
                filter_in_period = 1
                if last_filter.get('tds_before') and last_filter.get('tds_after'):
                    tds_change_from_filter = last_filter['tds_before'] - last_filter['tds_after']
        
        feature_vector = np.array([[
            days_since_prev,
            current_tds,
            last_change.get('tds_after', 0) or 0,
            tds_rate,
            last_change.get('volume_litres', 0) or 0,
            current_data.get('ph', 7.0) or 7.0,
            last_change.get('ph_after', 7.0) or 7.0,
            current_data.get('temperature', 25.0) or 25.0,
            last_change.get('temp_after', 25.0) or 25.0,
            day_of_week,
            season,
            days_since_filter,
            filter_in_period,
            tds_change_from_filter
        ]])
        
        # Scale and predict
        feature_scaled = self.scaler.transform(feature_vector)
        predicted_days = self.best_model.predict(feature_scaled)[0]
        
        # Calculate days remaining (subtract days already elapsed)
        days_remaining = max(0, predicted_days - days_since_last)
        
        # Calculate confidence based on model score and data quality
        X, y = self.extract_features(self.db.get_water_change_history(limit=50)[::-1])
        if len(X) > 0:
            X_scaled = self.scaler.transform(X)
            model_score = self.best_model.score(X_scaled, y)
            # Confidence is model R² score, capped at 100%
            confidence = min(1.0, max(0.0, model_score))
        else:
            confidence = 0.5
        
        # Adjust confidence based on amount of training data
        data_confidence = min(1.0, len(X) / 20.0)  # Full confidence at 20+ samples
        confidence *= data_confidence
        
        result = {
            'predicted_days_remaining': round(days_remaining, 1),
            'predicted_total_cycle_days': round(predicted_days, 1),
            'days_since_last_change': round(days_since_last, 1),
            'confidence': round(confidence, 2),
            'model': self.best_model_name,
            'current_tds': current_tds,
            'tds_increase_rate': round(tds_rate, 2),
            'needs_change_soon': days_remaining < 1.0,
            'timestamp': int(datetime.now().timestamp())
        }
        
        logger.info(f"Prediction: {days_remaining:.1f} days remaining (confidence: {confidence:.0%})")
        
        return result


class MQTTHandler:
    """MQTT client handler"""
    
    def __init__(self, db: AquariumDatabase, predictor: WaterChangePredictor):
        self.db = db
        self.predictor = predictor
        self.client = mqtt.Client()
        self.connected = False
        
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        # Set credentials if provided
        if MQTT_USER and MQTT_PASSWORD:
            self.client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback for when connected to MQTT broker"""
        if rc == 0:
            self.connected = True
            logger.info(f"Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
            
            # Subscribe to topics
            client.subscribe(TOPIC_SENSOR_DATA)
            client.subscribe(TOPIC_WC_HISTORY)
            client.subscribe(TOPIC_WC_EVENT)
            client.subscribe(TOPIC_FILTER_MAINTENANCE)
            logger.info(f"Subscribed to topics: {TOPIC_SENSOR_DATA}, {TOPIC_WC_HISTORY}, {TOPIC_WC_EVENT}, {TOPIC_FILTER_MAINTENANCE}")
        else:
            logger.error(f"Failed to connect to MQTT broker, return code: {rc}")
    
    def on_disconnect(self, client, userdata, rc):
        """Callback for when disconnected from MQTT broker"""
        self.connected = False
        logger.warning(f"Disconnected from MQTT broker, return code: {rc}")
    
    def on_message(self, client, userdata, msg):
        """Callback for when a message is received"""
        try:
            payload = json.loads(msg.payload.decode())
            
            if msg.topic == TOPIC_SENSOR_DATA:
                # Store sensor reading
                self.db.store_sensor_reading(payload)
                logger.debug(f"Sensor data stored: TDS={payload.get('tds')}, pH={payload.get('ph')}")
            
            elif msg.topic == TOPIC_WC_EVENT:
                # Water change event (real-time)
                self.db.store_water_change(payload)
                logger.info(f"Water change event received and stored")
                
                # Retrain model with new data
                logger.info("Retraining model with new water change data...")
                train_result = self.predictor.train()
                logger.info(f"Training complete: {train_result}")
                
                # Generate new prediction
                prediction = self.predictor.predict()
                self.publish_prediction(prediction)
            
            elif msg.topic == TOPIC_FILTER_MAINTENANCE:
                # Filter maintenance event
                self.db.store_filter_maintenance(payload)
                logger.info(f"Filter maintenance event received and stored")
                
                # Retrain model as filter maintenance affects water quality
                logger.info("Retraining model with filter maintenance data...")
                train_result = self.predictor.train()
                logger.info(f"Training complete: {train_result}")
                
                # Generate new prediction
                prediction = self.predictor.predict()
                self.publish_prediction(prediction)
            
            elif msg.topic == TOPIC_WC_HISTORY:
                # Bulk water change history (for initialization)
                if isinstance(payload, list):
                    for wc in payload:
                        self.db.store_water_change(wc)
                    logger.info(f"Stored {len(payload)} historical water changes")
        
        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse JSON from {msg.topic}: {e}")
        except Exception as e:
            logger.error(f"Error processing message from {msg.topic}: {e}")
    
    def publish_prediction(self, prediction: Dict):
        """Publish prediction to MQTT"""
        try:
            payload = json.dumps(prediction)
            result = self.client.publish(TOPIC_PREDICTION, payload, retain=True)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                logger.info(f"Prediction published: {prediction.get('predicted_days_remaining')} days")
            else:
                logger.error(f"Failed to publish prediction, return code: {result.rc}")
        except Exception as e:
            logger.error(f"Error publishing prediction: {e}")
    
    def connect(self):
        """Connect to MQTT broker"""
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.client.loop_start()
        except Exception as e:
            logger.error(f"Failed to connect to MQTT broker: {e}")
            raise
    
    def disconnect(self):
        """Disconnect from MQTT broker"""
        self.client.loop_stop()
        self.client.disconnect()


def main():
    """Main entry point"""
    logger.info("Starting Aquarium Water Change ML Predictor Service")
    logger.info(f"Connecting to MariaDB at {DB_HOST}:{DB_PORT}/{DB_NAME}")
    
    # Initialize MariaDB database
    db = AquariumDatabase(
        host=DB_HOST,
        port=DB_PORT,
        database=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD
    )
    
    # Initialize predictor
    predictor = WaterChangePredictor(db)
    
    # Train initial model
    logger.info("Training initial model...")
    train_result = predictor.train()
    logger.info(f"Initial training complete: {train_result}")
    
    # Generate initial prediction
    if 'error' not in train_result:
        prediction = predictor.predict()
        logger.info(f"Initial prediction: {prediction}")
    
    # Initialize MQTT handler
    mqtt_handler = MQTTHandler(db, predictor)
    
    try:
        # Connect to MQTT
        mqtt_handler.connect()
        
        # Keep running
        logger.info("Service running. Press Ctrl+C to stop.")
        import time
        while True:
            time.sleep(1)
            
            # Periodically retrain (every 6 hours)
            # This will also happen automatically when new water change events arrive
            if int(time.time()) % (6 * 3600) == 0:
                logger.info("Periodic retraining...")
                train_result = predictor.train()
                prediction = predictor.predict()
                mqtt_handler.publish_prediction(prediction)
    
    except KeyboardInterrupt:
        logger.info("Service stopped by user")
    except Exception as e:
        logger.error(f"Service error: {e}")
    finally:
        mqtt_handler.disconnect()
        db.close()
        logger.info("Service shut down")


if __name__ == "__main__":
    main()
