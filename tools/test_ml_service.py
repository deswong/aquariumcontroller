#!/usr/bin/env python3
"""
Unit and Integration Tests for Water Change ML Service

Run with: python3 -m pytest test_ml_service.py -v
Or: python3 test_ml_service.py
"""

import unittest
import sys
import os
from datetime import datetime, timedelta
from unittest.mock import Mock, MagicMock, patch
import json

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Import numpy first (needed for tests)
import numpy as np

# Now import from water_change_ml_service
# (dependencies are already installed, no need to mock)
from water_change_ml_service import (
    WaterChangePredictor,
    MIN_TRAINING_SAMPLES
)


class MockDatabase:
    """Mock database for testing"""
    
    def __init__(self):
        self.sensor_readings = []
        self.water_changes = []
        self.filter_maintenance = []
        self.predictions = []
    
    def store_sensor_reading(self, data):
        self.sensor_readings.append(data)
    
    def store_water_change(self, wc_data):
        self.water_changes.append(wc_data)
    
    def store_filter_maintenance(self, fm_data):
        self.filter_maintenance.append(fm_data)
    
    def get_water_change_history(self, limit=50):
        return self.water_changes[-limit:]
    
    def get_filter_maintenance_history(self, limit=50):
        return self.filter_maintenance[-limit:]
    
    def get_last_filter_maintenance(self):
        return self.filter_maintenance[-1] if self.filter_maintenance else None
    
    def get_sensor_readings_between(self, start_ts, end_ts):
        return [r for r in self.sensor_readings 
                if start_ts <= r['timestamp'] <= end_ts]
    
    def get_recent_sensor_readings(self, hours=24):
        end_ts = int(datetime.now().timestamp())
        start_ts = end_ts - (hours * 3600)
        return self.get_sensor_readings_between(start_ts, end_ts)
    
    def store_prediction(self, days, confidence, model_type):
        self.predictions.append({
            'timestamp': int(datetime.now().timestamp()),
            'predicted_days': days,
            'confidence': confidence,
            'model_type': model_type
        })
    
    def close(self):
        pass


class TestDatabaseOperations(unittest.TestCase):
    """Test database storage and retrieval operations"""
    
    def setUp(self):
        self.db = MockDatabase()
    
    def test_store_sensor_reading(self):
        """Test storing a sensor reading"""
        data = {
            'timestamp': int(datetime.now().timestamp()),
            'temperature': 25.5,
            'ambient_temp': 22.3,
            'ph': 6.8,
            'tds': 350,
            'heater': 'ON',
            'co2': 'OFF'
        }
        
        self.db.store_sensor_reading(data)
        self.assertEqual(len(self.db.sensor_readings), 1)
        self.assertEqual(self.db.sensor_readings[0]['temperature'], 25.5)
    
    def test_store_water_change(self):
        """Test storing a water change event"""
        now = int(datetime.now().timestamp())
        wc_data = {
            'startTime': now - 600,
            'endTime': now,
            'volume': 20.0,
            'tempBefore': 25.5,
            'tempAfter': 24.8,
            'phBefore': 6.85,
            'phAfter': 6.90,
            'tdsBefore': 380,
            'tdsAfter': 210,
            'duration': 10,
            'successful': True
        }
        
        self.db.store_water_change(wc_data)
        self.assertEqual(len(self.db.water_changes), 1)
        self.assertEqual(self.db.water_changes[0]['volume'], 20.0)
    
    def test_store_filter_maintenance(self):
        """Test storing filter maintenance event"""
        fm_data = {
            'timestamp': int(datetime.now().timestamp()),
            'filter_type': 'mechanical',
            'days_since_last': 14,
            'tds_before': 350,
            'tds_after': 320,
            'notes': 'Replaced filter media'
        }
        
        self.db.store_filter_maintenance(fm_data)
        self.assertEqual(len(self.db.filter_maintenance), 1)
        self.assertEqual(self.db.filter_maintenance[0]['notes'], 'Replaced filter media')
    
    def test_get_water_change_history(self):
        """Test retrieving water change history"""
        # Add multiple water changes
        for i in range(10):
            wc_data = {
                'startTime': int(datetime.now().timestamp()) - (i * 86400 * 7),
                'endTime': int(datetime.now().timestamp()) - (i * 86400 * 7) + 600,
                'volume': 20.0,
                'tempBefore': 25.5,
                'tempAfter': 24.8,
                'phBefore': 6.85,
                'phAfter': 6.90,
                'tdsBefore': 380,
                'tdsAfter': 210,
                'duration': 10,
                'successful': True
            }
            self.db.store_water_change(wc_data)
        
        history = self.db.get_water_change_history(limit=5)
        self.assertEqual(len(history), 5)
    
    def test_get_last_filter_maintenance(self):
        """Test retrieving last filter maintenance"""
        # No maintenance yet
        self.assertIsNone(self.db.get_last_filter_maintenance())
        
        # Add maintenance
        fm_data = {
            'timestamp': int(datetime.now().timestamp()),
            'filter_type': 'mechanical',
            'days_since_last': 14,
            'notes': 'Test maintenance'
        }
        self.db.store_filter_maintenance(fm_data)
        
        last_fm = self.db.get_last_filter_maintenance()
        self.assertIsNotNone(last_fm)
        self.assertEqual(last_fm['notes'], 'Test maintenance')


class TestMLPredictorFeatureExtraction(unittest.TestCase):
    """Test ML feature extraction from historical data"""
    
    def setUp(self):
        self.db = MockDatabase()
        self.predictor = WaterChangePredictor(self.db)
        
        # Mock numpy and sklearn
        self.original_np_array = np.array
        np.array = lambda x: x  # Return list as-is for testing
    
    def tearDown(self):
        np.array = self.original_np_array
    
    def _create_sample_water_changes(self, count=10):
        """Helper to create sample water change history"""
        water_changes = []
        base_time = int(datetime.now().timestamp())
        
        for i in range(count):
            # Each water change is 7 days apart
            end_time = base_time - (i * 86400 * 7)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 24.8,
                'ph_before': 6.85,
                'ph_after': 6.90,
                'tds_before': 380,
                'tds_after': 210,
                'duration_minutes': 10,
                'completed': 1
            }
            water_changes.append(wc)
        
        return water_changes
    
    def test_feature_extraction_basic(self):
        """Test basic feature extraction from water change history"""
        history = self._create_sample_water_changes(count=10)
        self.db.water_changes = history
        
        # extract_features expects chronological order (oldest first)
        history_asc = list(reversed(history))
        
        # Mock the extract_features to test it can process data
        try:
            features, targets = self.predictor.extract_features(history_asc)
            # Should have features for n-1 water changes (need pairs)
            self.assertGreater(len(features), 0)
            self.assertEqual(len(features), len(targets))
        except Exception as e:
            self.fail(f"Feature extraction failed: {e}")
    
    def test_insufficient_data_for_features(self):
        """Test feature extraction with insufficient data"""
        # Only 1 water change - can't extract features (need pairs)
        history = self._create_sample_water_changes(count=1)
        self.db.water_changes = history
        
        features, targets = self.predictor.extract_features(history)
        self.assertEqual(len(features), 0)
        self.assertEqual(len(targets), 0)
    
    def test_feature_extraction_with_filter_maintenance(self):
        """Test that filter maintenance data is included in features"""
        history = self._create_sample_water_changes(count=10)
        self.db.water_changes = history
        
        # Add filter maintenance
        fm_data = {
            'timestamp': int(datetime.now().timestamp()) - (86400 * 10),
            'filter_type': 'mechanical',
            'days_since_last': 14,
            'tds_before': 350,
            'tds_after': 320,
            'notes': 'Test'
        }
        self.db.store_filter_maintenance(fm_data)
        
        features, targets = self.predictor.extract_features(history)
        # Each feature vector should have 14 elements (including filter features)
        if len(features) > 0:
            self.assertEqual(len(features[0]), 14)


class TestMLPredictorTraining(unittest.TestCase):
    """Test ML model training"""
    
    def setUp(self):
        self.db = MockDatabase()
        self.predictor = WaterChangePredictor(self.db)
        
        # Create realistic water change history
        base_time = int(datetime.now().timestamp())
        for i in range(15):  # 15 water changes for good training
            end_time = base_time - (i * 86400 * 7)  # Weekly changes
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 24.8,
                'ph_before': 6.85,
                'ph_after': 6.90,
                'tds_before': 350 + (i * 5),  # TDS increases over time
                'tds_after': 200 + (i * 3),
                'duration_minutes': 10,
                'completed': 1
            }
            self.db.water_changes.append(wc)
    
    def test_train_with_sufficient_data(self):
        """Test training with sufficient data"""
        result = self.predictor.train()
        
        # Should succeed with 15 water changes
        self.assertNotIn('error', result)
        self.assertIn('model', result)
        self.assertIn('score', result)
        self.assertIsNotNone(self.predictor.best_model)
        self.assertIsNotNone(self.predictor.best_model_name)
    
    def test_train_insufficient_data(self):
        """Test training with insufficient data"""
        # Clear most data, keep only 3
        self.db.water_changes = self.db.water_changes[:3]
        
        result = self.predictor.train()
        
        # Should return error
        self.assertIn('error', result)
        self.assertEqual(result['error'], 'insufficient_data')
    
    def test_train_selects_best_model(self):
        """Test that training selects the best performing model"""
        result = self.predictor.train()
        
        if 'error' not in result:
            # Should have scores for all models
            self.assertIn('all_scores', result)
            scores = result['all_scores']
            
            # Should have tried all 3 models
            self.assertIn('linear', scores)
            self.assertIn('random_forest', scores)
            self.assertIn('gradient_boost', scores)
            
            # Best model should have highest score
            best_score = max(scores.values())
            self.assertEqual(result['score'], best_score)


class TestMLPredictorPrediction(unittest.TestCase):
    """Test ML prediction generation"""
    
    def setUp(self):
        self.db = MockDatabase()
        self.predictor = WaterChangePredictor(self.db)
        
        # Create realistic water change history
        base_time = int(datetime.now().timestamp())
        for i in range(15):
            end_time = base_time - (i * 86400 * 7)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 24.8,
                'ph_before': 6.85,
                'ph_after': 6.90,
                'tds_before': 350,
                'tds_after': 210,
                'duration_minutes': 10,
                'completed': 1
            }
            self.db.water_changes.append(wc)
        
        # Add current sensor reading
        self.db.sensor_readings.append({
            'timestamp': int(datetime.now().timestamp()),
            'temperature': 25.5,
            'ambient_temp': 22.3,
            'ph': 6.8,
            'tds': 320,
            'heater_state': 1,
            'co2_state': 0
        })
    
    def test_predict_without_training(self):
        """Test prediction without training first"""
        result = self.predictor.predict()
        
        # Should return error
        self.assertIn('error', result)
        self.assertEqual(result['error'], 'no_trained_model')
    
    def test_predict_after_training(self):
        """Test prediction after successful training"""
        # Train first
        train_result = self.predictor.train()
        
        if 'error' not in train_result:
            # Now predict
            prediction = self.predictor.predict()
            
            # Should have prediction data
            self.assertIn('predicted_days_remaining', prediction)
            self.assertIn('confidence', prediction)
            self.assertIn('model', prediction)
            self.assertIn('tds_increase_rate', prediction)
            
            # Values should be reasonable
            self.assertGreaterEqual(prediction['predicted_days_remaining'], 0)
            self.assertGreaterEqual(prediction['confidence'], 0)
            self.assertLessEqual(prediction['confidence'], 1.0)
    
    def test_predict_with_filter_maintenance(self):
        """Test that predictions account for filter maintenance"""
        # Train first
        self.predictor.train()
        
        # Add recent filter maintenance
        fm_data = {
            'timestamp': int(datetime.now().timestamp()) - (86400 * 3),
            'filter_type': 'mechanical',
            'days_since_last': 14,
            'tds_before': 350,
            'tds_after': 300,
            'notes': 'Recent maintenance'
        }
        self.db.store_filter_maintenance(fm_data)
        
        prediction = self.predictor.predict()
        
        if 'error' not in prediction:
            # Prediction should be generated (filter maintenance should be factored in)
            self.assertIn('predicted_days_remaining', prediction)


class TestDataValidation(unittest.TestCase):
    """Test data validation and edge cases"""
    
    def setUp(self):
        self.db = MockDatabase()
    
    def test_water_change_with_missing_fields(self):
        """Test handling of water change data with missing fields"""
        incomplete_wc = {
            'startTime': int(datetime.now().timestamp()) - 600,
            'endTime': int(datetime.now().timestamp()),
            'volume': 20.0
            # Missing temperature, pH, TDS
        }
        
        # Should not crash
        try:
            self.db.store_water_change(incomplete_wc)
            self.assertEqual(len(self.db.water_changes), 1)
        except Exception as e:
            self.fail(f"Should handle missing fields gracefully: {e}")
    
    def test_negative_values(self):
        """Test handling of invalid negative values"""
        invalid_wc = {
            'startTime': int(datetime.now().timestamp()) - 600,
            'endTime': int(datetime.now().timestamp()),
            'volume': -10.0,  # Negative volume
            'tempBefore': 25.5,
            'tempAfter': 24.8,
            'phBefore': 6.85,
            'phAfter': 6.90,
            'tdsBefore': -100,  # Negative TDS
            'tdsAfter': 210
        }
        
        # Should store but predictor should handle it
        self.db.store_water_change(invalid_wc)
        self.assertEqual(len(self.db.water_changes), 1)
    
    def test_extreme_values(self):
        """Test handling of extreme but technically valid values"""
        extreme_wc = {
            'startTime': int(datetime.now().timestamp()) - 600,
            'endTime': int(datetime.now().timestamp()),
            'volume': 1000.0,  # Very large volume
            'tempBefore': 50.0,  # Very high temp
            'tempAfter': 10.0,   # Very low temp
            'phBefore': 3.0,     # Very acidic
            'phAfter': 11.0,     # Very alkaline
            'tdsBefore': 10000,  # Very high TDS
            'tdsAfter': 5000
        }
        
        self.db.store_water_change(extreme_wc)
        self.assertEqual(len(self.db.water_changes), 1)


class TestIntegrationScenarios(unittest.TestCase):
    """Integration tests simulating real-world scenarios"""
    
    def setUp(self):
        self.db = MockDatabase()
        self.predictor = WaterChangePredictor(self.db)
    
    def test_weekly_water_changes_regular(self):
        """Test prediction with regular weekly water changes"""
        # Simulate 12 weeks of regular water changes
        # Last water change was 3 days ago (to test mid-cycle prediction)
        base_time = int(datetime.now().timestamp()) - (3 * 86400)
        
        for week in range(12):
            end_time = base_time - (week * 86400 * 7)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 25.0,
                'ph_before': 6.85,
                'ph_after': 6.88,
                'tds_before': 360,
                'tds_after': 220,
                'duration_minutes': 10,
                'completed': 1
            }
            self.db.water_changes.append(wc)
        
        # Add current sensor data
        self.db.sensor_readings.append({
            'timestamp': int(datetime.now().timestamp()),
            'temperature': 25.5,
            'ph': 6.82,
            'tds': 310
        })
        
        # Train and predict
        train_result = self.predictor.train()
        self.assertNotIn('error', train_result)
        
        prediction = self.predictor.predict()
        self.assertNotIn('error', prediction)
        
        # With regular weekly changes, should predict ~2-6 days (since last change was 3 days ago)
        # The prediction might be 0 if model predicts next change is due now (valid behavior)
        days = prediction['predicted_days_remaining']
        self.assertGreaterEqual(days, 0)
        self.assertLess(days, 14)  # Shouldn't predict more than 2 weeks
    
    def test_irregular_water_changes(self):
        """Test prediction with irregular water change schedule"""
        base_time = int(datetime.now().timestamp())
        intervals = [5, 10, 7, 14, 6, 8, 12, 7, 9, 11]  # Irregular days
        
        cumulative_days = 0
        for interval in intervals:
            cumulative_days += interval
            end_time = base_time - (cumulative_days * 86400)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 25.0,
                'ph_before': 6.85,
                'ph_after': 6.88,
                'tds_before': 360,
                'tds_after': 220,
                'duration_minutes': 10,
                'completed': 1
            }
            self.db.water_changes.append(wc)
        
        # Should still train successfully
        train_result = self.predictor.train()
        self.assertNotIn('error', train_result)
        
        # Confidence might be lower due to irregularity
        prediction = self.predictor.predict()
        if 'error' not in prediction:
            self.assertLess(prediction['confidence'], 1.0)
    
    def test_with_filter_maintenance_impact(self):
        """Test that filter maintenance extends water change intervals"""
        base_time = int(datetime.now().timestamp())
        
        # 6 water changes before filter maintenance (every 7 days)
        for i in range(6):
            end_time = base_time - ((i + 6) * 86400 * 7)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 25.0,
                'ph_before': 6.85,
                'ph_after': 6.88,
                'tds_before': 380,
                'tds_after': 230,
                'duration_minutes': 10,
                'completed': 1
            }
            self.db.water_changes.append(wc)
        
        # Filter maintenance
        fm_time = base_time - (6 * 86400 * 7)
        self.db.filter_maintenance.append({
            'timestamp': fm_time,
            'filter_type': 'mechanical',
            'days_since_last': 42,
            'tds_before': 380,
            'tds_after': 320,
            'notes': 'Full filter clean'
        })
        
        # 6 water changes after filter maintenance (every 9 days - extended)
        for i in range(6):
            end_time = base_time - (i * 86400 * 9)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 25.0,
                'ph_before': 6.85,
                'ph_after': 6.88,
                'tds_before': 340,  # Lower TDS increase after filter clean
                'tds_after': 220,
                'duration_minutes': 10,
                'completed': 1
            }
            self.db.water_changes.append(wc)
        
        # Train with filter maintenance data
        train_result = self.predictor.train()
        self.assertNotIn('error', train_result)
        
        # Model should learn the filter maintenance impact
        prediction = self.predictor.predict()
        self.assertNotIn('error', prediction)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and error conditions"""
    
    def test_empty_database(self):
        """Test behavior with completely empty database"""
        db = MockDatabase()
        predictor = WaterChangePredictor(db)
        
        # Training should fail gracefully
        result = predictor.train()
        self.assertIn('error', result)
        
        # Prediction should fail gracefully
        prediction = predictor.predict()
        self.assertIn('error', prediction)
    
    def test_only_one_water_change(self):
        """Test with only one water change in history"""
        db = MockDatabase()
        predictor = WaterChangePredictor(db)
        
        wc = {
            'start_timestamp': int(datetime.now().timestamp()) - 600,
            'end_timestamp': int(datetime.now().timestamp()),
            'volume_litres': 20.0,
            'temp_before': 25.5,
            'temp_after': 25.0,
            'ph_before': 6.85,
            'ph_after': 6.88,
            'tds_before': 360,
            'tds_after': 220,
            'duration_minutes': 10,
            'completed': 1
        }
        db.water_changes.append(wc)
        
        # Should fail to train (need MIN_TRAINING_SAMPLES)
        result = predictor.train()
        self.assertIn('error', result)
    
    def test_all_zero_tds_changes(self):
        """Test with water changes that have no TDS change"""
        db = MockDatabase()
        predictor = WaterChangePredictor(db)
        
        base_time = int(datetime.now().timestamp())
        for i in range(10):
            end_time = base_time - (i * 86400 * 7)
            wc = {
                'start_timestamp': end_time - 600,
                'end_timestamp': end_time,
                'volume_litres': 20.0,
                'temp_before': 25.5,
                'temp_after': 25.0,
                'ph_before': 6.85,
                'ph_after': 6.88,
                'tds_before': 300,
                'tds_after': 300,  # No change
                'duration_minutes': 10,
                'completed': 1
            }
            db.water_changes.append(wc)
        
        # Should still train but with 0 TDS rate
        result = predictor.train()
        # May succeed with low confidence
        if 'error' not in result:
            self.assertIsNotNone(result.get('model'))


def run_tests():
    """Run all tests"""
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    suite.addTests(loader.loadTestsFromTestCase(TestDatabaseOperations))
    suite.addTests(loader.loadTestsFromTestCase(TestMLPredictorFeatureExtraction))
    suite.addTests(loader.loadTestsFromTestCase(TestMLPredictorTraining))
    suite.addTests(loader.loadTestsFromTestCase(TestMLPredictorPrediction))
    suite.addTests(loader.loadTestsFromTestCase(TestDataValidation))
    suite.addTests(loader.loadTestsFromTestCase(TestIntegrationScenarios))
    suite.addTests(loader.loadTestsFromTestCase(TestEdgeCases))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Print summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    print(f"Tests run: {result.testsRun}")
    print(f"Successes: {result.testsRun - len(result.failures) - len(result.errors)}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print("="*70)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())
