#!/usr/bin/env python3
"""
Unified Aquarium ML Service

This service combines:
1. PID gain optimization (temperature and CO2 controllers)
2. Water change prediction
3. Sensor data collection
4. Performance tracking

Run as a systemd service on Linux for continuous operation.

Features:
- Shared MariaDB connection
- Shared MQTT client
- Coordinated training schedule
- Minimal resource usage
- Automatic recovery from errors

Usage:
    # Run as persistent service
    python3 aquarium_ml_service.py

    # Train PID models only
    python3 aquarium_ml_service.py --pid-only

    # Train water change model only
    python3 aquarium_ml_service.py --waterchange-only

    # One-time training (no service loop)
    python3 aquarium_ml_service.py --once

Systemd Service:
    sudo cp aquarium_ml_service.py /opt/aquarium/
    sudo cp aquarium-ml.service /etc/systemd/system/
    sudo systemctl enable aquarium-ml
    sudo systemctl start aquarium-ml
"""

import argparse
import json
import logging
import os
import signal
import sys
import time
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

import mysql.connector
import numpy as np
import paho.mqtt.client as mqtt
from sklearn.ensemble import GradientBoostingRegressor, RandomForestRegressor
from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import StandardScaler

# ============================================================================
# Configuration
# ============================================================================

# MQTT Configuration
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_TOPIC_PREFIX = os.getenv("MQTT_TOPIC_PREFIX", "aquarium")

# MQTT Topics
TOPIC_SENSOR_DATA = f"{MQTT_TOPIC_PREFIX}/data"
TOPIC_PID_PERFORMANCE = f"{MQTT_TOPIC_PREFIX}/pid/performance"
TOPIC_PID_GAINS = f"{MQTT_TOPIC_PREFIX}/ml/pid/gains"
TOPIC_WC_HISTORY = f"{MQTT_TOPIC_PREFIX}/waterchange/history"
TOPIC_WC_EVENT = f"{MQTT_TOPIC_PREFIX}/waterchange/event"
TOPIC_WC_PREDICTION = f"{MQTT_TOPIC_PREFIX}/ml/prediction"
TOPIC_FILTER_MAINTENANCE = f"{MQTT_TOPIC_PREFIX}/filter/maintenance"

# MariaDB Configuration
DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = int(os.getenv("DB_PORT", "3306"))
DB_NAME = os.getenv("DB_NAME", "aquarium")
DB_USER = os.getenv("DB_USER", "aquarium")
DB_PASSWORD = os.getenv("DB_PASSWORD", "aquarium")

# Training Parameters
MIN_PID_SAMPLES = 50           # Minimum samples for PID training
MIN_WC_SAMPLES = 5             # Minimum water changes for prediction
PID_TRAIN_INTERVAL = 6 * 3600  # Train PID every 6 hours
WC_TRAIN_INTERVAL = 24 * 3600  # Train water change predictor daily
CONFIDENCE_THRESHOLD = 0.6      # Minimum confidence to publish predictions

# Logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler('/var/log/aquarium-ml.log')
    ] if os.path.exists('/var/log') else [logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger(__name__)

# Global shutdown flag
shutdown_requested = False


# ============================================================================
# Database Manager
# ============================================================================

class AquariumDatabase:
    """Unified database manager for all ML operations"""
    
    def __init__(self, host: str, port: int, database: str, user: str, password: str):
        self.db_config = {
            'host': host,
            'port': port,
            'database': database,
            'user': user,
            'password': password,
            'autocommit': True,
            'pool_name': 'aquarium_pool',
            'pool_size': 5
        }
        self.conn = None
        self._init_database()
    
    def _init_database(self):
        """Initialize database and create all required tables"""
        try:
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
            
            # PID performance tracking
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS pid_performance (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    timestamp BIGINT NOT NULL,
                    controller VARCHAR(10) NOT NULL,
                    kp FLOAT NOT NULL,
                    ki FLOAT NOT NULL,
                    kd FLOAT NOT NULL,
                    settling_time FLOAT,
                    overshoot FLOAT,
                    steady_state_error FLOAT,
                    temperature FLOAT,
                    ambient_temp FLOAT,
                    tds FLOAT,
                    ph FLOAT,
                    hour INT,
                    season INT,
                    tank_volume FLOAT,
                    INDEX idx_controller_timestamp (controller, timestamp),
                    INDEX idx_season (season)
                ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
            ''')
            
            # Optimized PID gains
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS pid_gains (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    timestamp BIGINT NOT NULL,
                    controller VARCHAR(10) NOT NULL,
                    kp FLOAT NOT NULL,
                    ki FLOAT NOT NULL,
                    kd FLOAT NOT NULL,
                    confidence FLOAT,
                    model_type VARCHAR(50),
                    season INT,
                    INDEX idx_controller_season (controller, season)
                ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
            ''')
            
            # Water change events
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
            
            # Filter maintenance
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
            
            # Water change predictions (accuracy tracking)
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS wc_predictions (
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
            logger.info("Database initialized successfully")
            
        except mysql.connector.Error as e:
            logger.error(f"Database initialization error: {e}")
            raise
    
    def reconnect(self):
        """Reconnect to database if connection is lost"""
        try:
            if self.conn:
                self.conn.close()
            self.conn = mysql.connector.connect(**self.db_config)
            logger.info("Database reconnected")
        except mysql.connector.Error as e:
            logger.error(f"Database reconnection error: {e}")
            raise
    
    def ensure_connection(self):
        """Ensure database connection is active"""
        try:
            if not self.conn or not self.conn.is_connected():
                self.reconnect()
        except:
            self.reconnect()
    
    # ========================================================================
    # Sensor Data Methods
    # ========================================================================
    
    def store_sensor_reading(self, data: Dict):
        """Store a sensor reading"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        try:
            cursor.execute('''
                INSERT INTO sensor_readings 
                (timestamp, temperature, ambient_temp, ph, tds, heater_state, co2_state)
                VALUES (%s, %s, %s, %s, %s, %s, %s)
            ''', (
                data.get('timestamp', int(time.time())),
                data.get('temperature'),
                data.get('ambientTemp'),
                data.get('ph'),
                data.get('tds'),
                1 if data.get('heaterState') == 'ON' else 0,
                1 if data.get('co2State') == 'ON' else 0
            ))
            self.conn.commit()
        except mysql.connector.Error as e:
            logger.error(f"Error storing sensor reading: {e}")
    
    def get_recent_sensor_readings(self, hours: int = 24) -> List[Dict]:
        """Get sensor readings from the last N hours"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        end_ts = int(time.time())
        start_ts = end_ts - (hours * 3600)
        
        cursor.execute('''
            SELECT * FROM sensor_readings 
            WHERE timestamp BETWEEN %s AND %s
            ORDER BY timestamp ASC
        ''', (start_ts, end_ts))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    # ========================================================================
    # PID Performance Methods
    # ========================================================================
    
    def store_pid_performance(self, data: Dict):
        """Store PID performance data"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        try:
            cursor.execute('''
                INSERT INTO pid_performance 
                (timestamp, controller, kp, ki, kd, settling_time, overshoot, 
                 steady_state_error, temperature, ambient_temp, tds, ph, hour, season, tank_volume)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ''', (
                data.get('timestamp', int(time.time())),
                data['controller'],
                data['kp'], data['ki'], data['kd'],
                data.get('settling_time'),
                data.get('overshoot'),
                data.get('steady_state_error'),
                data.get('temperature'),
                data.get('ambient_temp'),
                data.get('tds'),
                data.get('ph'),
                data.get('hour'),
                data.get('season'),
                data.get('tank_volume')
            ))
            self.conn.commit()
            logger.debug(f"Stored PID performance for {data['controller']}")
        except mysql.connector.Error as e:
            logger.error(f"Error storing PID performance: {e}")
    
    def get_pid_performance_history(self, controller: str, season: Optional[int] = None, 
                                    limit: int = 1000) -> List[Dict]:
        """Get PID performance history for a controller and optional season"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        
        if season is not None:
            cursor.execute('''
                SELECT * FROM pid_performance 
                WHERE controller = %s AND season = %s
                ORDER BY timestamp DESC 
                LIMIT %s
            ''', (controller, season, limit))
        else:
            cursor.execute('''
                SELECT * FROM pid_performance 
                WHERE controller = %s
                ORDER BY timestamp DESC 
                LIMIT %s
            ''', (controller, limit))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    def store_pid_gains(self, controller: str, kp: float, ki: float, kd: float,
                       confidence: float, model_type: str, season: int):
        """Store optimized PID gains"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        try:
            cursor.execute('''
                INSERT INTO pid_gains 
                (timestamp, controller, kp, ki, kd, confidence, model_type, season)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            ''', (int(time.time()), controller, kp, ki, kd, confidence, model_type, season))
            self.conn.commit()
            logger.info(f"Stored {controller} gains for season {season}: Kp={kp:.3f}, Ki={ki:.3f}, Kd={kd:.3f}")
        except mysql.connector.Error as e:
            logger.error(f"Error storing PID gains: {e}")
    
    # ========================================================================
    # Water Change Methods
    # ========================================================================
    
    def store_water_change(self, wc_data: Dict):
        """Store a water change event"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        try:
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
            logger.info(f"Water change stored: {wc_data.get('volume')}L")
        except mysql.connector.Error as e:
            logger.error(f"Error storing water change: {e}")
    
    def get_water_change_history(self, limit: int = 50) -> List[Dict]:
        """Get recent water change history"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT * FROM water_changes 
            WHERE completed = 1
            ORDER BY end_timestamp DESC 
            LIMIT %s
        ''', (limit,))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    def store_filter_maintenance(self, fm_data: Dict):
        """Store filter maintenance event"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        try:
            cursor.execute('''
                INSERT INTO filter_maintenance 
                (timestamp, filter_type, days_since_last, tds_before, tds_after, notes)
                VALUES (%s, %s, %s, %s, %s, %s)
            ''', (
                fm_data.get('timestamp', int(time.time())),
                fm_data.get('filter_type', 'mechanical'),
                fm_data.get('days_since_last', 0),
                fm_data.get('tds_before'),
                fm_data.get('tds_after'),
                fm_data.get('notes', '')
            ))
            self.conn.commit()
            logger.info("Filter maintenance recorded")
        except mysql.connector.Error as e:
            logger.error(f"Error storing filter maintenance: {e}")
    
    def get_last_filter_maintenance(self) -> Optional[Dict]:
        """Get the most recent filter maintenance"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT * FROM filter_maintenance 
            ORDER BY timestamp DESC 
            LIMIT 1
        ''')
        
        result = cursor.fetchone()
        if result:
            columns = [desc[0] for desc in cursor.description]
            return dict(zip(columns, result))
        return None
    
    def store_wc_prediction(self, days: float, confidence: float, model_type: str):
        """Store water change prediction for accuracy tracking"""
        self.ensure_connection()
        cursor = self.conn.cursor()
        try:
            cursor.execute('''
                INSERT INTO wc_predictions 
                (prediction_timestamp, predicted_days, confidence, model_type)
                VALUES (%s, %s, %s, %s)
            ''', (int(time.time()), days, confidence, model_type))
            self.conn.commit()
        except mysql.connector.Error as e:
            logger.error(f"Error storing WC prediction: {e}")
    
    def close(self):
        """Close database connection"""
        if self.conn and self.conn.is_connected():
            self.conn.close()
            logger.info("Database connection closed")


# ============================================================================
# PID Optimizer
# ============================================================================

class PIDOptimizer:
    """ML-based PID gain optimizer with seasonal models"""
    
    def __init__(self, db: AquariumDatabase, controller: str):
        self.db = db
        self.controller = controller  # 'temp' or 'co2'
        self.models = {}  # season -> {kp_model, ki_model, kd_model}
        self.scalers = {}  # season -> scaler
        self.last_train_time = {}  # season -> timestamp
        
        logger.info(f"PID Optimizer initialized for {controller} controller")
    
    def extract_features(self, history: List[Dict]) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """Extract features and targets from performance history
        
        Features:
        - Temperature (or pH for CO2 controller)
        - Ambient temperature
        - TDS
        - pH (or temperature for CO2 controller)
        - Hour of day (0-23)
        - Day of week (0-6)
        - Tank volume
        
        Targets: Kp, Ki, Kd
        Weights: Based on performance (lower settling time & overshoot = higher weight)
        """
        X = []
        y_kp = []
        y_ki = []
        y_kd = []
        weights = []
        
        for record in history:
            # Skip records with missing data
            if not all(k in record for k in ['kp', 'ki', 'kd', 'settling_time', 'overshoot']):
                continue
            
            # Extract features
            features = [
                record.get('temperature', 25.0) if self.controller == 'temp' else record.get('ph', 7.0),
                record.get('ambient_temp', 22.0),
                record.get('tds', 300.0),
                record.get('ph', 7.0) if self.controller == 'temp' else record.get('temperature', 25.0),
                record.get('hour', 12),
                datetime.fromtimestamp(record['timestamp']).weekday(),
                record.get('tank_volume', 200.0)
            ]
            
            X.append(features)
            y_kp.append(record['kp'])
            y_ki.append(record['ki'])
            y_kd.append(record['kd'])
            
            # Calculate weight based on performance
            # Lower settling time and overshoot = better performance = higher weight
            settling_time = max(record['settling_time'], 1.0)  # Avoid division by zero
            overshoot = max(record['overshoot'], 0.01)
            weight = 1.0 / (settling_time * overshoot)
            weights.append(weight)
        
        X = np.array(X)
        y_kp = np.array(y_kp)
        y_ki = np.array(y_ki)
        y_kd = np.array(y_kd)
        weights = np.array(weights)
        
        # Normalize weights
        if len(weights) > 0:
            weights = weights / np.mean(weights)
        
        return X, y_kp, y_ki, y_kd, weights
    
    def train_season(self, season: int) -> Dict:
        """Train models for a specific season (0=spring, 1=summer, 2=autumn, 3=winter)"""
        logger.info(f"Training {self.controller} controller for season {season}...")
        
        # Get performance history for this season
        history = self.db.get_pid_performance_history(self.controller, season=season, limit=1000)
        
        if len(history) < MIN_PID_SAMPLES:
            logger.warning(f"Insufficient data for {self.controller} season {season}: {len(history)} samples (need {MIN_PID_SAMPLES})")
            return {'error': 'insufficient_data', 'samples': len(history)}
        
        # Extract features and targets
        X, y_kp, y_ki, y_kd, weights = self.extract_features(history)
        
        if len(X) == 0:
            logger.error(f"No valid samples after feature extraction for {self.controller} season {season}")
            return {'error': 'no_valid_samples'}
        
        # Scale features
        scaler = StandardScaler()
        X_scaled = scaler.fit_transform(X)
        
        # Train three models (one for each PID parameter)
        kp_model = GradientBoostingRegressor(n_estimators=50, random_state=42)
        ki_model = GradientBoostingRegressor(n_estimators=50, random_state=42)
        kd_model = GradientBoostingRegressor(n_estimators=50, random_state=42)
        
        kp_model.fit(X_scaled, y_kp, sample_weight=weights)
        ki_model.fit(X_scaled, y_ki, sample_weight=weights)
        kd_model.fit(X_scaled, y_kd, sample_weight=weights)
        
        # Calculate R² scores
        kp_score = kp_model.score(X_scaled, y_kp, sample_weight=weights)
        ki_score = ki_model.score(X_scaled, y_ki, sample_weight=weights)
        kd_score = kd_model.score(X_scaled, y_kd, sample_weight=weights)
        avg_score = (kp_score + ki_score + kd_score) / 3
        
        # Store models
        self.models[season] = {
            'kp': kp_model,
            'ki': ki_model,
            'kd': kd_model
        }
        self.scalers[season] = scaler
        self.last_train_time[season] = time.time()
        
        logger.info(f"Season {season} training complete:")
        logger.info(f"  Samples: {len(X)}")
        logger.info(f"  Kp R²: {kp_score:.3f}")
        logger.info(f"  Ki R²: {ki_score:.3f}")
        logger.info(f"  Kd R²: {kd_score:.3f}")
        logger.info(f"  Average R²: {avg_score:.3f}")
        
        return {
            'season': season,
            'samples': len(X),
            'kp_score': kp_score,
            'ki_score': ki_score,
            'kd_score': kd_score,
            'avg_score': avg_score
        }
    
    def train_all_seasons(self) -> Dict:
        """Train models for all four seasons"""
        results = {}
        for season in range(4):
            season_name = ['spring', 'summer', 'autumn', 'winter'][season]
            result = self.train_season(season)
            results[season_name] = result
        return results
    
    def predict(self, sensor_data: Dict, season: int) -> Dict:
        """Predict optimal PID gains for current conditions"""
        if season not in self.models:
            logger.warning(f"No model available for {self.controller} season {season}")
            return {'error': 'no_model', 'season': season}
        
        # Extract features
        features = [
            sensor_data.get('temperature', 25.0) if self.controller == 'temp' else sensor_data.get('ph', 7.0),
            sensor_data.get('ambient_temp', 22.0),
            sensor_data.get('tds', 300.0),
            sensor_data.get('ph', 7.0) if self.controller == 'temp' else sensor_data.get('temperature', 25.0),
            datetime.now().hour,
            datetime.now().weekday(),
            sensor_data.get('tank_volume', 200.0)
        ]
        
        X = np.array([features])
        X_scaled = self.scalers[season].transform(X)
        
        # Predict
        kp = float(self.models[season]['kp'].predict(X_scaled)[0])
        ki = float(self.models[season]['ki'].predict(X_scaled)[0])
        kd = float(self.models[season]['kd'].predict(X_scaled)[0])
        
        # Calculate confidence (based on how similar current conditions are to training data)
        # For simplicity, use average R² score
        confidence = (self.models[season]['kp'].score(X_scaled, [kp]) + 
                     self.models[season]['ki'].score(X_scaled, [ki]) + 
                     self.models[season]['kd'].score(X_scaled, [kd])) / 3
        confidence = max(0.0, min(1.0, confidence))  # Clamp to [0, 1]
        
        return {
            'controller': self.controller,
            'season': season,
            'kp': kp,
            'ki': ki,
            'kd': kd,
            'confidence': confidence,
            'model': 'gradient_boosting',
            'timestamp': int(time.time())
        }


# ============================================================================
# Water Change Predictor
# ============================================================================

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
        self.last_train_time = 0
        
        logger.info("Water Change Predictor initialized")
    
    def extract_features(self, history: List[Dict]) -> Tuple[np.ndarray, np.ndarray]:
        """Extract features from water change history
        
        Features:
        - Days since previous water change
        - TDS before change
        - TDS increase rate
        - Volume percentage
        - Ambient temperature (affects evaporation)
        - Days since last filter maintenance
        
        Target: Days between water changes
        """
        X = []
        y = []
        
        # Get last filter maintenance
        last_fm = self.db.get_last_filter_maintenance()
        last_fm_ts = last_fm['timestamp'] if last_fm else 0
        
        for i in range(1, len(history)):
            current = history[i]
            previous = history[i - 1]
            
            # Skip if missing data
            if not all(k in current for k in ['end_timestamp', 'tds_before', 'volume_litres']):
                continue
            
            days_between = (current['end_timestamp'] - previous['end_timestamp']) / 86400.0
            tds_before = current.get('tds_before', 300.0)
            tds_after_prev = previous.get('tds_after', 200.0)
            tds_increase_rate = (tds_before - tds_after_prev) / max(days_between, 1.0)
            volume_percent = (current['volume_litres'] / 200.0) * 100.0  # Assuming 200L tank
            
            # Get ambient temp from sensor readings around that time
            ambient_temp = 22.0  # Default
            sensor_readings = self.db.get_recent_sensor_readings(hours=24)
            if sensor_readings:
                temps = [r.get('ambient_temp', 22.0) for r in sensor_readings if r.get('ambient_temp')]
                if temps:
                    ambient_temp = np.mean(temps)
            
            # Days since last filter maintenance
            days_since_fm = (current['end_timestamp'] - last_fm_ts) / 86400.0 if last_fm_ts > 0 else 30.0
            
            features = [
                days_between,
                tds_before,
                tds_increase_rate,
                volume_percent,
                ambient_temp,
                days_since_fm
            ]
            
            X.append(features)
            y.append(days_between)
        
        return np.array(X), np.array(y)
    
    def train(self) -> Dict:
        """Train water change prediction models"""
        logger.info("Training water change prediction models...")
        
        # Get water change history
        history = self.db.get_water_change_history(limit=100)
        
        if len(history) < MIN_WC_SAMPLES:
            logger.warning(f"Insufficient water change history: {len(history)} records (need {MIN_WC_SAMPLES})")
            return {'error': 'insufficient_data', 'samples': len(history)}
        
        # Extract features
        X, y = self.extract_features(history)
        
        if len(X) < MIN_WC_SAMPLES:
            logger.warning(f"Insufficient valid samples after feature extraction: {len(X)}")
            return {'error': 'insufficient_samples', 'samples': len(X)}
        
        # Scale features
        X_scaled = self.scaler.fit_transform(X)
        
        # Train all models and find best
        scores = {}
        for name, model in self.models.items():
            model.fit(X_scaled, y)
            score = model.score(X_scaled, y)
            scores[name] = score
            logger.debug(f"  {name}: R² = {score:.3f}")
        
        # Select best model
        self.best_model_name = max(scores, key=scores.get)
        self.best_model = self.models[self.best_model_name]
        best_score = scores[self.best_model_name]
        self.last_train_time = time.time()
        
        logger.info(f"Training complete:")
        logger.info(f"  Best model: {self.best_model_name}")
        logger.info(f"  R² score: {best_score:.3f}")
        logger.info(f"  Training samples: {len(X)}")
        
        return {
            'model': self.best_model_name,
            'score': best_score,
            'training_samples': len(X),
            'all_scores': scores
        }
    
    def predict(self) -> Dict:
        """Predict next water change"""
        if self.best_model is None:
            logger.warning("No trained model available for water change prediction")
            return {'error': 'no_model'}
        
        # Get recent data
        history = self.db.get_water_change_history(limit=10)
        sensor_readings = self.db.get_recent_sensor_readings(hours=24)
        
        if len(history) < 1:
            return {'error': 'no_history'}
        
        last_wc = history[0]
        days_since_last = (time.time() - last_wc['end_timestamp']) / 86400.0
        
        # Get current TDS
        current_tds = 300.0
        if sensor_readings:
            tds_values = [r.get('tds', 300.0) for r in sensor_readings if r.get('tds')]
            if tds_values:
                current_tds = np.mean(tds_values)
        
        # Calculate TDS increase rate
        tds_after_last = last_wc.get('tds_after', 200.0)
        tds_increase_rate = (current_tds - tds_after_last) / max(days_since_last, 1.0)
        
        # Get ambient temp
        ambient_temp = 22.0
        if sensor_readings:
            temps = [r.get('ambient_temp', 22.0) for r in sensor_readings if r.get('ambient_temp')]
            if temps:
                ambient_temp = np.mean(temps)
        
        # Days since last filter maintenance
        last_fm = self.db.get_last_filter_maintenance()
        days_since_fm = (time.time() - last_fm['timestamp']) / 86400.0 if last_fm else 30.0
        
        # Average volume from history
        volumes = [wc.get('volume_litres', 40.0) for wc in history if wc.get('volume_litres')]
        avg_volume_percent = (np.mean(volumes) / 200.0) * 100.0 if volumes else 20.0
        
        # Predict
        features = np.array([[
            days_since_last,
            current_tds,
            tds_increase_rate,
            avg_volume_percent,
            ambient_temp,
            days_since_fm
        ]])
        
        X_scaled = self.scaler.transform(features)
        predicted_total_days = float(self.best_model.predict(X_scaled)[0])
        predicted_days_remaining = max(0, predicted_total_days - days_since_last)
        
        # Calculate confidence
        confidence = min(0.95, self.best_model.score(X_scaled, [predicted_total_days]))
        confidence = max(CONFIDENCE_THRESHOLD, confidence)
        
        needs_change_soon = predicted_days_remaining < 3.0
        
        return {
            'predicted_days_remaining': round(predicted_days_remaining, 1),
            'predicted_total_cycle_days': round(predicted_total_days, 1),
            'days_since_last_change': round(days_since_last, 1),
            'current_tds': round(current_tds, 1),
            'tds_increase_rate': round(tds_increase_rate, 2),
            'confidence': round(confidence, 2),
            'model': self.best_model_name,
            'needs_change_soon': needs_change_soon,
            'timestamp': int(time.time())
        }


# ============================================================================
# MQTT Manager
# ============================================================================

class MQTTManager:
    """Unified MQTT client for all ML operations"""
    
    def __init__(self, db: AquariumDatabase, pid_optimizers: Dict[str, PIDOptimizer], 
                 wc_predictor: WaterChangePredictor):
        self.db = db
        self.pid_optimizers = pid_optimizers
        self.wc_predictor = wc_predictor
        self.client = mqtt.Client()
        
        if MQTT_USER and MQTT_PASSWORD:
            self.client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        self.connected = False
    
    def connect(self):
        """Connect to MQTT broker"""
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.client.loop_start()
            logger.info(f"MQTT connecting to {MQTT_BROKER}:{MQTT_PORT}")
        except Exception as e:
            logger.error(f"MQTT connection error: {e}")
            raise
    
    def disconnect(self):
        """Disconnect from MQTT broker"""
        self.client.loop_stop()
        self.client.disconnect()
        logger.info("MQTT disconnected")
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            self.connected = True
            logger.info("MQTT connected successfully")
            
            # Subscribe to topics
            client.subscribe(TOPIC_SENSOR_DATA)
            client.subscribe(TOPIC_PID_PERFORMANCE)
            client.subscribe(TOPIC_WC_EVENT)
            client.subscribe(TOPIC_FILTER_MAINTENANCE)
            
            logger.info(f"Subscribed to: {TOPIC_SENSOR_DATA}, {TOPIC_PID_PERFORMANCE}, {TOPIC_WC_EVENT}, {TOPIC_FILTER_MAINTENANCE}")
        else:
            logger.error(f"MQTT connection failed with code {rc}")
    
    def on_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT broker"""
        self.connected = False
        logger.warning(f"MQTT disconnected with code {rc}")
    
    def on_message(self, client, userdata, msg):
        """Callback when message received"""
        try:
            payload = json.loads(msg.payload.decode())
            topic = msg.topic
            
            # Handle sensor data
            if topic == TOPIC_SENSOR_DATA:
                self.db.store_sensor_reading(payload)
            
            # Handle PID performance data
            elif topic == TOPIC_PID_PERFORMANCE:
                self.db.store_pid_performance(payload)
                logger.debug(f"Stored PID performance for {payload.get('controller', 'unknown')}")
            
            # Handle water change events
            elif topic == TOPIC_WC_EVENT:
                self.db.store_water_change(payload)
                logger.info("Water change event recorded")
            
            # Handle filter maintenance
            elif topic == TOPIC_FILTER_MAINTENANCE:
                self.db.store_filter_maintenance(payload)
                logger.info("Filter maintenance recorded")
            
        except json.JSONDecodeError as e:
            logger.error(f"JSON decode error: {e}")
        except Exception as e:
            logger.error(f"Error processing message: {e}")
    
    def publish_pid_gains(self, gains: Dict):
        """Publish optimized PID gains"""
        try:
            payload = json.dumps(gains)
            result = self.client.publish(TOPIC_PID_GAINS, payload, retain=True)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                logger.info(f"Published {gains['controller']} PID gains: Kp={gains['kp']:.3f}, Ki={gains['ki']:.3f}, Kd={gains['kd']:.3f}")
                return True
            else:
                logger.error(f"Failed to publish PID gains, return code: {result.rc}")
                return False
        except Exception as e:
            logger.error(f"Error publishing PID gains: {e}")
            return False
    
    def publish_wc_prediction(self, prediction: Dict):
        """Publish water change prediction"""
        try:
            payload = json.dumps(prediction)
            result = self.client.publish(TOPIC_WC_PREDICTION, payload, retain=True)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                logger.info(f"Published WC prediction: {prediction['predicted_days_remaining']} days remaining")
                return True
            else:
                logger.error(f"Failed to publish WC prediction, return code: {result.rc}")
                return False
        except Exception as e:
            logger.error(f"Error publishing WC prediction: {e}")
            return False


# ============================================================================
# Unified ML Service
# ============================================================================

class AquariumMLService:
    """Unified ML service for aquarium automation"""
    
    def __init__(self):
        self.db = AquariumDatabase(DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD)
        self.pid_temp = PIDOptimizer(self.db, 'temp')
        self.pid_co2 = PIDOptimizer(self.db, 'co2')
        self.wc_predictor = WaterChangePredictor(self.db)
        self.mqtt = MQTTManager(self.db, {'temp': self.pid_temp, 'co2': self.pid_co2}, self.wc_predictor)
        
        self.last_pid_train = {}
        self.last_wc_train = 0
        
        logger.info("=" * 60)
        logger.info("Aquarium ML Service Initialized")
        logger.info(f"Database: {DB_HOST}:{DB_PORT}/{DB_NAME}")
        logger.info(f"MQTT: {MQTT_BROKER}:{MQTT_PORT}")
        logger.info("=" * 60)
    
    def train_all_pid_models(self):
        """Train all PID models (both controllers, all seasons)"""
        logger.info("Training all PID models...")
        
        # Temperature controller
        logger.info("\n--- Temperature PID ---")
        temp_results = self.pid_temp.train_all_seasons()
        
        # CO2 controller
        logger.info("\n--- CO2 PID ---")
        co2_results = self.pid_co2.train_all_seasons()
        
        # Publish predictions for current season
        current_season = (datetime.now().month - 1) // 3  # 0-3
        self.publish_pid_predictions(current_season)
        
        self.last_pid_train[current_season] = time.time()
        
        return {'temp': temp_results, 'co2': co2_results}
    
    def train_wc_model(self):
        """Train water change prediction model"""
        logger.info("\n--- Water Change Predictor ---")
        result = self.wc_predictor.train()
        
        if 'error' not in result:
            # Generate and publish prediction
            prediction = self.wc_predictor.predict()
            if 'error' not in prediction:
                self.mqtt.publish_wc_prediction(prediction)
                self.db.store_wc_prediction(
                    prediction['predicted_days_remaining'],
                    prediction['confidence'],
                    prediction['model']
                )
        
        self.last_wc_train = time.time()
        
        return result
    
    def publish_pid_predictions(self, season: int):
        """Generate and publish PID predictions for current conditions"""
        # Get recent sensor data
        sensor_readings = self.db.get_recent_sensor_readings(hours=1)
        if not sensor_readings:
            logger.warning("No recent sensor data for PID prediction")
            return
        
        # Average recent readings
        sensor_data = {
            'temperature': np.mean([r.get('temperature', 25.0) for r in sensor_readings if r.get('temperature')]),
            'ambient_temp': np.mean([r.get('ambient_temp', 22.0) for r in sensor_readings if r.get('ambient_temp')]),
            'tds': np.mean([r.get('tds', 300.0) for r in sensor_readings if r.get('tds')]),
            'ph': np.mean([r.get('ph', 7.0) for r in sensor_readings if r.get('ph')]),
            'tank_volume': 200.0  # TODO: Get from config
        }
        
        # Temperature PID
        temp_gains = self.pid_temp.predict(sensor_data, season)
        if 'error' not in temp_gains and temp_gains['confidence'] >= CONFIDENCE_THRESHOLD:
            self.mqtt.publish_pid_gains(temp_gains)
            self.db.store_pid_gains(
                'temp', temp_gains['kp'], temp_gains['ki'], temp_gains['kd'],
                temp_gains['confidence'], temp_gains['model'], season
            )
        
        # CO2 PID
        co2_gains = self.pid_co2.predict(sensor_data, season)
        if 'error' not in co2_gains and co2_gains['confidence'] >= CONFIDENCE_THRESHOLD:
            self.mqtt.publish_pid_gains(co2_gains)
            self.db.store_pid_gains(
                'co2', co2_gains['kp'], co2_gains['ki'], co2_gains['kd'],
                co2_gains['confidence'], co2_gains['model'], season
            )
    
    def run_once(self, pid_only=False, wc_only=False):
        """Run training once and exit"""
        if not wc_only:
            self.train_all_pid_models()
        
        if not pid_only:
            self.train_wc_model()
        
        logger.info("\nTraining complete!")
    
    def run_service(self):
        """Run as persistent service"""
        global shutdown_requested
        
        # Connect to MQTT
        self.mqtt.connect()
        
        # Wait for MQTT connection
        timeout = 10
        while not self.mqtt.connected and timeout > 0:
            time.sleep(1)
            timeout -= 1
        
        if not self.mqtt.connected:
            logger.error("Failed to connect to MQTT broker")
            return
        
        logger.info("Service running. Press Ctrl+C to stop.")
        
        # Initial training
        try:
            self.train_all_pid_models()
            self.train_wc_model()
        except Exception as e:
            logger.error(f"Initial training error: {e}")
        
        # Main service loop
        try:
            while not shutdown_requested:
                time.sleep(60)  # Check every minute
                
                current_time = time.time()
                current_season = (datetime.now().month - 1) // 3
                
                # Check if PID training is needed
                if current_season not in self.last_pid_train or \
                   (current_time - self.last_pid_train[current_season]) > PID_TRAIN_INTERVAL:
                    try:
                        logger.info("\nScheduled PID training...")
                        self.train_all_pid_models()
                    except Exception as e:
                        logger.error(f"PID training error: {e}")
                
                # Check if water change training is needed
                if (current_time - self.last_wc_train) > WC_TRAIN_INTERVAL:
                    try:
                        logger.info("\nScheduled water change training...")
                        self.train_wc_model()
                    except Exception as e:
                        logger.error(f"Water change training error: {e}")
        
        except KeyboardInterrupt:
            logger.info("\nShutdown requested by user")
        finally:
            self.mqtt.disconnect()
            self.db.close()
            logger.info("Service stopped")


# ============================================================================
# Signal Handlers
# ============================================================================

def signal_handler(signum, frame):
    """Handle shutdown signals"""
    global shutdown_requested
    logger.info(f"\nReceived signal {signum}")
    shutdown_requested = True


# ============================================================================
# Main Entry Point
# ============================================================================

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Unified Aquarium ML Service')
    parser.add_argument('--once', action='store_true', help='Train once and exit')
    parser.add_argument('--pid-only', action='store_true', help='Train PID models only')
    parser.add_argument('--waterchange-only', action='store_true', help='Train water change model only')
    parser.add_argument('--service', action='store_true', help='Run as persistent service (default)')
    
    args = parser.parse_args()
    
    # Register signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create service
    service = AquariumMLService()
    
    # Run
    if args.once:
        service.run_once(pid_only=args.pid_only, wc_only=args.waterchange_only)
    else:
        service.run_service()


if __name__ == "__main__":
    main()
