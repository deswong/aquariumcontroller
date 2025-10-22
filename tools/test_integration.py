#!/usr/bin/env python3
"""
Integration Tests for Unified Aquarium ML Service

These tests require actual services to be running:
1. MariaDB database (test database will be created/destroyed)
2. Mosquitto MQTT broker

Setup:
    # Install test database
    sudo mysql -e "CREATE DATABASE IF NOT EXISTS aquarium_test;"
    sudo mysql -e "CREATE USER IF NOT EXISTS 'aquarium_test'@'localhost' IDENTIFIED BY 'test_password';"
    sudo mysql -e "GRANT ALL PRIVILEGES ON aquarium_test.* TO 'aquarium_test'@'localhost';"
    
    # Ensure MQTT broker is running
    sudo systemctl start mosquitto

Usage:
    python3 test_integration.py
    pytest test_integration.py -v -s
"""

import json
import os
import sys
import time
import unittest
from datetime import datetime, timedelta

import numpy as np
import paho.mqtt.client as mqtt

# Add parent directory to path
sys.path.insert(0, '.')

from aquarium_ml_service import (
    AquariumDatabase,
    PIDOptimizer,
    WaterChangePredictor,
    MQTTManager,
    AquariumMLService
)


# Test configuration - override with environment variables
TEST_DB_HOST = os.environ.get('TEST_DB_HOST', 'localhost')
TEST_DB_PORT = int(os.environ.get('TEST_DB_PORT', 3306))
TEST_DB_NAME = os.environ.get('TEST_DB_NAME', 'aquarium_test')
TEST_DB_USER = os.environ.get('TEST_DB_USER', 'aquarium_test')
TEST_DB_PASSWORD = os.environ.get('TEST_DB_PASSWORD', 'test_password')

TEST_MQTT_BROKER = os.environ.get('TEST_MQTT_BROKER', 'localhost')
TEST_MQTT_PORT = int(os.environ.get('TEST_MQTT_PORT', 1883))
TEST_MQTT_USER = os.environ.get('TEST_MQTT_USER', '')
TEST_MQTT_PASSWORD = os.environ.get('TEST_MQTT_PASSWORD', '')


class TestDatabaseIntegration(unittest.TestCase):
    """Integration tests for database operations"""
    
    @classmethod
    def setUpClass(cls):
        """Set up test database"""
        try:
            cls.db = AquariumDatabase(
                TEST_DB_HOST,
                TEST_DB_PORT,
                TEST_DB_NAME,
                TEST_DB_USER,
                TEST_DB_PASSWORD
            )
            print(f"\n✓ Connected to test database: {TEST_DB_NAME}")
        except Exception as e:
            raise unittest.SkipTest(f"Cannot connect to test database: {e}")
    
    @classmethod
    def tearDownClass(cls):
        """Clean up test database"""
        if hasattr(cls, 'db') and cls.db.conn:
            # Drop all tables
            cursor = cls.db.conn.cursor()
            try:
                cursor.execute("DROP TABLE IF EXISTS sensor_readings")
                cursor.execute("DROP TABLE IF EXISTS pid_performance")
                cursor.execute("DROP TABLE IF EXISTS pid_gains")
                cursor.execute("DROP TABLE IF EXISTS water_changes")
                cursor.execute("DROP TABLE IF EXISTS filter_maintenance")
                cursor.execute("DROP TABLE IF EXISTS wc_predictions")
                cls.db.conn.commit()
                print("\n✓ Cleaned up test database")
            finally:
                cursor.close()
                cls.db.conn.close()
    
    def test_01_table_creation(self):
        """Test that all tables are created"""
        cursor = self.db.conn.cursor()
        cursor.execute("SHOW TABLES")
        tables = [row[0] for row in cursor.fetchall()]
        cursor.close()
        
        expected_tables = [
            'sensor_readings',
            'pid_performance',
            'pid_gains',
            'water_changes',
            'filter_maintenance',
            'wc_predictions'
        ]
        
        for table in expected_tables:
            self.assertIn(table, tables, f"Table {table} not created")
        
        print(f"✓ All {len(expected_tables)} tables created")
    
    def test_02_store_and_retrieve_sensor_data(self):
        """Test storing and retrieving sensor readings"""
        # Store sensor reading
        test_data = {
            'timestamp': int(time.time()),
            'temperature': 24.5,
            'ambientTemp': 22.0,
            'ph': 7.2,
            'tds': 300.0,
            'heaterState': 'ON',
            'co2State': 'OFF'
        }
        
        self.db.store_sensor_reading(test_data)
        
        # Retrieve it
        cursor = self.db.conn.cursor()
        cursor.execute("SELECT * FROM sensor_readings ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        cursor.close()
        
        self.assertIsNotNone(row)
        self.assertEqual(row[2], 24.5)  # temperature
        self.assertEqual(row[3], 22.0)  # ambient_temp
        self.assertEqual(row[4], 7.2)   # ph
        self.assertEqual(row[5], 300.0) # tds
        
        print(f"✓ Stored and retrieved sensor reading: temp={row[2]}°C")
    
    def test_03_store_pid_performance(self):
        """Test storing PID performance data"""
        test_data = {
            'timestamp': int(time.time()),
            'controller': 'temp',
            'kp': 10.0,
            'ki': 0.5,
            'kd': 5.0,
            'settling_time': 12.5,
            'overshoot': 0.3,
            'steady_state_error': 0.05,
            'temperature': 24.5,
            'ambient_temp': 22.0,
            'tds': 300.0,
            'ph': 7.2,
            'hour': 14,
            'season': 1,
            'tank_volume': 200.0
        }
        
        self.db.store_pid_performance(test_data)
        
        # Retrieve it
        cursor = self.db.conn.cursor()
        cursor.execute("SELECT * FROM pid_performance WHERE controller='temp' ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        cursor.close()
        
        self.assertIsNotNone(row)
        self.assertEqual(row[3], 10.0)  # kp
        self.assertEqual(row[4], 0.5)   # ki
        self.assertEqual(row[5], 5.0)   # kd
        
        print(f"✓ Stored PID performance: Kp={row[3]}, Ki={row[4]}, Kd={row[5]}")
    
    def test_04_store_water_change(self):
        """Test storing water change event"""
        now = int(time.time())
        test_data = {
            'startTime': now - 3600,
            'endTime': now,
            'volume': 40.0,
            'tempBefore': 24.5,
            'tempAfter': 23.8,
            'phBefore': 7.2,
            'phAfter': 7.0,
            'tdsBefore': 320.0,
            'tdsAfter': 210.0,
            'duration': 60,
            'successful': True
        }
        
        self.db.store_water_change(test_data)
        
        # Retrieve it
        cursor = self.db.conn.cursor()
        cursor.execute("SELECT * FROM water_changes ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        cursor.close()
        
        self.assertIsNotNone(row)
        self.assertEqual(row[3], 40.0)   # volume
        self.assertEqual(row[7], 320.0)  # tds_before
        self.assertEqual(row[8], 210.0)  # tds_after
        
        print(f"✓ Stored water change: {row[3]}L, TDS {row[7]}→{row[8]} ppm")


class TestPIDOptimizerIntegration(unittest.TestCase):
    """Integration tests for PID optimizer with real database"""
    
    @classmethod
    def setUpClass(cls):
        """Set up database and optimizer"""
        try:
            cls.db = AquariumDatabase(
                TEST_DB_HOST,
                TEST_DB_PORT,
                TEST_DB_NAME,
                TEST_DB_USER,
                TEST_DB_PASSWORD
            )
            cls.optimizer = PIDOptimizer(cls.db, 'temp')
            print(f"\n✓ Created PID optimizer for temperature controller")
        except Exception as e:
            raise unittest.SkipTest(f"Cannot connect to test database: {e}")
    
    def test_01_populate_training_data(self):
        """Populate database with synthetic training data"""
        base_ts = int(time.time())
        
        # Generate 100 performance records with realistic variations
        for i in range(100):
            # Vary conditions throughout the day and seasons
            hour = (i * 3) % 24
            season = i % 4  # 0=spring, 1=summer, 2=autumn, 3=winter
            
            # Temperature varies by season and time of day
            if season == 1:  # Summer
                temp = 25.0 + np.random.randn() * 0.5
                ambient = 23.0 + (hour - 12) * 0.2 + np.random.randn() * 0.3
            elif season == 3:  # Winter
                temp = 23.5 + np.random.randn() * 0.5
                ambient = 20.0 + (hour - 12) * 0.2 + np.random.randn() * 0.3
            else:  # Spring/Autumn
                temp = 24.5 + np.random.randn() * 0.5
                ambient = 22.0 + (hour - 12) * 0.2 + np.random.randn() * 0.3
            
            # TDS slowly increases between water changes
            days_since_wc = (i % 14) / 14.0  # Reset every 14 days
            tds = 250.0 + days_since_wc * 80.0 + np.random.randn() * 10
            
            # pH varies slightly
            ph = 7.2 + np.random.randn() * 0.1
            
            # Gains vary with performance
            base_kp = 10.0 if season in [0, 2] else 11.0  # Higher in summer/winter
            kp = base_kp + np.random.randn() * 1.0
            ki = 0.5 + np.random.randn() * 0.1
            kd = 5.0 + np.random.randn() * 0.5
            
            # Performance metrics - better gains = better performance
            settling_time = 12.0 + np.random.randn() * 2.0 + (abs(kp - base_kp) * 2)
            overshoot = 0.3 + np.random.randn() * 0.1 + (abs(kp - base_kp) * 0.05)
            steady_state_error = 0.05 + np.random.randn() * 0.01
            
            data = {
                'timestamp': base_ts - (i * 3600),
                'controller': 'temp',
                'kp': kp,
                'ki': ki,
                'kd': kd,
                'settling_time': settling_time,
                'overshoot': overshoot,
                'steady_state_error': steady_state_error,
                'temperature': temp,
                'ambient_temp': ambient,
                'tds': tds,
                'ph': ph,
                'hour': hour,
                'season': season,
                'tank_volume': 200.0
            }
            
            self.db.store_pid_performance(data)
        
        print(f"✓ Populated 100 training samples")
    
    def test_02_train_seasonal_models(self):
        """Test training models for all seasons"""
        results = {}
        
        for season in range(4):
            season_names = ['Spring', 'Summer', 'Autumn', 'Winter']
            result = self.optimizer.train_season(season)
            
            self.assertNotIn('error', result, f"Training failed for {season_names[season]}")
            self.assertGreater(result['samples'], 0, f"No samples for {season_names[season]}")
            
            results[season_names[season]] = result
            print(f"✓ {season_names[season]}: {result['samples']} samples, "
                  f"avg score: {result['avg_score']:.3f}")
        
        # Verify all seasons trained
        self.assertEqual(len(self.optimizer.seasonal_models), 4)
    
    def test_03_predict_gains(self):
        """Test predicting optimal gains"""
        test_conditions = [
            {'name': 'Summer day', 'temperature': 25.0, 'ambient_temp': 24.0, 'tds': 280.0, 'ph': 7.2, 'tank_volume': 200.0, 'season': 1},
            {'name': 'Winter night', 'temperature': 23.5, 'ambient_temp': 19.0, 'tds': 300.0, 'ph': 7.1, 'tank_volume': 200.0, 'season': 3},
            {'name': 'Spring morning', 'temperature': 24.5, 'ambient_temp': 21.0, 'tds': 260.0, 'ph': 7.3, 'tank_volume': 200.0, 'season': 0},
        ]
        
        for condition in test_conditions:
            name = condition.pop('name')
            season = condition.pop('season')
            
            prediction = self.optimizer.predict(condition, season=season)
            
            self.assertNotIn('error', prediction)
            self.assertGreater(prediction['kp'], 0)
            self.assertGreater(prediction['ki'], 0)
            self.assertGreater(prediction['kd'], 0)
            
            print(f"✓ {name}: Kp={prediction['kp']:.2f}, Ki={prediction['ki']:.3f}, Kd={prediction['kd']:.2f}")


class TestWaterChangePredictorIntegration(unittest.TestCase):
    """Integration tests for water change predictor with real database"""
    
    @classmethod
    def setUpClass(cls):
        """Set up database and predictor"""
        try:
            cls.db = AquariumDatabase(
                TEST_DB_HOST,
                TEST_DB_PORT,
                TEST_DB_NAME,
                TEST_DB_USER,
                TEST_DB_PASSWORD
            )
            cls.predictor = WaterChangePredictor(cls.db)
            print(f"\n✓ Created water change predictor")
        except Exception as e:
            raise unittest.SkipTest(f"Cannot connect to test database: {e}")
    
    def test_01_populate_water_change_history(self):
        """Populate database with water change history"""
        base_ts = int(time.time())
        
        # Generate 20 water changes over ~1 year
        for i in range(20):
            # Water changes every 12-16 days
            days_between = 12 + (i % 5)
            end_ts = base_ts - (i * days_between * 86400)
            start_ts = end_ts - 3600  # 1 hour duration
            
            # TDS increases over time
            tds_before = 280 + (i % 14) * 5 + np.random.randn() * 10
            tds_after = 200 + np.random.randn() * 15
            
            data = {
                'startTime': start_ts,
                'endTime': end_ts,
                'volume': 40.0,
                'tempBefore': 24.5 + np.random.randn() * 0.5,
                'tempAfter': 23.8 + np.random.randn() * 0.5,
                'phBefore': 7.2 + np.random.randn() * 0.1,
                'phAfter': 7.0 + np.random.randn() * 0.1,
                'tdsBefore': tds_before,
                'tdsAfter': tds_after,
                'duration': 60,
                'successful': True
            }
            
            self.db.store_water_change(data)
        
        # Add filter maintenance records
        for i in range(5):
            cursor = self.db.conn.cursor()
            cursor.execute("""
                INSERT INTO filter_maintenance 
                (timestamp, filter_type, days_since_last, tds_before, tds_after, notes)
                VALUES (%s, %s, %s, %s, %s, %s)
            """, (
                base_ts - (i * 90 * 86400),  # Every 90 days
                'mechanical' if i % 2 == 0 else 'biological',
                90,
                310.0,
                280.0,
                f'Test maintenance {i}'
            ))
            self.db.conn.commit()
            cursor.close()
        
        print(f"✓ Populated 20 water change events and 5 filter maintenance records")
    
    def test_02_train_predictor(self):
        """Test training water change predictor"""
        result = self.predictor.train()
        
        self.assertNotIn('error', result)
        self.assertGreater(result['samples'], 0)
        self.assertIn('best_model', result)
        
        print(f"✓ Trained on {result['samples']} samples")
        print(f"  Best model: {result['best_model']}")
        print(f"  Scores: Linear={result['linear_r2']:.3f}, "
              f"RF={result['rf_r2']:.3f}, GB={result['gb_r2']:.3f}")
    
    def test_03_predict_next_change(self):
        """Test predicting next water change"""
        # First, store current sensor reading
        current_data = {
            'timestamp': int(time.time()),
            'temperature': 24.5,
            'ambientTemp': 22.0,
            'ph': 7.2,
            'tds': 300.0,
            'heaterState': 'ON',
            'co2State': 'OFF'
        }
        self.db.store_sensor_reading(current_data)
        
        # Make prediction
        prediction = self.predictor.predict()
        
        self.assertNotIn('error', prediction)
        self.assertGreater(prediction['predicted_days_remaining'], 0)
        self.assertIn('confidence', prediction)
        
        print(f"✓ Prediction: {prediction['predicted_days_remaining']:.1f} days remaining")
        print(f"  Confidence: {prediction['confidence']:.2f}")
        print(f"  TDS: {prediction['current_tds']:.1f} ppm")
        print(f"  TDS increase rate: {prediction['tds_increase_rate']:.1f} ppm/day")
        print(f"  Needs change soon: {prediction['needs_change_soon']}")


class TestMQTTIntegration(unittest.TestCase):
    """Integration tests for MQTT messaging"""
    
    @classmethod
    def setUpClass(cls):
        """Set up MQTT client and database"""
        try:
            # Database
            cls.db = AquariumDatabase(
                TEST_DB_HOST,
                TEST_DB_PORT,
                TEST_DB_NAME,
                TEST_DB_USER,
                TEST_DB_PASSWORD
            )
            
            # ML components
            cls.pid_temp = PIDOptimizer(cls.db, 'temp')
            cls.pid_co2 = PIDOptimizer(cls.db, 'co2')
            cls.wc_predictor = WaterChangePredictor(cls.db)
            
            # MQTT manager
            cls.mqtt_manager = MQTTManager(
                cls.db,
                {'temp': cls.pid_temp, 'co2': cls.pid_co2},
                cls.wc_predictor,
                broker=TEST_MQTT_BROKER,
                port=TEST_MQTT_PORT,
                username=TEST_MQTT_USER,
                password=TEST_MQTT_PASSWORD
            )
            
            # Test client for receiving published messages
            cls.test_client = mqtt.Client()
            if TEST_MQTT_USER:
                cls.test_client.username_pw_set(TEST_MQTT_USER, TEST_MQTT_PASSWORD)
            
            cls.received_messages = []
            
            def on_message(client, userdata, msg):
                cls.received_messages.append({
                    'topic': msg.topic,
                    'payload': json.loads(msg.payload.decode())
                })
            
            cls.test_client.on_message = on_message
            cls.test_client.connect(TEST_MQTT_BROKER, TEST_MQTT_PORT, 60)
            cls.test_client.subscribe("aquarium/ml/#")
            cls.test_client.loop_start()
            
            # Connect MQTT manager
            cls.mqtt_manager.connect()
            time.sleep(2)  # Allow connections to establish
            
            print(f"\n✓ Connected to MQTT broker: {TEST_MQTT_BROKER}:{TEST_MQTT_PORT}")
            
        except Exception as e:
            raise unittest.SkipTest(f"Cannot connect to MQTT broker: {e}")
    
    @classmethod
    def tearDownClass(cls):
        """Clean up MQTT connections"""
        if hasattr(cls, 'test_client'):
            cls.test_client.loop_stop()
            cls.test_client.disconnect()
        if hasattr(cls, 'mqtt_manager'):
            cls.mqtt_manager.client.loop_stop()
            cls.mqtt_manager.client.disconnect()
        print("\n✓ Disconnected from MQTT broker")
    
    def setUp(self):
        """Clear received messages before each test"""
        self.__class__.received_messages = []
    
    def test_01_publish_sensor_data(self):
        """Test publishing and receiving sensor data"""
        sensor_data = {
            'timestamp': int(time.time()),
            'temperature': 24.5,
            'ambientTemp': 22.0,
            'ph': 7.2,
            'tds': 300.0,
            'heaterState': 'ON',
            'co2State': 'OFF'
        }
        
        # Publish via test client
        self.test_client.publish('aquarium/data', json.dumps(sensor_data))
        
        # Wait for message to be received and processed
        time.sleep(1)
        
        # Verify data was stored in database
        cursor = self.db.conn.cursor()
        cursor.execute("SELECT * FROM sensor_readings ORDER BY id DESC LIMIT 1")
        row = cursor.fetchone()
        cursor.close()
        
        self.assertIsNotNone(row)
        self.assertEqual(row[2], 24.5)  # temperature
        
        print(f"✓ Sensor data published and stored: temp={row[2]}°C, TDS={row[5]} ppm")
    
    def test_02_publish_pid_gains(self):
        """Test publishing PID gains"""
        gains = {
            'controller': 'temp',
            'season': 1,
            'kp': 10.5,
            'ki': 0.7,
            'kd': 5.2,
            'confidence': 0.85,
            'model': 'gradient_boosting',
            'timestamp': int(time.time())
        }
        
        success = self.mqtt_manager.publish_pid_gains(gains)
        self.assertTrue(success)
        
        # Wait for message
        time.sleep(1)
        
        # Check if received
        pid_messages = [m for m in self.received_messages if 'pid' in m['topic']]
        self.assertGreater(len(pid_messages), 0)
        
        msg = pid_messages[0]
        self.assertEqual(msg['payload']['controller'], 'temp')
        self.assertEqual(msg['payload']['kp'], 10.5)
        
        print(f"✓ PID gains published: Kp={msg['payload']['kp']}, "
              f"Ki={msg['payload']['ki']}, Kd={msg['payload']['kd']}")
    
    def test_03_publish_wc_prediction(self):
        """Test publishing water change prediction"""
        prediction = {
            'predicted_days_remaining': 3.5,
            'predicted_total_cycle_days': 14.0,
            'days_since_last_change': 10.5,
            'current_tds': 315.0,
            'tds_increase_rate': 7.5,
            'confidence': 0.82,
            'model': 'gradient_boost',
            'needs_change_soon': True,
            'timestamp': int(time.time())
        }
        
        success = self.mqtt_manager.publish_wc_prediction(prediction)
        self.assertTrue(success)
        
        # Wait for message
        time.sleep(1)
        
        # Check if received
        wc_messages = [m for m in self.received_messages if 'prediction' in m['topic']]
        self.assertGreater(len(wc_messages), 0)
        
        msg = wc_messages[0]
        self.assertEqual(msg['payload']['predicted_days_remaining'], 3.5)
        self.assertTrue(msg['payload']['needs_change_soon'])
        
        print(f"✓ Water change prediction published: {msg['payload']['predicted_days_remaining']} days, "
              f"confidence={msg['payload']['confidence']:.2f}")


class TestEndToEndWorkflow(unittest.TestCase):
    """End-to-end integration test of complete workflow"""
    
    @classmethod
    def setUpClass(cls):
        """Set up complete system"""
        try:
            cls.service = AquariumMLService(
                db_host=TEST_DB_HOST,
                db_port=TEST_DB_PORT,
                db_name=TEST_DB_NAME,
                db_user=TEST_DB_USER,
                db_password=TEST_DB_PASSWORD,
                mqtt_broker=TEST_MQTT_BROKER,
                mqtt_port=TEST_MQTT_PORT,
                mqtt_user=TEST_MQTT_USER,
                mqtt_password=TEST_MQTT_PASSWORD
            )
            print(f"\n✓ Created unified ML service")
        except Exception as e:
            raise unittest.SkipTest(f"Cannot create service: {e}")
    
    def test_complete_workflow(self):
        """Test complete training and prediction workflow"""
        print("\n=== Running Complete Workflow ===")
        
        # 1. Populate training data (abbreviated for speed)
        print("\n1. Populating training data...")
        base_ts = int(time.time())
        
        # PID performance data
        for i in range(60):
            data = {
                'timestamp': base_ts - (i * 3600),
                'controller': 'temp',
                'kp': 10.0 + np.random.randn() * 0.5,
                'ki': 0.5 + np.random.randn() * 0.05,
                'kd': 5.0 + np.random.randn() * 0.3,
                'settling_time': 12.0 + np.random.randn() * 2.0,
                'overshoot': 0.3 + np.random.randn() * 0.1,
                'steady_state_error': 0.05,
                'temperature': 24.5,
                'ambient_temp': 22.0,
                'tds': 300.0,
                'ph': 7.2,
                'hour': (i * 3) % 24,
                'season': i % 4,
                'tank_volume': 200.0
            }
            self.service.db.store_pid_performance(data)
        
        # Water change data
        for i in range(15):
            data = {
                'startTime': base_ts - (i * 14 * 86400) - 3600,
                'endTime': base_ts - (i * 14 * 86400),
                'volume': 40.0,
                'tempBefore': 24.5,
                'tempAfter': 23.8,
                'phBefore': 7.2,
                'phAfter': 7.0,
                'tdsBefore': 320.0,
                'tdsAfter': 210.0,
                'duration': 60,
                'successful': True
            }
            self.service.db.store_water_change(data)
        
        print("✓ Populated 60 PID records and 15 water change events")
        
        # 2. Train all models
        print("\n2. Training all models...")
        self.service.train_all_pid_models()
        self.service.train_wc_model()
        print("✓ Training complete")
        
        # 3. Generate predictions
        print("\n3. Generating predictions...")
        
        # Store current sensor data
        sensor_data = {
            'timestamp': int(time.time()),
            'temperature': 24.5,
            'ambientTemp': 22.0,
            'ph': 7.2,
            'tds': 300.0,
            'heaterState': 'ON',
            'co2State': 'OFF'
        }
        self.service.db.store_sensor_reading(sensor_data)
        
        # Publish predictions
        self.service.publish_pid_predictions()
        
        print("✓ Predictions published")
        
        # 4. Verify predictions were stored
        print("\n4. Verifying stored predictions...")
        
        cursor = self.service.db.conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM pid_gains")
        pid_count = cursor.fetchone()[0]
        
        cursor.execute("SELECT COUNT(*) FROM wc_predictions")
        wc_count = cursor.fetchone()[0]
        cursor.close()
        
        self.assertGreater(pid_count, 0, "No PID gains stored")
        self.assertGreater(wc_count, 0, "No water change predictions stored")
        
        print(f"✓ Stored {pid_count} PID gain records")
        print(f"✓ Stored {wc_count} water change predictions")
        print("\n=== Workflow Complete ===")


def run_integration_tests():
    """Run all integration tests"""
    # Check if we should skip integration tests
    if os.environ.get('SKIP_INTEGRATION', '').lower() in ('1', 'true', 'yes'):
        print("Skipping integration tests (SKIP_INTEGRATION set)")
        return 0
    
    print("=" * 70)
    print("INTEGRATION TESTS FOR UNIFIED AQUARIUM ML SERVICE")
    print("=" * 70)
    print(f"\nTest Configuration:")
    print(f"  Database: {TEST_DB_USER}@{TEST_DB_HOST}:{TEST_DB_PORT}/{TEST_DB_NAME}")
    print(f"  MQTT Broker: {TEST_MQTT_BROKER}:{TEST_MQTT_PORT}")
    print()
    
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add test classes in order
    suite.addTests(loader.loadTestsFromTestCase(TestDatabaseIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestPIDOptimizerIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestWaterChangePredictorIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestMQTTIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestEndToEndWorkflow))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    print("\n" + "=" * 70)
    print("INTEGRATION TEST SUMMARY")
    print("=" * 70)
    print(f"Tests run: {result.testsRun}")
    print(f"Successes: {result.testsRun - len(result.failures) - len(result.errors)}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Skipped: {len(result.skipped)}")
    
    # Return exit code
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_integration_tests())
