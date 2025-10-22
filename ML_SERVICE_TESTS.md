# Water Change ML Service - Test Suite

## Overview

The Water Change ML Service now has comprehensive test coverage with two complementary test suites:

1. **Unit Tests** (`test_ml_service.py`) - Fast, isolated tests with mocked dependencies
2. **Integration Tests** (`test_ml_integration.py`) - Real database operations with MariaDB

## Test Results Summary

### Unit Tests Status: ✅ ALL PASSING (23/23)

```
Test Classes:
✅ TestDatabaseOperations (5 tests) - Database storage/retrieval operations
✅ TestMLPredictorFeatureExtraction (3 tests) - 14-feature vector creation
✅ TestMLPredictorTraining (3 tests) - Model training and selection
✅ TestMLPredictorPrediction (3 tests) - Prediction generation
✅ TestDataValidation (3 tests) - Input validation and error handling
✅ TestIntegrationScenarios (3 tests) - Real-world usage patterns
✅ TestEdgeCases (3 tests) - Edge conditions and empty data

Total: 23 tests, 23 successes, 0 failures, 0 errors
Execution time: ~1 second
```

### Test Coverage

The test suite validates:
- ✅ Database operations (storing/retrieving sensor data, water changes, filter maintenance)
- ✅ Feature extraction (14 features including filter maintenance impact)
- ✅ ML model training (Linear Regression, Random Forest, Gradient Boosting)
- ✅ Model selection (automatic best model selection based on R² score)
- ✅ Prediction generation with confidence scoring
- ✅ Filter maintenance integration (days_since_filter, filter_in_period, tds_change_from_filter)
- ✅ Input validation (missing fields, negative values, extreme values)
- ✅ Edge cases (empty database, single water change, zero TDS changes)
- ✅ Real-world scenarios (regular/irregular schedules, filter impact)

## Running the Tests

### Prerequisites

1. **Install Python Dependencies**
   ```bash
   cd tools
   pip3 install -r requirements.txt
   ```

   This installs:
   - paho-mqtt >= 1.6.1
   - numpy >= 1.21.0
   - scikit-learn >= 1.0.0
   - mysql-connector-python >= 8.0.0

### Unit Tests (No Database Required)

Unit tests use mock databases and can run instantly without any setup:

```bash
cd tools
python3 test_ml_service.py
```

**Expected output:**
```
test_get_last_filter_maintenance ... ok
test_get_water_change_history ... ok
test_store_filter_maintenance ... ok
...
----------------------------------------------------------------------
Ran 23 tests in 1.070s

OK

======================================================================
TEST SUMMARY
======================================================================
Tests run: 23
Successes: 23
Failures: 0
Errors: 0
======================================================================
```

### Integration Tests (MariaDB Required)

Integration tests validate actual database operations, ML training, and predictions.

**Setup:**

1. **Create Test Database**
   ```bash
   mysql -u root -p
   ```
   ```sql
   CREATE DATABASE aquarium_test;
   GRANT ALL PRIVILEGES ON aquarium_test.* TO 'aquarium'@'localhost';
   FLUSH PRIVILEGES;
   EXIT;
   ```

2. **Configure Test Environment**
   ```bash
   cd tools
   cp .env.test.example .env.test
   nano .env.test
   ```

   Update with your MariaDB credentials:
   ```bash
   TEST_DB_HOST=localhost
   TEST_DB_PORT=3306
   TEST_DB_NAME=aquarium_test
   TEST_DB_USER=aquarium
   TEST_DB_PASSWORD=your_password_here
   ```

3. **Run Integration Tests**
   ```bash
   cd tools
   python3 test_ml_integration.py
   ```

   The script will:
   - Prompt for confirmation (type 'yes' to proceed)
   - Connect to the test database
   - Create tables
   - Insert test data (100 sensor readings, 12 water changes, 3 filter maintenance events)
   - Train ML models with real data
   - Generate predictions
   - Validate data integrity
   - Clean up test data

**Expected output:**
```
======================================================================
AQUARIUM ML SERVICE INTEGRATION TESTS
======================================================================

WARNING: This will create and delete data in the test database!
Database: aquarium_test on localhost

Are you sure you want to continue? (yes/no): yes

Running integration tests...

✅ test_1_database_connection
   Successfully connected to MariaDB

✅ test_2_table_creation
   All required tables exist

✅ test_3_store_sensor_data
   Stored 100 sensor readings

...

======================================================================
TEST SUMMARY
======================================================================
Total tests: 10
Passed: 10
Failed: 0
Success rate: 100.0%
======================================================================
```

## Test Details

### Unit Tests

**TestDatabaseOperations**
- `test_store_sensor_reading` - Validates sensor data storage
- `test_store_water_change` - Validates water change event storage
- `test_store_filter_maintenance` - Validates filter maintenance storage
- `test_get_water_change_history` - Tests history retrieval
- `test_get_last_filter_maintenance` - Tests filter maintenance retrieval

**TestMLPredictorFeatureExtraction**
- `test_feature_extraction_basic` - Validates 14-feature vector creation
- `test_feature_extraction_with_filter_maintenance` - Confirms filter features included
- `test_insufficient_data_for_features` - Handles empty data gracefully

**TestMLPredictorTraining**
- `test_train_with_sufficient_data` - Trains models with adequate samples
- `test_train_insufficient_data` - Handles insufficient samples
- `test_train_selects_best_model` - Verifies best model selection (highest R²)

**TestMLPredictorPrediction**
- `test_predict_after_training` - Generates predictions after training
- `test_predict_without_training` - Handles untrained model gracefully
- `test_predict_with_filter_maintenance` - Verifies filter impact on predictions

**TestDataValidation**
- `test_water_change_with_missing_fields` - Handles missing data
- `test_negative_values` - Validates against invalid negative values
- `test_extreme_values` - Handles extreme but valid values

**TestIntegrationScenarios**
- `test_weekly_water_changes_regular` - Regular 7-day water change schedule
- `test_irregular_water_changes` - Irregular schedules (5-14 day intervals)
- `test_with_filter_maintenance_impact` - Filter maintenance extending intervals

**TestEdgeCases**
- `test_empty_database` - Empty database (no data)
- `test_only_one_water_change` - Single water change (insufficient for training)
- `test_all_zero_tds_changes` - Zero TDS changes (edge case)

### Integration Tests

1. **Database Connection** - Verify MariaDB connectivity
2. **Table Creation** - Confirm schema exists (sensor_readings, water_changes, filter_maintenance, predictions)
3. **Store Sensor Data** - Insert 100 readings over 24 hours
4. **Store Water Changes** - Insert 12 weekly water changes
5. **Store Filter Maintenance** - Insert 3 monthly filter events
6. **Retrieve History** - Fetch water change records
7. **ML Training** - Train models with real data
8. **Generate Prediction** - Produce forecast after training
9. **Store Prediction** - Save prediction to database
10. **Data Consistency** - Validate integrity and relationships

## Continuous Integration

### Automated Testing

For CI/CD pipelines, use pytest:

```bash
cd tools
python3 -m pytest test_ml_service.py -v --tb=short
```

### Test in Docker

```bash
# Start MariaDB container
docker run -d --name aquarium-mariadb \
  -e MYSQL_ROOT_PASSWORD=root \
  -e MYSQL_DATABASE=aquarium_test \
  -e MYSQL_USER=aquarium \
  -e MYSQL_PASSWORD=testpass \
  -p 3306:3306 \
  mariadb:10.6

# Wait for startup
sleep 10

# Run integration tests
cd tools
python3 test_ml_integration.py
```

## Troubleshooting

### Import Errors

If you see `ModuleNotFoundError`:
```bash
cd tools
pip3 install -r requirements.txt
```

### Database Connection Errors

**Error:** `Can't connect to MySQL server on 'localhost'`
- Verify MariaDB is running: `sudo systemctl status mariadb`
- Check credentials in `.env.test`
- Ensure user has permissions: `GRANT ALL ON aquarium_test.* TO 'aquarium'@'localhost';`

**Error:** `Access denied for user 'aquarium'@'localhost'`
- Reset password: 
  ```sql
  SET PASSWORD FOR 'aquarium'@'localhost' = PASSWORD('new_password');
  FLUSH PRIVILEGES;
  ```

### Test Failures

**Unit tests fail:**
- Check Python version: `python3 --version` (requires 3.7+)
- Reinstall dependencies: `pip3 install -r requirements.txt --force-reinstall`
- Check for syntax errors: `python3 -m py_compile test_ml_service.py`

**Integration tests fail:**
- Verify test database exists: `mysql -u aquarium -p -e "SHOW DATABASES;"`
- Check table creation permissions
- Review logs for specific error messages
- Ensure no other process is using test database

## Next Steps

After successful testing:

1. **Deploy to Production**
   - Copy `water_change_ml_service.py` to production server
   - Configure `.env` with production MariaDB credentials
   - Set up systemd service or cron job

2. **Monitor Performance**
   - Check logs regularly
   - Monitor prediction accuracy
   - Review model selection (Linear vs Random Forest vs Gradient Boosting)

3. **Tune Models**
   - Adjust `MIN_TRAINING_SAMPLES` if needed (currently 5)
   - Add more features if patterns emerge
   - Retrain models as more data accumulates

## Files

- `tools/test_ml_service.py` - Unit test suite (23 tests)
- `tools/test_ml_integration.py` - Integration test suite (10 tests)
- `tools/.env.test.example` - Test environment configuration template
- `tools/water_change_ml_service.py` - ML service implementation
- `tools/train_and_predict.py` - Standalone training script
- `tools/requirements.txt` - Python dependencies

## Test Maintenance

- **Update tests** when adding new features
- **Run tests** before deploying changes
- **Review failures** to identify regressions
- **Monitor coverage** to ensure comprehensive testing

---

**Status:** ✅ All unit tests passing (23/23)  
**Last Updated:** 2025-10-22  
**Test Execution Time:** ~1 second (unit), ~5-10 seconds (integration)  
**Coverage:** Database operations, feature extraction, ML training, predictions, validation, edge cases
