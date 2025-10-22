#!/usr/bin/env python3
"""
Integration Test for Water Change ML Service with Real MariaDB

This test connects to an actual MariaDB database to verify:
1. Database connection and table creation
2. Data storage and retrieval
3. ML training with real data
4. Prediction generation

Prerequisites:
- MariaDB server running
- Database 'aquarium_test' created
- User with access permissions
- Set environment variables or use .env.test file

Usage:
    python3 test_ml_integration.py
"""

import sys
import os
from datetime import datetime, timedelta
import json

# Load environment variables
try:
    from dotenv import load_dotenv
    if os.path.exists('.env.test'):
        load_dotenv('.env.test')
    else:
        load_dotenv('.env')
except ImportError:
    print("Warning: python-dotenv not installed, using environment variables")

# Import the ML service
from water_change_ml_service import (
    AquariumDatabase,
    WaterChangePredictor
)


class IntegrationTest:
    """Integration test suite"""
    
    def __init__(self):
        self.db = None
        self.predictor = None
        self.test_results = []
        
        # Test database configuration (separate from production)
        self.db_config = {
            'host': os.getenv('TEST_DB_HOST', 'localhost'),
            'port': int(os.getenv('TEST_DB_PORT', '3306')),
            'database': os.getenv('TEST_DB_NAME', 'aquarium_test'),
            'user': os.getenv('TEST_DB_USER', 'aquarium'),
            'password': os.getenv('TEST_DB_PASSWORD', '')
        }
    
    def log(self, test_name, passed, message=""):
        """Log test result"""
        status = "✅ PASS" if passed else "❌ FAIL"
        self.test_results.append({
            'test': test_name,
            'passed': passed,
            'message': message
        })
        print(f"{status}: {test_name}")
        if message:
            print(f"  → {message}")
    
    def cleanup_tables(self):
        """Clean up test data"""
        if not self.db or not self.db.conn:
            return
        
        try:
            cursor = self.db.conn.cursor()
            cursor.execute("DELETE FROM predictions")
            cursor.execute("DELETE FROM filter_maintenance")
            cursor.execute("DELETE FROM sensor_readings")
            cursor.execute("DELETE FROM water_changes")
            self.db.conn.commit()
            print("🧹 Cleaned up test data")
        except Exception as e:
            print(f"⚠️  Cleanup warning: {e}")
    
    def test_1_database_connection(self):
        """Test 1: Database connection"""
        try:
            self.db = AquariumDatabase(**self.db_config)
            self.log("Database Connection", True, f"Connected to {self.db_config['host']}")
            return True
        except Exception as e:
            self.log("Database Connection", False, str(e))
            return False
    
    def test_2_table_creation(self):
        """Test 2: Verify tables were created"""
        try:
            cursor = self.db.conn.cursor()
            cursor.execute("SHOW TABLES")
            tables = [row[0] for row in cursor.fetchall()]
            
            required_tables = ['sensor_readings', 'water_changes', 'filter_maintenance', 'predictions']
            missing_tables = [t for t in required_tables if t not in tables]
            
            if missing_tables:
                self.log("Table Creation", False, f"Missing tables: {missing_tables}")
                return False
            
            self.log("Table Creation", True, f"All {len(required_tables)} tables exist")
            return True
        except Exception as e:
            self.log("Table Creation", False, str(e))
            return False
    
    def test_3_store_sensor_data(self):
        """Test 3: Store sensor readings"""
        try:
            # Store 100 sensor readings over 24 hours
            base_time = int(datetime.now().timestamp())
            
            for i in range(100):
                sensor_data = {
                    'timestamp': base_time - (i * 300),  # Every 5 minutes
                    'temperature': 25.5 + (i * 0.01),
                    'ambient_temp': 22.3,
                    'ph': 6.8 + (i * 0.001),
                    'tds': 300 + (i * 0.5),
                    'heater': 'ON' if i % 2 == 0 else 'OFF',
                    'co2': 'ON'
                }
                self.db.store_sensor_reading(sensor_data)
            
            # Verify count
            cursor = self.db.conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM sensor_readings")
            count = cursor.fetchone()[0]
            
            if count >= 100:
                self.log("Store Sensor Data", True, f"Stored {count} readings")
                return True
            else:
                self.log("Store Sensor Data", False, f"Only {count} readings stored")
                return False
        except Exception as e:
            self.log("Store Sensor Data", False, str(e))
            return False
    
    def test_4_store_water_changes(self):
        """Test 4: Store water change events"""
        try:
            base_time = int(datetime.now().timestamp())
            
            # Store 12 weekly water changes
            for week in range(12):
                end_time = base_time - (week * 86400 * 7)
                wc_data = {
                    'startTime': end_time - 600,
                    'endTime': end_time,
                    'volume': 20.0,
                    'tempBefore': 25.5,
                    'tempAfter': 24.8,
                    'phBefore': 6.85,
                    'phAfter': 6.90,
                    'tdsBefore': 350 + (week * 5),
                    'tdsAfter': 210 + (week * 3),
                    'duration': 10,
                    'successful': True
                }
                self.db.store_water_change(wc_data)
            
            # Verify count
            cursor = self.db.conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM water_changes")
            count = cursor.fetchone()[0]
            
            if count >= 12:
                self.log("Store Water Changes", True, f"Stored {count} water changes")
                return True
            else:
                self.log("Store Water Changes", False, f"Only {count} water changes stored")
                return False
        except Exception as e:
            self.log("Store Water Changes", False, str(e))
            return False
    
    def test_5_store_filter_maintenance(self):
        """Test 5: Store filter maintenance events"""
        try:
            base_time = int(datetime.now().timestamp())
            
            # Store 3 filter maintenance events
            for i in range(3):
                fm_data = {
                    'timestamp': base_time - (i * 86400 * 30),  # Monthly
                    'filter_type': 'mechanical',
                    'days_since_last': 30,
                    'tds_before': 350,
                    'tds_after': 320,
                    'notes': f'Filter maintenance #{i+1}'
                }
                self.db.store_filter_maintenance(fm_data)
            
            # Verify count
            cursor = self.db.conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM filter_maintenance")
            count = cursor.fetchone()[0]
            
            if count >= 3:
                self.log("Store Filter Maintenance", True, f"Stored {count} maintenance events")
                return True
            else:
                self.log("Store Filter Maintenance", False, f"Only {count} events stored")
                return False
        except Exception as e:
            self.log("Store Filter Maintenance", False, str(e))
            return False
    
    def test_6_retrieve_history(self):
        """Test 6: Retrieve water change history"""
        try:
            history = self.db.get_water_change_history(limit=10)
            
            if len(history) >= 10:
                self.log("Retrieve History", True, f"Retrieved {len(history)} records")
                return True
            else:
                self.log("Retrieve History", False, f"Only retrieved {len(history)} records")
                return False
        except Exception as e:
            self.log("Retrieve History", False, str(e))
            return False
    
    def test_7_ml_training(self):
        """Test 7: Train ML model"""
        try:
            self.predictor = WaterChangePredictor(self.db)
            result = self.predictor.train()
            
            if 'error' in result:
                self.log("ML Training", False, f"Training error: {result['error']}")
                return False
            
            if 'model' not in result or 'score' not in result:
                self.log("ML Training", False, "Missing model or score in result")
                return False
            
            model_name = result['model']
            score = result['score']
            samples = result.get('training_samples', 0)
            
            self.log("ML Training", True, 
                    f"Model: {model_name}, Score: {score:.3f}, Samples: {samples}")
            return True
        except Exception as e:
            self.log("ML Training", False, str(e))
            return False
    
    def test_8_generate_prediction(self):
        """Test 8: Generate prediction"""
        try:
            if not self.predictor or not self.predictor.best_model:
                self.log("Generate Prediction", False, "Model not trained")
                return False
            
            prediction = self.predictor.predict()
            
            if 'error' in prediction:
                self.log("Generate Prediction", False, f"Prediction error: {prediction['error']}")
                return False
            
            required_fields = ['predicted_days_remaining', 'confidence', 'model', 'tds_increase_rate']
            missing_fields = [f for f in required_fields if f not in prediction]
            
            if missing_fields:
                self.log("Generate Prediction", False, f"Missing fields: {missing_fields}")
                return False
            
            days = prediction['predicted_days_remaining']
            confidence = prediction['confidence']
            model = prediction['model']
            tds_rate = prediction['tds_increase_rate']
            
            self.log("Generate Prediction", True,
                    f"Days: {days:.1f}, Confidence: {confidence*100:.0f}%, "
                    f"TDS Rate: {tds_rate:.2f} ppm/day, Model: {model}")
            return True
        except Exception as e:
            self.log("Generate Prediction", False, str(e))
            return False
    
    def test_9_store_prediction(self):
        """Test 9: Store prediction in database"""
        try:
            self.db.store_prediction(6.5, 0.85, 'gradient_boost')
            
            # Verify
            cursor = self.db.conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM predictions")
            count = cursor.fetchone()[0]
            
            if count >= 1:
                self.log("Store Prediction", True, f"Stored prediction (total: {count})")
                return True
            else:
                self.log("Store Prediction", False, "Prediction not stored")
                return False
        except Exception as e:
            self.log("Store Prediction", False, str(e))
            return False
    
    def test_10_data_consistency(self):
        """Test 10: Verify data consistency"""
        try:
            # Get history and verify data integrity
            history = self.db.get_water_change_history(limit=5)
            
            issues = []
            for wc in history:
                # Check timestamps
                if wc.get('start_timestamp', 0) >= wc.get('end_timestamp', 0):
                    issues.append("Invalid timestamps")
                
                # Check TDS values
                if wc.get('tds_before', 0) < wc.get('tds_after', 0):
                    issues.append("TDS increased during water change (unexpected)")
                
                # Check volumes
                if wc.get('volume_litres', 0) <= 0:
                    issues.append("Invalid volume")
            
            if issues:
                self.log("Data Consistency", False, f"Issues: {', '.join(set(issues))}")
                return False
            
            self.log("Data Consistency", True, "All data checks passed")
            return True
        except Exception as e:
            self.log("Data Consistency", False, str(e))
            return False
    
    def run_all_tests(self):
        """Run all integration tests"""
        print("="*70)
        print("WATER CHANGE ML SERVICE - INTEGRATION TESTS")
        print("="*70)
        print(f"Database: {self.db_config['database']} @ {self.db_config['host']}")
        print("="*70)
        print()
        
        tests = [
            self.test_1_database_connection,
            self.test_2_table_creation,
            self.test_3_store_sensor_data,
            self.test_4_store_water_changes,
            self.test_5_store_filter_maintenance,
            self.test_6_retrieve_history,
            self.test_7_ml_training,
            self.test_8_generate_prediction,
            self.test_9_store_prediction,
            self.test_10_data_consistency
        ]
        
        passed = 0
        failed = 0
        
        for test_func in tests:
            try:
                if test_func():
                    passed += 1
                else:
                    failed += 1
            except Exception as e:
                print(f"❌ EXCEPTION in {test_func.__name__}: {e}")
                failed += 1
            print()
        
        # Cleanup
        print("="*70)
        self.cleanup_tables()
        
        if self.db:
            self.db.close()
        
        # Summary
        print("="*70)
        print("TEST SUMMARY")
        print("="*70)
        print(f"Total Tests: {passed + failed}")
        print(f"Passed: {passed} ✅")
        print(f"Failed: {failed} ❌")
        print(f"Success Rate: {(passed/(passed+failed)*100):.1f}%")
        print("="*70)
        
        return failed == 0


def main():
    """Main entry point"""
    # Check if test database is configured
    test_db = os.getenv('TEST_DB_NAME', 'aquarium_test')
    
    print()
    print("⚠️  WARNING: This test will create and delete data in the database!")
    print(f"Database: {test_db}")
    print()
    
    response = input("Continue with integration tests? (yes/no): ").strip().lower()
    if response != 'yes':
        print("Tests cancelled.")
        return 1
    
    print()
    
    # Run tests
    test_suite = IntegrationTest()
    success = test_suite.run_all_tests()
    
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
