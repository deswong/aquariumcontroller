# Unified ML Service Testing Guide

Complete testing documentation for the Aquarium ML Service, including unit tests, integration tests, and deployment validation.

## Table of Contents
1. [Test Overview](#test-overview)
2. [Unit Tests](#unit-tests)
3. [Integration Tests](#integration-tests)
4. [Test Environment Setup](#test-environment-setup)
5. [Running Tests](#running-tests)
6. [CI/CD Integration](#cicd-integration)
7. [Performance Testing](#performance-testing)

---

## Test Overview

The ML service has two complementary test suites:

### Unit Tests (`test_aquarium_ml_service.py`)
- **480 lines** of mocked unit tests
- Tests individual components in isolation
- No external dependencies (database, MQTT)
- Fast execution (~5 seconds)
- Run on every code change

### Integration Tests (`test_integration.py`)
- **680 lines** of real-world integration tests
- Tests complete workflows with actual services
- Requires MariaDB and MQTT broker
- Slower execution (~30-60 seconds)
- Run before deployment

---

## Unit Tests

### Test Coverage

#### 1. TestAquariumDatabase
Tests database operations with mocked connections:
- `test_store_sensor_reading()` - Verify INSERT statements
- `test_store_pid_performance()` - Validate PID data storage
- `test_store_water_change()` - Check water change event storage

#### 2. TestPIDOptimizer
Tests PID optimization logic:
- `test_extract_features()` - Verify 7 features extracted correctly
- `test_train_season_insufficient_data()` - Handle missing data gracefully
- `test_predict_no_model()` - Return error when model not trained

#### 3. TestWaterChangePredictor
Tests water change prediction:
- `test_extract_features()` - Verify 6 features with TDS and ambient temp
- `test_train_insufficient_data()` - Handle insufficient history
- `test_predict_no_model()` - Return error when no model available

#### 4. TestMQTTIntegration
Tests MQTT messaging:
- `test_publish_pid_gains()` - Verify gains published correctly
- `test_publish_wc_prediction()` - Check prediction message format
- `test_on_message_sensor_data()` - Process incoming sensor data
- `test_on_message_pid_performance()` - Handle PID performance updates

#### 5. TestEndToEndIntegration
Tests complete workflow:
- `test_full_pid_workflow()` - Train with 60 samples → predict gains
- Validates realistic gain values (Kp > 0, Ki > 0, Kd > 0)

### Running Unit Tests

```bash
cd /home/des/Documents/aquariumcontroller/tools

# Run all unit tests
python3 test_aquarium_ml_service.py

# Run with pytest (more detailed)
pytest test_aquarium_ml_service.py -v

# Run specific test class
pytest test_aquarium_ml_service.py::TestPIDOptimizer -v

# Run with coverage report
pytest test_aquarium_ml_service.py --cov=aquarium_ml_service --cov-report=html
# Open htmlcov/index.html to view coverage

# Run in parallel (faster)
pytest test_aquarium_ml_service.py -n auto
```

### Expected Output

```
test_aquarium_ml_service.py::TestAquariumDatabase::test_store_sensor_reading PASSED
test_aquarium_ml_service.py::TestAquariumDatabase::test_store_pid_performance PASSED
test_aquarium_ml_service.py::TestAquariumDatabase::test_store_water_change PASSED
test_aquarium_ml_service.py::TestPIDOptimizer::test_extract_features PASSED
test_aquarium_ml_service.py::TestPIDOptimizer::test_train_season_insufficient_data PASSED
test_aquarium_ml_service.py::TestPIDOptimizer::test_predict_no_model PASSED
test_aquarium_ml_service.py::TestWaterChangePredictor::test_extract_features PASSED
test_aquarium_ml_service.py::TestWaterChangePredictor::test_train_insufficient_data PASSED
test_aquarium_ml_service.py::TestWaterChangePredictor::test_predict_no_model PASSED
test_aquarium_ml_service.py::TestMQTTIntegration::test_publish_pid_gains PASSED
test_aquarium_ml_service.py::TestMQTTIntegration::test_publish_wc_prediction PASSED
test_aquarium_ml_service.py::TestMQTTIntegration::test_on_message_sensor_data PASSED
test_aquarium_ml_service.py::TestMQTTIntegration::test_on_message_pid_performance PASSED
test_aquarium_ml_service.py::TestEndToEndIntegration::test_full_pid_workflow PASSED

============================== 14 passed in 4.82s ==============================
```

---

## Integration Tests

### Test Coverage

#### 1. TestDatabaseIntegration
Tests real database operations:
- `test_01_table_creation()` - Verify all 6 tables created
- `test_02_store_and_retrieve_sensor_data()` - Round-trip sensor data
- `test_03_store_pid_performance()` - Store and retrieve PID records
- `test_04_store_water_change()` - Store water change events

#### 2. TestPIDOptimizerIntegration
Tests PID optimizer with real database:
- `test_01_populate_training_data()` - Generate 100 realistic samples
- `test_02_train_seasonal_models()` - Train all 4 seasonal models
- `test_03_predict_gains()` - Generate predictions for different conditions

#### 3. TestWaterChangePredictorIntegration
Tests water change predictor with real database:
- `test_01_populate_water_change_history()` - Generate 20 water changes + 5 filter maintenance
- `test_02_train_predictor()` - Train all 3 models (linear, RF, GB)
- `test_03_predict_next_change()` - Predict next water change timing

#### 4. TestMQTTIntegration
Tests MQTT messaging with real broker:
- `test_01_publish_sensor_data()` - Publish and verify storage
- `test_02_publish_pid_gains()` - Publish gains via MQTT
- `test_03_publish_wc_prediction()` - Publish water change prediction

#### 5. TestEndToEndWorkflow
Tests complete system:
- `test_complete_workflow()` - Full cycle: populate data → train → predict → verify

### Running Integration Tests

```bash
cd /home/des/Documents/aquariumcontroller/tools

# Run all integration tests
python3 test_integration.py

# Run with pytest
pytest test_integration.py -v -s

# Run specific test class
pytest test_integration.py::TestPIDOptimizerIntegration -v -s

# Skip integration tests (when services unavailable)
SKIP_INTEGRATION=1 python3 test_integration.py
```

### Expected Output

```
======================================================================
INTEGRATION TESTS FOR UNIFIED AQUARIUM ML SERVICE
======================================================================

Test Configuration:
  Database: aquarium_test@localhost:3306/aquarium_test
  MQTT Broker: localhost:1883

✓ Connected to test database: aquarium_test

test_01_table_creation (test_integration.TestDatabaseIntegration)
Test that all tables are created ... ✓ All 6 tables created
ok

test_02_store_and_retrieve_sensor_data (test_integration.TestDatabaseIntegration)
Test storing and retrieving sensor readings ... ✓ Stored and retrieved sensor reading: temp=24.5°C
ok

test_03_store_pid_performance (test_integration.TestDatabaseIntegration)
Test storing PID performance data ... ✓ Stored PID performance: Kp=10.0, Ki=0.5, Kd=5.0
ok

...

======================================================================
INTEGRATION TEST SUMMARY
======================================================================
Tests run: 15
Successes: 15
Failures: 0
Errors: 0
Skipped: 0
```

---

## Test Environment Setup

### 1. Install Test Database

```bash
# Create test database
sudo mysql -e "CREATE DATABASE IF NOT EXISTS aquarium_test;"

# Create test user with limited permissions
sudo mysql -e "CREATE USER IF NOT EXISTS 'aquarium_test'@'localhost' IDENTIFIED BY 'test_password';"

# Grant permissions (test database only)
sudo mysql -e "GRANT ALL PRIVILEGES ON aquarium_test.* TO 'aquarium_test'@'localhost';"
sudo mysql -e "FLUSH PRIVILEGES;"
```

### 2. Install MQTT Broker

```bash
# Install Mosquitto
sudo apt-get update
sudo apt-get install mosquitto mosquitto-clients

# Start service
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

# Test broker
mosquitto_pub -t test/topic -m "hello"
mosquitto_sub -t test/topic
```

### 3. Install Python Dependencies

```bash
cd /home/des/Documents/aquariumcontroller/tools

# Install test dependencies
pip3 install pytest pytest-cov pytest-xdist pytest-timeout

# Install service dependencies (if not already installed)
pip3 install numpy scikit-learn mysql-connector-python paho-mqtt
```

### 4. Configure Test Environment (Optional)

Create `.env.test` file:

```bash
# Test database configuration
TEST_DB_HOST=localhost
TEST_DB_PORT=3306
TEST_DB_NAME=aquarium_test
TEST_DB_USER=aquarium_test
TEST_DB_PASSWORD=test_password

# Test MQTT configuration
TEST_MQTT_BROKER=localhost
TEST_MQTT_PORT=1883
TEST_MQTT_USER=
TEST_MQTT_PASSWORD=

# Test behavior
SKIP_INTEGRATION=0  # Set to 1 to skip integration tests
```

Load environment:
```bash
export $(cat .env.test | xargs)
python3 test_integration.py
```

---

## Running Tests

### Quick Test (Unit Tests Only)

```bash
# Fastest - run unit tests only
pytest tools/test_aquarium_ml_service.py -v
```

**Time:** ~5 seconds  
**Requirements:** None (fully mocked)

### Full Test (Unit + Integration)

```bash
# Run both test suites
pytest tools/test_aquarium_ml_service.py tools/test_integration.py -v -s
```

**Time:** ~60 seconds  
**Requirements:** MariaDB, MQTT broker

### Pre-Deployment Test

```bash
# Run with coverage and detailed output
pytest tools/test_aquarium_ml_service.py tools/test_integration.py \
  --cov=tools/aquarium_ml_service \
  --cov-report=html \
  --cov-report=term \
  -v -s

# View coverage report
firefox htmlcov/index.html
```

### Continuous Testing (Watch Mode)

```bash
# Install pytest-watch
pip3 install pytest-watch

# Run tests on file changes
ptw tools/ -- test_aquarium_ml_service.py -v
```

---

## CI/CD Integration

### GitHub Actions Workflow

Create `.github/workflows/ml-service-tests.yml`:

```yaml
name: ML Service Tests

on:
  push:
    branches: [ main ]
    paths:
      - 'tools/aquarium_ml_service.py'
      - 'tools/test_*.py'
  pull_request:
    branches: [ main ]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Install dependencies
        run: |
          python -m pip install --upgrade pip
          pip install pytest pytest-cov numpy scikit-learn mysql-connector-python paho-mqtt
      
      - name: Run unit tests
        run: |
          cd tools
          pytest test_aquarium_ml_service.py -v --cov=aquarium_ml_service --cov-report=xml
      
      - name: Upload coverage
        uses: codecov/codecov-action@v3
        with:
          files: ./tools/coverage.xml

  integration-tests:
    runs-on: ubuntu-latest
    services:
      mysql:
        image: mariadb:10.6
        env:
          MYSQL_ROOT_PASSWORD: root
          MYSQL_DATABASE: aquarium_test
          MYSQL_USER: aquarium_test
          MYSQL_PASSWORD: test_password
        ports:
          - 3306:3306
        options: --health-cmd="mysqladmin ping" --health-interval=10s --health-timeout=5s --health-retries=3
      
      mosquitto:
        image: eclipse-mosquitto:2
        ports:
          - 1883:1883

    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Install dependencies
        run: |
          python -m pip install --upgrade pip
          pip install pytest numpy scikit-learn mysql-connector-python paho-mqtt
      
      - name: Wait for services
        run: |
          sleep 10
      
      - name: Run integration tests
        env:
          TEST_DB_HOST: 127.0.0.1
          TEST_DB_PORT: 3306
          TEST_DB_NAME: aquarium_test
          TEST_DB_USER: aquarium_test
          TEST_DB_PASSWORD: test_password
          TEST_MQTT_BROKER: 127.0.0.1
          TEST_MQTT_PORT: 1883
        run: |
          cd tools
          pytest test_integration.py -v -s
```

### Jenkins Pipeline

Create `Jenkinsfile`:

```groovy
pipeline {
    agent any
    
    stages {
        stage('Setup') {
            steps {
                sh 'pip3 install -r tools/requirements.txt'
            }
        }
        
        stage('Unit Tests') {
            steps {
                sh 'cd tools && pytest test_aquarium_ml_service.py -v --junitxml=unit-results.xml'
            }
        }
        
        stage('Integration Tests') {
            when {
                branch 'main'
            }
            steps {
                sh 'cd tools && pytest test_integration.py -v -s --junitxml=integration-results.xml'
            }
        }
    }
    
    post {
        always {
            junit 'tools/*-results.xml'
        }
    }
}
```

---

## Performance Testing

### Load Testing

Test ML service performance under load:

```python
# tools/test_performance.py
import time
import concurrent.futures
from aquarium_ml_service import AquariumMLService

def benchmark_prediction():
    """Benchmark prediction performance"""
    service = AquariumMLService(...)
    
    # Train models first
    service.train_all_pid_models()
    
    # Benchmark predictions
    iterations = 100
    start = time.time()
    
    for i in range(iterations):
        sensor_data = {
            'temperature': 24.5,
            'ambient_temp': 22.0,
            'tds': 300.0,
            'ph': 7.2,
            'tank_volume': 200.0
        }
        service.pid_optimizers['temp'].predict(sensor_data, season=1)
    
    elapsed = time.time() - start
    print(f"Predictions: {iterations} in {elapsed:.2f}s")
    print(f"Average: {elapsed/iterations*1000:.2f}ms per prediction")
    print(f"Throughput: {iterations/elapsed:.1f} predictions/sec")

if __name__ == '__main__':
    benchmark_prediction()
```

Expected performance:
- **Prediction latency:** 5-10ms
- **Throughput:** 100-200 predictions/sec
- **Training time:** 
  - PID model (1 season): 2-5 seconds
  - Water change model: 1-2 seconds
  - All models: 10-20 seconds

### Memory Profiling

```bash
# Install memory profiler
pip3 install memory_profiler

# Profile memory usage
python3 -m memory_profiler tools/aquarium_ml_service.py --once

# Expected memory usage:
# - Baseline: ~50 MB
# - With models loaded: ~150 MB
# - Peak during training: ~250 MB
```

### Database Performance

Monitor query performance:

```sql
-- Enable slow query log
SET GLOBAL slow_query_log = 'ON';
SET GLOBAL long_query_time = 0.1;  -- 100ms threshold

-- Check slow queries
SELECT * FROM mysql.slow_log ORDER BY query_time DESC LIMIT 10;
```

Optimize common queries:

```sql
-- Add indexes for frequent queries
CREATE INDEX idx_timestamp ON sensor_readings(timestamp);
CREATE INDEX idx_controller_season ON pid_performance(controller, season);
CREATE INDEX idx_end_timestamp ON water_changes(end_timestamp);
```

---

## Test Maintenance

### Adding New Tests

When adding new features to the ML service:

1. **Add unit test** in `test_aquarium_ml_service.py`:
   - Create new test method in appropriate class
   - Mock external dependencies
   - Verify expected behavior

2. **Add integration test** in `test_integration.py`:
   - Test with real database/MQTT
   - Verify end-to-end workflow
   - Check data persistence

3. **Update this guide** with new test coverage

### Test Data Management

Clean up test database regularly:

```bash
# Reset test database
sudo mysql aquarium_test -e "DROP DATABASE aquarium_test;"
sudo mysql -e "CREATE DATABASE aquarium_test;"

# Or use test script cleanup
python3 -c "
from test_integration import TestDatabaseIntegration
import unittest
suite = unittest.TestLoader().loadTestsFromTestCase(TestDatabaseIntegration)
runner = unittest.TextTestRunner()
result = runner.run(suite)
TestDatabaseIntegration.tearDownClass()
"
```

---

## Troubleshooting

### Common Issues

#### 1. Database Connection Failed

```
Error: Cannot connect to test database
```

**Solution:**
```bash
# Check if MariaDB is running
sudo systemctl status mariadb

# Verify test user exists
sudo mysql -e "SELECT User, Host FROM mysql.user WHERE User='aquarium_test';"

# Test connection manually
mysql -u aquarium_test -p aquarium_test
```

#### 2. MQTT Connection Failed

```
Error: Cannot connect to MQTT broker
```

**Solution:**
```bash
# Check if Mosquitto is running
sudo systemctl status mosquitto

# Test broker manually
mosquitto_pub -t test -m "hello"

# Check firewall
sudo ufw status
sudo ufw allow 1883/tcp
```

#### 3. Import Errors

```
ModuleNotFoundError: No module named 'aquarium_ml_service'
```

**Solution:**
```bash
# Ensure you're in tools directory
cd /home/des/Documents/aquariumcontroller/tools

# Or add to PYTHONPATH
export PYTHONPATH=/home/des/Documents/aquariumcontroller/tools:$PYTHONPATH
```

#### 4. Insufficient Training Data

```
{'error': 'insufficient_data', 'samples': 5, 'required': 10}
```

**Solution:**
- Integration tests populate synthetic data
- For real deployment, wait for ESP32 to accumulate data
- Minimum requirements:
  - PID: 10 samples per season
  - Water change: 10 water change events

---

## Test Summary

| Test Suite | Tests | Time | Requirements | Coverage |
|------------|-------|------|--------------|----------|
| **Unit Tests** | 14 | ~5s | None | Database, PID, WC, MQTT |
| **Integration Tests** | 15 | ~60s | MariaDB, MQTT | End-to-end workflows |
| **Total** | 29 | ~65s | - | ~85% code coverage |

### Test Matrix

| Component | Unit Tests | Integration Tests | Coverage |
|-----------|------------|-------------------|----------|
| AquariumDatabase | ✓ (mocked) | ✓ (real DB) | 90% |
| PIDOptimizer | ✓ (mocked) | ✓ (real DB) | 85% |
| WaterChangePredictor | ✓ (mocked) | ✓ (real DB) | 85% |
| MQTTManager | ✓ (mocked) | ✓ (real MQTT) | 80% |
| AquariumMLService | ✓ (mocked) | ✓ (full workflow) | 75% |

---

## Next Steps

After testing:

1. **Deploy to production:**
   ```bash
   sudo cp aquarium_ml_service.py /opt/aquarium/
   sudo cp aquarium-ml.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl start aquarium-ml
   ```

2. **Monitor performance:**
   ```bash
   # Check logs
   sudo journalctl -u aquarium-ml -f
   
   # Check resource usage
   systemctl status aquarium-ml
   ```

3. **Validate predictions:**
   - Subscribe to MQTT topics: `aquarium/ml/#`
   - Check database for stored predictions
   - Compare predictions with actual ESP32 behavior

4. **Continuous improvement:**
   - Monitor prediction accuracy
   - Adjust model hyperparameters
   - Add new features based on sensor data
   - Retrain models as more data accumulates
