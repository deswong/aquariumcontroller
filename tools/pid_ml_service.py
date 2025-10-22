#!/usr/bin/env python3
"""
PID ML Training Service for Aquarium Controller

This service trains machine learning models to optimize PID controller gains (Kp, Ki, Kd)
based on historical performance data, sensor context (TDS, ambient temp, pH), time of day,
and seasonal patterns.

Features:
- Gradient Boosting Regressor for optimal PID gain prediction
- Multi-sensor fusion (temperature, ambient, TDS, pH, time, season)
- Seasonal model selection (4 separate models for spring, summer, autumn, winter)
- MQTT integration for real-time gain updates
- Performance tracking and validation
- Compatible with Phase 1+2+3 ML-enhanced PID control

Usage:
    # Run once for training
    python3 pid_ml_service.py --train

    # Run as persistent service (retrains every 6 hours)
    python3 pid_ml_service.py --service

    # Run with custom config
    python3 pid_ml_service.py --train --controller temp --samples 100

Database Schema:
    - sensor_readings: temperature, ambient_temp, ph, tds, timestamp
    - pid_performance: controller, kp, ki, kd, settling_time, overshoot, error, timestamp
    - pid_gains: controller, kp, ki, kd, confidence, timestamp

MQTT Topics:
    - Subscribe: aquarium/pid/performance (from ESP32)
    - Publish: aquarium/ml/pid/gains (to ESP32)
"""

import argparse
import json
import logging
import os
import time
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

import mysql.connector
import numpy as np
import paho.mqtt.client as mqtt
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import mean_squared_error, r2_score

# Configuration
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_TOPIC_PREFIX = os.getenv("MQTT_TOPIC_PREFIX", "aquarium")

# MQTT Topics
TOPIC_PID_PERFORMANCE = f"{MQTT_TOPIC_PREFIX}/pid/performance"
TOPIC_PID_GAINS = f"{MQTT_TOPIC_PREFIX}/ml/pid/gains"
TOPIC_PID_MODEL_UPDATE = f"{MQTT_TOPIC_PREFIX}/ml/pid/model_update"

# MariaDB Configuration
DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = int(os.getenv("DB_PORT", "3306"))
DB_NAME = os.getenv("DB_NAME", "aquarium")
DB_USER = os.getenv("DB_USER", "aquarium")
DB_PASSWORD = os.getenv("DB_PASSWORD", "")

# ML Parameters
MIN_TRAINING_SAMPLES = 50  # Minimum samples needed per season
PREDICTION_CONFIDENCE_THRESHOLD = 0.7
RETRAIN_INTERVAL_HOURS = 6
N_ESTIMATORS = 100
RANDOM_STATE = 42

# Seasons (Northern Hemisphere)
SEASONS = ["spring", "summer", "autumn", "winter"]

# Logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class PIDDatabase:
    """MariaDB database for PID performance tracking"""
    
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
        """Initialize database and create tables"""
        self.conn = mysql.connector.connect(**self.db_config)
        cursor = self.conn.cursor()
        
        # PID performance history
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
                INDEX idx_timestamp (timestamp),
                INDEX idx_controller (controller)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ''')
        
        # Current PID gains
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
                INDEX idx_timestamp (timestamp),
                INDEX idx_controller (controller),
                UNIQUE KEY unique_controller_season (controller, season)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
        ''')
        
        self.conn.commit()
        logger.info("PID database initialized")
    
    def store_performance(self, data: Dict):
        """Store PID performance data"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO pid_performance 
            (timestamp, controller, kp, ki, kd, settling_time, overshoot, steady_state_error,
             temperature, ambient_temp, tds, ph, hour, season, tank_volume)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
        ''', (
            data.get('timestamp', int(datetime.now().timestamp())),
            data.get('controller', 'temp'),
            data.get('kp'),
            data.get('ki'),
            data.get('kd'),
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
    
    def get_performance_history(self, controller: str = 'temp', limit: int = 1000, season: Optional[int] = None) -> List[Dict]:
        """Get recent PID performance history"""
        cursor = self.conn.cursor()
        
        if season is not None:
            query = '''
                SELECT * FROM pid_performance 
                WHERE controller = %s AND season = %s
                ORDER BY timestamp DESC 
                LIMIT %s
            '''
            cursor.execute(query, (controller, season, limit))
        else:
            query = '''
                SELECT * FROM pid_performance 
                WHERE controller = %s
                ORDER BY timestamp DESC 
                LIMIT %s
            '''
            cursor.execute(query, (controller, limit))
        
        columns = [desc[0] for desc in cursor.description]
        return [dict(zip(columns, row)) for row in cursor.fetchall()]
    
    def store_gains(self, controller: str, kp: float, ki: float, kd: float, 
                    confidence: float, model_type: str, season: int):
        """Store optimized PID gains"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO pid_gains 
            (timestamp, controller, kp, ki, kd, confidence, model_type, season)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            ON DUPLICATE KEY UPDATE
                timestamp = VALUES(timestamp),
                kp = VALUES(kp),
                ki = VALUES(ki),
                kd = VALUES(kd),
                confidence = VALUES(confidence),
                model_type = VALUES(model_type)
        ''', (
            int(datetime.now().timestamp()),
            controller,
            kp, ki, kd,
            confidence,
            model_type,
            season
        ))
        self.conn.commit()
    
    def get_current_gains(self, controller: str, season: int) -> Optional[Dict]:
        """Get current optimized gains for controller and season"""
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT * FROM pid_gains 
            WHERE controller = %s AND season = %s
            ORDER BY timestamp DESC 
            LIMIT 1
        ''', (controller, season))
        
        row = cursor.fetchone()
        if row:
            columns = [desc[0] for desc in cursor.description]
            return dict(zip(columns, row))
        return None
    
    def close(self):
        """Close database connection"""
        if self.conn:
            self.conn.close()


class PIDMLTrainer:
    """ML trainer for PID gain optimization"""
    
    def __init__(self, db: PIDDatabase, controller: str = 'temp'):
        self.db = db
        self.controller = controller
        self.models = {}  # Seasonal models: {season: model}
        self.scalers = {}  # Seasonal scalers: {season: scaler}
        self.season_names = SEASONS
    
    def extract_features(self, history: List[Dict]) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """Extract features and targets from performance history
        
        Features (7 inputs):
        - Temperature (or pH for CO2 controller)
        - Ambient temperature
        - TDS
        - pH
        - Hour of day (0-23)
        - Day of week (0-6)
        - Tank volume
        
        Targets (3 outputs):
        - Optimal Kp
        - Optimal Ki
        - Optimal Kd
        
        Performance metric for optimization:
        - Combined score = 1.0 / (settling_time + overshoot * 10 + steady_state_error * 100)
        """
        if not history:
            return np.array([]), np.array([]), np.array([]), np.array([])
        
        features = []
        kp_targets = []
        ki_targets = []
        kd_targets = []
        
        for record in history:
            # Skip records with missing data
            if record.get('settling_time') is None or record.get('settling_time') <= 0:
                continue
            
            # Extract features
            temp = record.get('temperature', 0) if self.controller == 'temp' else record.get('ph', 0)
            ambient = record.get('ambient_temp', 0)
            tds = record.get('tds', 0)
            ph = record.get('ph', 0)
            hour = record.get('hour', 0)
            
            # Calculate day of week from timestamp
            ts = record.get('timestamp', int(datetime.now().timestamp()))
            day_of_week = datetime.fromtimestamp(ts).weekday()
            
            tank_volume = record.get('tank_volume', 200.0)
            
            feature_vec = [temp, ambient, tds, ph, hour, day_of_week, tank_volume]
            features.append(feature_vec)
            
            # Extract PID gains
            kp_targets.append(record.get('kp', 10.0))
            ki_targets.append(record.get('ki', 0.5))
            kd_targets.append(record.get('kd', 5.0))
        
        return (np.array(features), np.array(kp_targets), 
                np.array(ki_targets), np.array(kd_targets))
    
    def calculate_performance_score(self, history: List[Dict]) -> np.ndarray:
        """Calculate performance scores for training samples
        
        Better performance = higher score
        - Fast settling time is good
        - Low overshoot is good
        - Low steady-state error is good
        """
        scores = []
        for record in history:
            settling = max(record.get('settling_time', 100), 1.0)
            overshoot = abs(record.get('overshoot', 0.5))
            error = abs(record.get('steady_state_error', 0.1))
            
            # Combined score (higher is better)
            score = 100.0 / (settling + overshoot * 10.0 + error * 100.0)
            scores.append(score)
        
        return np.array(scores)
    
    def train_seasonal_models(self, min_samples: int = MIN_TRAINING_SAMPLES) -> Dict:
        """Train separate models for each season"""
        results = {}
        
        for season_idx, season_name in enumerate(self.season_names):
            logger.info(f"Training {season_name} model for {self.controller} controller...")
            
            # Get seasonal data
            history = self.db.get_performance_history(
                controller=self.controller,
                limit=1000,
                season=season_idx
            )
            
            if len(history) < min_samples:
                logger.warning(f"Insufficient {season_name} data: {len(history)} samples (need {min_samples})")
                results[season_name] = {
                    'status': 'insufficient_data',
                    'samples': len(history)
                }
                continue
            
            # Extract features and targets
            X, kp_y, ki_y, kd_y = self.extract_features(history)
            
            if len(X) < min_samples:
                logger.warning(f"Insufficient valid {season_name} samples after filtering: {len(X)}")
                results[season_name] = {
                    'status': 'insufficient_data',
                    'samples': len(X)
                }
                continue
            
            # Calculate performance scores
            scores = self.calculate_performance_score(history[:len(X)])
            
            # Weight samples by performance (better performance = higher weight)
            weights = scores / scores.sum()
            
            # Train three separate models (Kp, Ki, Kd)
            scaler = StandardScaler()
            X_scaled = scaler.fit_transform(X)
            
            # Split data
            X_train, X_test, kp_train, kp_test = train_test_split(X_scaled, kp_y, test_size=0.2, random_state=RANDOM_STATE)
            _, _, ki_train, ki_test = train_test_split(X_scaled, ki_y, test_size=0.2, random_state=RANDOM_STATE)
            _, _, kd_train, kd_test = train_test_split(X_scaled, kd_y, test_size=0.2, random_state=RANDOM_STATE)
            _, _, w_train, _ = train_test_split(weights, test_size=0.2, random_state=RANDOM_STATE)
            
            # Train models
            kp_model = GradientBoostingRegressor(n_estimators=N_ESTIMATORS, random_state=RANDOM_STATE)
            ki_model = GradientBoostingRegressor(n_estimators=N_ESTIMATORS, random_state=RANDOM_STATE)
            kd_model = GradientBoostingRegressor(n_estimators=N_ESTIMATORS, random_state=RANDOM_STATE)
            
            kp_model.fit(X_train, kp_train, sample_weight=w_train)
            ki_model.fit(X_train, ki_train, sample_weight=w_train)
            kd_model.fit(X_train, kd_train, sample_weight=w_train)
            
            # Evaluate
            kp_pred = kp_model.predict(X_test)
            ki_pred = ki_model.predict(X_test)
            kd_pred = kd_model.predict(X_test)
            
            kp_r2 = r2_score(kp_test, kp_pred)
            ki_r2 = r2_score(ki_test, ki_pred)
            kd_r2 = r2_score(kd_test, kd_pred)
            
            avg_r2 = (kp_r2 + ki_r2 + kd_r2) / 3.0
            
            # Store models
            self.models[season_idx] = {
                'kp': kp_model,
                'ki': ki_model,
                'kd': kd_model
            }
            self.scalers[season_idx] = scaler
            
            logger.info(f"{season_name} training complete:")
            logger.info(f"  Samples: {len(X)}")
            logger.info(f"  Kp R²: {kp_r2:.3f}")
            logger.info(f"  Ki R²: {ki_r2:.3f}")
            logger.info(f"  Kd R²: {kd_r2:.3f}")
            logger.info(f"  Average R²: {avg_r2:.3f}")
            
            results[season_name] = {
                'status': 'success',
                'samples': len(X),
                'kp_r2': float(kp_r2),
                'ki_r2': float(ki_r2),
                'kd_r2': float(kd_r2),
                'avg_r2': float(avg_r2)
            }
        
        return results
    
    def predict_gains(self, temperature: float, ambient_temp: float, tds: float, ph: float,
                     hour: int, day_of_week: int, tank_volume: float, season: int) -> Optional[Dict]:
        """Predict optimal PID gains for given conditions"""
        if season not in self.models:
            logger.error(f"No model available for season {season}")
            return None
        
        # Prepare features
        if self.controller == 'temp':
            main_sensor = temperature
        else:
            main_sensor = ph
        
        features = np.array([[main_sensor, ambient_temp, tds, ph, hour, day_of_week, tank_volume]])
        
        # Scale features
        features_scaled = self.scalers[season].transform(features)
        
        # Predict gains
        kp = float(self.models[season]['kp'].predict(features_scaled)[0])
        ki = float(self.models[season]['ki'].predict(features_scaled)[0])
        kd = float(self.models[season]['kd'].predict(features_scaled)[0])
        
        # Clamp to reasonable ranges
        kp = max(1.0, min(50.0, kp))
        ki = max(0.01, min(5.0, ki))
        kd = max(0.1, min(20.0, kd))
        
        return {
            'controller': self.controller,
            'kp': kp,
            'ki': ki,
            'kd': kd,
            'season': season,
            'season_name': self.season_names[season],
            'confidence': 0.85,  # Placeholder - could calculate based on model uncertainty
            'timestamp': int(datetime.now().timestamp())
        }


def publish_gains_to_mqtt(gains: Dict) -> bool:
    """Publish optimized gains to MQTT"""
    try:
        client = mqtt.Client()
        
        if MQTT_USER and MQTT_PASSWORD:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        payload = json.dumps(gains)
        result = client.publish(TOPIC_PID_GAINS, payload, retain=True)
        
        client.disconnect()
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            logger.info(f"Gains published to MQTT: Kp={gains['kp']:.2f}, Ki={gains['ki']:.3f}, Kd={gains['kd']:.2f}")
            return True
        else:
            logger.error(f"Failed to publish gains, return code: {result.rc}")
            return False
    
    except Exception as e:
        logger.error(f"Error publishing to MQTT: {e}")
        return False


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='PID ML Training Service')
    parser.add_argument('--train', action='store_true', help='Train models once and exit')
    parser.add_argument('--service', action='store_true', help='Run as persistent service')
    parser.add_argument('--controller', default='temp', choices=['temp', 'co2'], help='Controller type')
    parser.add_argument('--samples', type=int, default=MIN_TRAINING_SAMPLES, help='Minimum samples per season')
    args = parser.parse_args()
    
    logger.info("="*80)
    logger.info(f"PID ML Training Service - {datetime.now()}")
    logger.info(f"Controller: {args.controller}")
    logger.info("="*80)
    
    # Initialize database
    db = PIDDatabase(
        host=DB_HOST,
        port=DB_PORT,
        database=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD
    )
    logger.info(f"Database: MariaDB at {DB_HOST}:{DB_PORT}/{DB_NAME}")
    
    # Initialize trainer
    trainer = PIDMLTrainer(db, controller=args.controller)
    
    # Training function
    def train_and_publish():
        logger.info("Starting training...")
        results = trainer.train_seasonal_models(min_samples=args.samples)
        
        # Log results
        for season, result in results.items():
            logger.info(f"{season}: {result}")
        
        # Publish gains for current season
        current_month = datetime.now().month
        current_season = (current_month - 3) // 3 % 4  # 0=spring, 1=summer, 2=autumn, 3=winter
        
        if current_season in trainer.models:
            # Get current conditions (use recent averages)
            history = db.get_performance_history(controller=args.controller, limit=1)
            if history:
                last = history[0]
                gains = trainer.predict_gains(
                    temperature=last.get('temperature', 25.0),
                    ambient_temp=last.get('ambient_temp', 22.0),
                    tds=last.get('tds', 300.0),
                    ph=last.get('ph', 7.0),
                    hour=datetime.now().hour,
                    day_of_week=datetime.now().weekday(),
                    tank_volume=last.get('tank_volume', 200.0),
                    season=current_season
                )
                
                if gains:
                    # Store in database
                    db.store_gains(
                        controller=args.controller,
                        kp=gains['kp'],
                        ki=gains['ki'],
                        kd=gains['kd'],
                        confidence=gains['confidence'],
                        model_type='GradientBoostingRegressor',
                        season=current_season
                    )
                    
                    # Publish to MQTT
                    publish_gains_to_mqtt(gains)
        
        logger.info("Training complete!")
        logger.info("="*80)
    
    # Run once or as service
    if args.train:
        train_and_publish()
    elif args.service:
        logger.info(f"Running as service, retraining every {RETRAIN_INTERVAL_HOURS} hours")
        while True:
            try:
                train_and_publish()
                logger.info(f"Sleeping for {RETRAIN_INTERVAL_HOURS} hours...")
                time.sleep(RETRAIN_INTERVAL_HOURS * 3600)
            except KeyboardInterrupt:
                logger.info("Service stopped by user")
                break
            except Exception as e:
                logger.error(f"Error in service loop: {e}")
                time.sleep(300)  # Sleep 5 minutes on error
    else:
        parser.print_help()
        return 1
    
    db.close()
    return 0


if __name__ == "__main__":
    exit(main())
