#!/usr/bin/env python3
"""
Daily Water Change ML Training Script

Run this script daily via cron to retrain the ML model and publish predictions.
This is a lighter alternative to running the full service.

Usage:
    python3 train_and_predict.py

Cron example (run at 2 AM daily):
    0 2 * * * /usr/bin/python3 /path/to/train_and_predict.py >> /var/log/aquarium-ml.log 2>&1
"""

import json
import logging
import os
import sys
from datetime import datetime

# Add parent directory to path to import the main module
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from water_change_ml_service import (
    AquariumDatabase,
    WaterChangePredictor,
    MQTT_BROKER,
    MQTT_PORT,
    MQTT_USER,
    MQTT_PASSWORD,
    TOPIC_PREDICTION,
    DB_HOST,
    DB_PORT,
    DB_NAME,
    DB_USER,
    DB_PASSWORD
)

import paho.mqtt.client as mqtt

# Logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def publish_prediction(prediction: dict) -> bool:
    """Publish prediction to MQTT broker"""
    try:
        client = mqtt.Client()
        
        if MQTT_USER and MQTT_PASSWORD:
            client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        payload = json.dumps(prediction)
        result = client.publish(TOPIC_PREDICTION, payload, retain=True)
        
        client.disconnect()
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            logger.info(f"Prediction published successfully")
            return True
        else:
            logger.error(f"Failed to publish prediction, return code: {result.rc}")
            return False
    
    except Exception as e:
        logger.error(f"Error publishing to MQTT: {e}")
        return False


def main():
    """Main entry point"""
    logger.info("="*60)
    logger.info(f"Water Change ML Training - {datetime.now()}")
    logger.info("="*60)
    
    # Initialize MariaDB database
    db = AquariumDatabase(
        host=DB_HOST,
        port=DB_PORT,
        database=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD
    )
    logger.info(f"Database: MariaDB at {DB_HOST}:{DB_PORT}/{DB_NAME}")
    
    # Get data statistics
    history = db.get_water_change_history(limit=100)
    sensor_readings = db.get_recent_sensor_readings(hours=24)
    
    logger.info(f"Water change history: {len(history)} records")
    logger.info(f"Recent sensor readings: {len(sensor_readings)} records (last 24h)")
    
    if len(history) < 5:
        logger.warning("Insufficient water change history for training (need at least 5)")
        logger.info("Skipping training and prediction")
        return 1
    
    # Initialize predictor
    predictor = WaterChangePredictor(db)
    
    # Train model
    logger.info("Training ML model...")
    train_result = predictor.train()
    
    if 'error' in train_result:
        logger.error(f"Training failed: {train_result['error']}")
        return 1
    
    logger.info(f"Training complete:")
    logger.info(f"  - Best model: {train_result['model']}")
    logger.info(f"  - R² score: {train_result['score']:.3f}")
    logger.info(f"  - Training samples: {train_result['training_samples']}")
    logger.info(f"  - All scores: {train_result['all_scores']}")
    
    # Generate prediction
    logger.info("Generating prediction...")
    prediction = predictor.predict()
    
    if 'error' in prediction:
        logger.error(f"Prediction failed: {prediction['error']}")
        return 1
    
    logger.info(f"Prediction generated:")
    logger.info(f"  - Days remaining: {prediction['predicted_days_remaining']}")
    logger.info(f"  - Total cycle days: {prediction['predicted_total_cycle_days']}")
    logger.info(f"  - Days since last change: {prediction['days_since_last_change']}")
    logger.info(f"  - Confidence: {prediction['confidence']:.0%}")
    logger.info(f"  - Current TDS: {prediction['current_tds']}")
    logger.info(f"  - TDS increase rate: {prediction['tds_increase_rate']} ppm/day")
    logger.info(f"  - Needs change soon: {prediction['needs_change_soon']}")
    
    # Store prediction in database for accuracy tracking
    db.store_prediction(
        days=prediction['predicted_days_remaining'],
        confidence=prediction['confidence'],
        model_type=prediction['model']
    )
    
    # Publish to MQTT
    logger.info(f"Publishing to MQTT: {MQTT_BROKER}:{MQTT_PORT}")
    success = publish_prediction(prediction)
    
    # Close database
    db.close()
    
    if success:
        logger.info("✓ Training and prediction complete")
        logger.info("="*60)
        return 0
    else:
        logger.error("✗ Failed to publish prediction")
        logger.info("="*60)
        return 1


if __name__ == "__main__":
    sys.exit(main())
