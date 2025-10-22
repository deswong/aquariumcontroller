#!/usr/bin/env python3
"""
Unit Tests for Unified Aquarium ML Service

Tests:
1. PID Optimizer - feature extraction, training, prediction
2. Water Change Predictor - feature extraction, training, prediction
3. Database operations - CRUD operations
4. MQTT integration - message handling, publishing

Usage:
    python3 test_aquarium_ml_service.py
    pytest test_aquarium_ml_service.py -v
"""

import json
import sys
import time
import unittest
from datetime import datetime
from unittest.mock import MagicMock, Mock, patch

import numpy as np

# Add parent directory to path
sys.path.insert(0, '.')

from aquarium_ml_service import (
    AquariumDatabase,
    PIDOptimizer,
    WaterChangePredictor,
    MQTTManager,
    AquariumMLService
)


class TestAquariumDatabase(unittest.TestCase):
    """Test database operations"""
    
    @patch('aquarium_ml_service.mysql.connector.connect')
    def setUp(self, mock_connect):
        """Set up test database"""
        self.mock_conn = Mock()
        self.mock_cursor = Mock()
        self.mock_conn.cursor.return_value = self.mock_cursor
        mock_connect.return_value = self.mock_conn
        
        self.db = AquariumDatabase('localhost', 3306, 'test', 'test', 'test')
    
    def test_store_sensor_reading(self):
        """Test storing sensor reading"""
        data = {
            'timestamp': int(time.time()),
            'temperature': 24.5,
            'ambientTemp': 22.0,
            'ph': 7.2,
            'tds': 300.0,
            'heaterState': 'ON',
            'co2State': 'OFF'
        }
        
        self.db.store_sensor_reading(data)
        
        # Verify INSERT was called
        self.mock_cursor.execute.assert_called()
        call_args = self.mock_cursor.execute.call_args[0]
        self.assertIn('INSERT INTO sensor_readings', call_args[0])
    
    def test_store_pid_performance(self):
        """Test storing PID performance data"""
        data = {
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
        
        self.db.store_pid_performance(data)
        
        # Verify INSERT was called
        self.mock_cursor.execute.assert_called()
        call_args = self.mock_cursor.execute.call_args[0]
        self.assertIn('INSERT INTO pid_performance', call_args[0])
    
    def test_store_water_change(self):
        """Test storing water change event"""
        wc_data = {
            'startTime': int(time.time()) - 3600,
            'endTime': int(time.time()),
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
        
        self.db.store_water_change(wc_data)
        
        # Verify INSERT was called
        self.mock_cursor.execute.assert_called()
        call_args = self.mock_cursor.execute.call_args[0]
        self.assertIn('INSERT INTO water_changes', call_args[0])


class TestPIDOptimizer(unittest.TestCase):
    """Test PID optimizer"""
    
    @patch('aquarium_ml_service.mysql.connector.connect')
    def setUp(self, mock_connect):
        """Set up test optimizer"""
        self.mock_conn = Mock()
        self.mock_cursor = Mock()
        self.mock_conn.cursor.return_value = self.mock_cursor
        mock_connect.return_value = self.mock_conn
        
        self.db = AquariumDatabase('localhost', 3306, 'test', 'test', 'test')
        self.optimizer = PIDOptimizer(self.db, 'temp')
    
    def test_extract_features(self):
        """Test feature extraction from performance history"""
        # Create sample history
        history = []
        for i in range(100):
            history.append({
                'timestamp': int(time.time()) - (i * 3600),
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
                'hour': 14,
                'tank_volume': 200.0
            })
        
        X, y_kp, y_ki, y_kd, weights = self.optimizer.extract_features(history)
        
        # Verify shapes
        self.assertEqual(X.shape[0], 100)
        self.assertEqual(X.shape[1], 7)  # 7 features
        self.assertEqual(len(y_kp), 100)
        self.assertEqual(len(y_ki), 100)
        self.assertEqual(len(y_kd), 100)
        self.assertEqual(len(weights), 100)
        
        # Verify weights are positive
        self.assertTrue(np.all(weights > 0))
    
    def test_train_season_insufficient_data(self):
        """Test training with insufficient data"""
        # Mock insufficient data
        self.mock_cursor.fetchall.return_value = []
        self.mock_cursor.description = [('id',), ('timestamp',)]
        
        result = self.optimizer.train_season(0)
        
        self.assertIn('error', result)
        self.assertEqual(result['error'], 'insufficient_data')
    
    def test_predict_no_model(self):
        """Test prediction without trained model"""
        sensor_data = {
            'temperature': 24.5,
            'ambient_temp': 22.0,
            'tds': 300.0,
            'ph': 7.2,
            'tank_volume': 200.0
        }
        
        result = self.optimizer.predict(sensor_data, season=0)
        
        self.assertIn('error', result)
        self.assertEqual(result['error'], 'no_model')


class TestWaterChangePredictor(unittest.TestCase):
    """Test water change predictor"""
    
    @patch('aquarium_ml_service.mysql.connector.connect')
    def setUp(self, mock_connect):
        """Set up test predictor"""
        self.mock_conn = Mock()
        self.mock_cursor = Mock()
        self.mock_conn.cursor.return_value = self.mock_cursor
        mock_connect.return_value = self.mock_conn
        
        self.db = AquariumDatabase('localhost', 3306, 'test', 'test', 'test')
        self.predictor = WaterChangePredictor(self.db)
    
    def test_extract_features(self):
        """Test feature extraction from water change history"""
        # Create sample history
        history = []
        base_ts = int(time.time())
        for i in range(10):
            history.append({
                'end_timestamp': base_ts - (i * 14 * 86400),  # Every 14 days
                'tds_before': 320.0 + np.random.randn() * 20,
                'tds_after': 210.0 + np.random.randn() * 10,
                'volume_litres': 40.0,
                'completed': 1
            })
        
        # Need last filter maintenance
        self.mock_cursor.fetchone.return_value = (1, base_ts - 30 * 86400, 'mechanical', 30, 300, 280, '')
        self.mock_cursor.description = [('id',), ('timestamp',), ('filter_type',), 
                                         ('days_since_last',), ('tds_before',), ('tds_after',), ('notes',)]
        
        X, y = self.predictor.extract_features(history)
        
        # Verify shapes
        self.assertEqual(X.shape[0], 9)  # 10 records - 1 (need previous)
        self.assertEqual(X.shape[1], 6)  # 6 features
        self.assertEqual(len(y), 9)
        
        # Verify all features are positive
        self.assertTrue(np.all(X >= 0))
    
    def test_train_insufficient_data(self):
        """Test training with insufficient data"""
        # Mock insufficient data
        self.mock_cursor.fetchall.return_value = []
        
        result = self.predictor.train()
        
        self.assertIn('error', result)
        self.assertEqual(result['error'], 'insufficient_data')
    
    def test_predict_no_model(self):
        """Test prediction without trained model"""
        result = self.predictor.predict()
        
        self.assertIn('error', result)
        self.assertEqual(result['error'], 'no_model')


class TestMQTTIntegration(unittest.TestCase):
    """Test MQTT integration"""
    
    @patch('aquarium_ml_service.mysql.connector.connect')
    @patch('aquarium_ml_service.mqtt.Client')
    def setUp(self, mock_mqtt_client, mock_connect):
        """Set up test MQTT manager"""
        self.mock_conn = Mock()
        self.mock_cursor = Mock()
        self.mock_conn.cursor.return_value = self.mock_cursor
        mock_connect.return_value = self.mock_conn
        
        self.mock_mqtt = Mock()
        mock_mqtt_client.return_value = self.mock_mqtt
        
        self.db = AquariumDatabase('localhost', 3306, 'test', 'test', 'test')
        self.pid_temp = PIDOptimizer(self.db, 'temp')
        self.pid_co2 = PIDOptimizer(self.db, 'co2')
        self.wc_predictor = WaterChangePredictor(self.db)
        
        self.mqtt_manager = MQTTManager(
            self.db,
            {'temp': self.pid_temp, 'co2': self.pid_co2},
            self.wc_predictor
        )
    
    def test_publish_pid_gains(self):
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
        
        # Mock successful publish
        result_mock = Mock()
        result_mock.rc = 0  # MQTT_ERR_SUCCESS
        self.mock_mqtt.publish.return_value = result_mock
        
        success = self.mqtt_manager.publish_pid_gains(gains)
        
        self.assertTrue(success)
        self.mock_mqtt.publish.assert_called_once()
    
    def test_publish_wc_prediction(self):
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
        
        # Mock successful publish
        result_mock = Mock()
        result_mock.rc = 0
        self.mock_mqtt.publish.return_value = result_mock
        
        success = self.mqtt_manager.publish_wc_prediction(prediction)
        
        self.assertTrue(success)
        self.mock_mqtt.publish.assert_called_once()
    
    def test_on_message_sensor_data(self):
        """Test handling sensor data message"""
        payload = json.dumps({
            'timestamp': int(time.time()),
            'temperature': 24.5,
            'ambientTemp': 22.0,
            'ph': 7.2,
            'tds': 300.0,
            'heaterState': 'ON',
            'co2State': 'OFF'
        })
        
        msg = Mock()
        msg.topic = 'aquarium/data'
        msg.payload = payload.encode()
        
        self.mqtt_manager.on_message(None, None, msg)
        
        # Verify sensor reading was stored
        self.mock_cursor.execute.assert_called()
    
    def test_on_message_pid_performance(self):
        """Test handling PID performance message"""
        payload = json.dumps({
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
        })
        
        msg = Mock()
        msg.topic = 'aquarium/pid/performance'
        msg.payload = payload.encode()
        
        self.mqtt_manager.on_message(None, None, msg)
        
        # Verify performance data was stored
        self.mock_cursor.execute.assert_called()


class TestEndToEndIntegration(unittest.TestCase):
    """Test end-to-end integration"""
    
    @patch('aquarium_ml_service.mysql.connector.connect')
    @patch('aquarium_ml_service.mqtt.Client')
    def test_full_pid_workflow(self, mock_mqtt_client, mock_connect):
        """Test complete PID optimization workflow"""
        # Setup mocks
        mock_conn = Mock()
        mock_cursor = Mock()
        mock_conn.cursor.return_value = mock_cursor
        mock_connect.return_value = mock_conn
        
        # Mock PID performance history
        history = []
        for i in range(60):
            history.append((
                i + 1,  # id
                int(time.time()) - (i * 3600),  # timestamp
                'temp',  # controller
                10.0 + np.random.randn() * 0.5,  # kp
                0.5 + np.random.randn() * 0.05,  # ki
                5.0 + np.random.randn() * 0.3,  # kd
                12.0 + np.random.randn() * 2.0,  # settling_time
                0.3 + np.random.randn() * 0.1,  # overshoot
                0.05,  # steady_state_error
                24.5,  # temperature
                22.0,  # ambient_temp
                300.0,  # tds
                7.2,  # ph
                14,  # hour
                1,  # season (summer)
                200.0  # tank_volume
            ))
        
        mock_cursor.fetchall.return_value = history
        mock_cursor.description = [
            ('id',), ('timestamp',), ('controller',), ('kp',), ('ki',), ('kd',),
            ('settling_time',), ('overshoot',), ('steady_state_error',),
            ('temperature',), ('ambient_temp',), ('tds',), ('ph',),
            ('hour',), ('season',), ('tank_volume',)
        ]
        
        # Create optimizer
        db = AquariumDatabase('localhost', 3306, 'test', 'test', 'test')
        optimizer = PIDOptimizer(db, 'temp')
        
        # Train
        result = optimizer.train_season(1)  # Summer
        
        # Verify training succeeded
        self.assertNotIn('error', result)
        self.assertGreater(result['samples'], 0)
        self.assertIn('avg_score', result)
        
        # Predict
        sensor_data = {
            'temperature': 24.5,
            'ambient_temp': 22.0,
            'tds': 300.0,
            'ph': 7.2,
            'tank_volume': 200.0
        }
        
        prediction = optimizer.predict(sensor_data, season=1)
        
        # Verify prediction
        self.assertNotIn('error', prediction)
        self.assertIn('kp', prediction)
        self.assertIn('ki', prediction)
        self.assertIn('kd', prediction)
        self.assertGreater(prediction['kp'], 0)
        self.assertGreater(prediction['ki'], 0)
        self.assertGreater(prediction['kd'], 0)


def run_tests():
    """Run all tests"""
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    suite.addTests(loader.loadTestsFromTestCase(TestAquariumDatabase))
    suite.addTests(loader.loadTestsFromTestCase(TestPIDOptimizer))
    suite.addTests(loader.loadTestsFromTestCase(TestWaterChangePredictor))
    suite.addTests(loader.loadTestsFromTestCase(TestMQTTIntegration))
    suite.addTests(loader.loadTestsFromTestCase(TestEndToEndIntegration))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Return exit code
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())
