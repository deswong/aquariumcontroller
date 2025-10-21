# ML-Enhanced PID Control - Implementation Roadmap

## Overview

This document outlines the three-phase implementation plan for machine learning-enhanced PID control in the aquarium controller. The goal is to enable the system to learn optimal PID parameters from historical performance data, adapting to environmental conditions, daily patterns, and seasonal changes.

**Current Status**: ✅ **Phase 1 Complete** (October 2025)

---

## Phase 1: Data Collection & Lookup Table (COMPLETED ✅)

### Implementation Summary

Phase 1 establishes the foundation for ML-based PID optimization through performance-based data logging and a lookup table system.

### Components Implemented

#### 1. MLDataLogger Class
- **Location**: `include/MLDataLogger.h`, `src/MLDataLogger.cpp`
- **Purpose**: Collect and manage PID performance data
- **Storage**: Binary format in `/ml/pid_samples.dat` (SPIFFS)
- **Capacity**: ~26,000 samples (4MB partition)

**Key Features**:
- **PIDPerformanceSample Structure** (18 fields):
  - Environmental context: currentValue, targetValue, ambientTemp, hourOfDay, season, tankVolume
  - PID parameters: kp, ki, kd
  - Performance metrics: errorMean, errorVariance, settlingTime, overshoot, steadyStateError, averageOutput, cycleCount
  - Computed score (0-100, higher = better)

- **Performance Scoring Algorithm**:
  ```
  score = 100 × (1 - weighted_penalties)
  
  where:
    settling_penalty = min(settlingTime / 600, 1.0) × 0.30
    overshoot_penalty = min(overshoot / 50, 1.0) × 0.30
    steady_state_penalty = min(abs(steadyStateError) / 5, 1.0) × 0.20
    variance_penalty = min(errorVariance / 2, 1.0) × 0.20
  ```

- **Lookup Table System**:
  - Feature buckets: 2°C temp, 3°C ambient, 6-hour time blocks, discrete seasons
  - Fast O(1) retrieval via bucketed map structure
  - Exponential moving average updates (better scores weighted 70%, worse 30%)
  - Minimum confidence threshold: 50 samples + score > 50
  - Persistence: `/ml/pid_lookup.dat` (serialized map)

#### 2. AdaptivePID ML Integration
- **Location**: `include/AdaptivePID.h`, `src/AdaptivePID.cpp`
- **Methods Added**:
  - `setMLLogger(MLDataLogger*)` - Attach ML logger
  - `computeWithContext(input, dt, ambient, hour, season, tankVolume)` - ML-aware compute
  - `enableMLAdaptation(bool)` - Toggle ML parameter adaptation
  - `isMLEnabled()` - Check ML status
  - `getMLConfidence()` - Get confidence for current conditions
  - `adaptParametersWithML(ambient, hour, season)` - Query lookup table and blend gains
  - `logPerformanceToML(ambient, hour, season, tankVolume)` - Create performance samples

**Adaptation Logic**:
- **High Confidence (>70%)**: Aggressive blending (70% ML, 30% current)
- **Medium Confidence (50-70%)**: Conservative blending (30% ML) only if performance poor
- **Low Confidence (<50%)**: No adaptation, continue learning

**Data Logging**:
- Windowed performance tracking (10-minute intervals)
- Automatic sample generation when settled
- Tracks: error sum, error squared sum, output sum, sample count
- Resets after each log to start fresh window

#### 3. Build Configuration
- **Partition Table**: `partitions_s3_16mb.csv`
  - ML Data Partition: 4MB FAT filesystem at 0x7B0000
  - SPIFFS: 4MB for web UI + event logs
- **Build Flags**: `ML_DATA_ENABLED` defined in `platformio.ini`
- **Storage**: Binary format for space efficiency (~150 bytes/sample)

### Usage Pattern

```cpp
// In SystemTasks setup:
MLDataLogger mlLogger;
mlLogger.begin();

tempPID.setMLLogger(&mlLogger);
tempPID.enableMLAdaptation(true);

// In control loop:
float ambient = ambientSensor.getTemperature();
uint8_t hour = timeClient.getHours();
uint8_t season = calculateSeason(month);
float tankVolume = config.getTankVolume();

float output = tempPID.computeWithContext(
    currentTemp, dt, ambient, hour, season, tankVolume
);
```

### Benefits Achieved

1. ✅ **Automatic Data Collection**: System learns from every control cycle
2. ✅ **Fast Parameter Retrieval**: O(1) lookup via bucketed feature space
3. ✅ **Persistent Learning**: Survives reboots via SPIFFS storage
4. ✅ **Confidence-Based Adaptation**: Only applies ML when confident
5. ✅ **CSV Export**: Ready for Phase 2 neural network training
6. ✅ **Non-Intrusive**: Existing `compute()` unchanged, backward compatible

### Limitations

- Simple lookup table (no interpolation between conditions)
- No predictive capabilities (reactive only)
- Limited to exact or bucketed condition matches
- No disturbance rejection learning

---

## Phase 2: Neural Network Training (PLANNED 📅)

### Objective

Train a neural network to predict optimal PID parameters based on environmental conditions and system dynamics. This enables interpolation between known conditions and better generalization.

### Architecture Design

#### 1. Model Structure

**Input Features** (10 features):
- Current temperature/pH (normalized)
- Target setpoint (normalized)
- Ambient temperature (normalized)
- Hour of day (sin/cos encoded for cyclical)
- Season (one-hot encoded, 4 values)
- Tank volume (normalized)
- Recent error trend (moving average of last 5 errors)
- Current PID gains (kp, ki, kd - normalized)

**Output** (3 values):
- Optimal Kp (denormalized)
- Optimal Ki (denormalized)
- Optimal Kd (denormalized)

**Network Layers**:
```
Input Layer: 10 neurons
Hidden Layer 1: 32 neurons (ReLU activation)
Hidden Layer 2: 16 neurons (ReLU activation)
Hidden Layer 3: 8 neurons (ReLU activation)
Output Layer: 3 neurons (Linear activation)
```

**Loss Function**: Mean Squared Error weighted by performance score
```python
weighted_mse = mean((y_pred - y_true)² × performance_score)
```

#### 2. Training Data Preparation

**Export from ESP32**:
```cpp
// Add to WebServer.cpp
server.on("/api/ml/export", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!mlLogger.exportAsCSV("/ml/export.csv")) {
        request->send(500, "text/plain", "Export failed");
        return;
    }
    request->send(SPIFFS, "/ml/export.csv", "text/csv", true);
});
```

**Python Training Script** (`tools/train_pid_model.py`):
```python
import pandas as pd
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler

# Load data from ESP32 CSV export
df = pd.read_csv('ml_export.csv')

# Filter high-performing samples (score > 70)
df = df[df['score'] > 70]

# Feature engineering
df['hour_sin'] = np.sin(2 * np.pi * df['hourOfDay'] / 24)
df['hour_cos'] = np.cos(2 * np.pi * df['hourOfDay'] / 24)
df['season_onehot'] = pd.get_dummies(df['season'], prefix='season')

# Prepare features and targets
X = df[['currentValue', 'targetValue', 'ambientTemp', 'hour_sin', 
        'hour_cos', 'tankVolume', 'errorMean', 'kp', 'ki', 'kd']]
X = pd.concat([X, df['season_onehot']], axis=1)

y = df[['kp', 'ki', 'kd']]
weights = df['score'] / 100.0  # Use score as sample weight

# Normalize
scaler_X = StandardScaler()
scaler_y = StandardScaler()
X_scaled = scaler_X.fit_transform(X)
y_scaled = scaler_y.fit_transform(y)

# Split data
X_train, X_test, y_train, y_test, w_train, w_test = train_test_split(
    X_scaled, y_scaled, weights, test_size=0.2, random_state=42
)

# Build model
model = tf.keras.Sequential([
    tf.keras.layers.Dense(32, activation='relu', input_shape=(X.shape[1],)),
    tf.keras.layers.Dropout(0.2),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dropout(0.2),
    tf.keras.layers.Dense(8, activation='relu'),
    tf.keras.layers.Dense(3, activation='linear')
])

# Compile with weighted loss
model.compile(optimizer='adam', loss='mse', metrics=['mae'])

# Train
history = model.fit(
    X_train, y_train,
    sample_weight=w_train,
    validation_data=(X_test, y_test, w_test),
    epochs=100,
    batch_size=32,
    callbacks=[
        tf.keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True),
        tf.keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=5)
    ]
)

# Save model and scalers
model.save('pid_model.h5')
np.save('scaler_X_mean.npy', scaler_X.mean_)
np.save('scaler_X_scale.npy', scaler_X.scale_)
np.save('scaler_y_mean.npy', scaler_y.mean_)
np.save('scaler_y_scale.npy', scaler_y.scale_)
```

#### 3. Model Conversion for ESP32

**TensorFlow Lite Conversion**:
```python
# Convert to TFLite with quantization
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

# Representative dataset for quantization
def representative_dataset():
    for i in range(100):
        yield [X_train[i:i+1].astype(np.float32)]

converter.representative_dataset = representative_dataset
tflite_model = converter.convert()

# Save as header file for embedding
with open('ml_model.h', 'w') as f:
    f.write('#ifndef ML_MODEL_H\n#define ML_MODEL_H\n\n')
    f.write(f'const unsigned int ml_model_len = {len(tflite_model)};\n')
    f.write('const unsigned char ml_model[] = {\n')
    for i, byte in enumerate(tflite_model):
        if i % 12 == 0:
            f.write('  ')
        f.write(f'0x{byte:02x}')
        if i < len(tflite_model) - 1:
            f.write(',')
        if i % 12 == 11:
            f.write('\n')
    f.write('\n};\n\n#endif\n')
```

#### 4. ESP32 Inference Implementation

**New Files to Create**:

**`include/MLInference.h`**:
```cpp
#ifndef ML_INFERENCE_H
#define ML_INFERENCE_H

#include <Arduino.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

class MLInference {
private:
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    TfLiteTensor* input;
    TfLiteTensor* output;
    
    uint8_t tensor_arena[50 * 1024]; // 50KB tensor arena
    
    // Normalization parameters (loaded from training)
    float input_mean[10];
    float input_scale[10];
    float output_mean[3];
    float output_scale[3];
    
    void normalize(float* features, int count);
    void denormalize(float* outputs, int count);
    
public:
    MLInference();
    ~MLInference();
    
    bool begin();
    bool predict(float currentValue, float targetValue, float ambientTemp,
                 uint8_t hour, uint8_t season, float tankVolume,
                 float errorTrend, float current_kp, float current_ki, float current_kd,
                 float& pred_kp, float& pred_ki, float& pred_kd);
    
    bool isModelLoaded() { return model != nullptr; }
};

#endif
```

**`src/MLInference.cpp`**:
```cpp
#include "MLInference.h"
#include "ml_model.h" // Generated from Python script
#include <cmath>

MLInference::MLInference() 
    : model(nullptr), interpreter(nullptr), input(nullptr), output(nullptr) {
    // Initialize normalization params (copy from training)
    // These would be loaded from SPIFFS or embedded
}

bool MLInference::begin() {
    // Load model from embedded data
    model = tflite::GetModel(ml_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("Model schema version %d not supported\n", model->version());
        return false;
    }
    
    // Create resolver with required ops
    static tflite::MicroMutableOpResolver<5> resolver;
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddReshape();
    
    // Create interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena)
    );
    interpreter = &static_interpreter;
    
    // Allocate tensors
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("Failed to allocate tensors");
        return false;
    }
    
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    Serial.println("ML model loaded successfully");
    return true;
}

bool MLInference::predict(float currentValue, float targetValue, float ambientTemp,
                          uint8_t hour, uint8_t season, float tankVolume,
                          float errorTrend, float current_kp, float current_ki, float current_kd,
                          float& pred_kp, float& pred_ki, float& pred_kd) {
    if (!model) return false;
    
    // Prepare features
    float features[10];
    features[0] = currentValue;
    features[1] = targetValue;
    features[2] = ambientTemp;
    features[3] = sin(2.0 * PI * hour / 24.0); // Hour sin
    features[4] = cos(2.0 * PI * hour / 24.0); // Hour cos
    features[5] = tankVolume;
    features[6] = errorTrend;
    features[7] = current_kp;
    features[8] = current_ki;
    features[9] = current_kd;
    
    // Normalize
    normalize(features, 10);
    
    // Copy to input tensor
    for (int i = 0; i < 10; i++) {
        input->data.f[i] = features[i];
    }
    
    // Run inference
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Inference failed");
        return false;
    }
    
    // Extract outputs
    float outputs[3];
    outputs[0] = output->data.f[0];
    outputs[1] = output->data.f[1];
    outputs[2] = output->data.f[2];
    
    // Denormalize
    denormalize(outputs, 3);
    
    pred_kp = outputs[0];
    pred_ki = outputs[1];
    pred_kd = outputs[2];
    
    return true;
}

void MLInference::normalize(float* features, int count) {
    for (int i = 0; i < count; i++) {
        features[i] = (features[i] - input_mean[i]) / input_scale[i];
    }
}

void MLInference::denormalize(float* outputs, int count) {
    for (int i = 0; i < count; i++) {
        outputs[i] = outputs[i] * output_scale[i] + output_mean[i];
    }
}
```

#### 5. Integration with AdaptivePID

**Update `include/AdaptivePID.h`**:
```cpp
// Add forward declaration
class MLInference;

// Add to private members:
    MLInference* mlInference;
    bool useNeuralNetwork;
    float errorTrend; // Moving average of recent errors

// Add to public methods:
    void setMLInference(MLInference* inference);
    void enableNeuralNetwork(bool enable);
    bool isNeuralNetworkEnabled() { return useNeuralNetwork && mlInference != nullptr; }
```

**Update `src/AdaptivePID.cpp`**:
```cpp
void AdaptivePID::adaptParametersWithML(float ambientTemp, uint8_t hourOfDay, uint8_t season) {
    if (!mlLogger || !mlEnabled) return;
    
    float kp_ml, ki_ml, kd_ml;
    
    // Try neural network first (Phase 2)
    if (useNeuralNetwork && mlInference) {
        // Calculate error trend
        float recentErrorSum = 0;
        for (int i = 0; i < 5; i++) {
            int idx = (errorIndex - i - 1 + 100) % 100;
            recentErrorSum += errorHistory[idx];
        }
        errorTrend = recentErrorSum / 5.0f;
        
        float pred_kp, pred_ki, pred_kd;
        if (mlInference->predict(lastInput, target, ambientTemp, hourOfDay, season,
                                 0, errorTrend, kp, ki, kd,
                                 pred_kp, pred_ki, pred_kd)) {
            // Neural network prediction successful
            kp_ml = pred_kp;
            ki_ml = pred_ki;
            kd_ml = pred_kd;
            
            // Blend with current gains (50/50 for safety)
            kp = kp * 0.5f + kp_ml * 0.5f;
            ki = ki * 0.5f + ki_ml * 0.5f;
            kd = kd * 0.5f + kd_ml * 0.5f;
            
            // Constrain to safety bounds
            kp = constrain(kp, 0.1, 20.0);
            ki = constrain(ki, 0.01, 5.0);
            kd = constrain(kd, 0.01, 10.0);
            
            mlConfidence = 0.9f; // High confidence from NN
            Serial.printf("PID '%s': Neural network adaptation applied\n", namespace_name.c_str());
            return;
        }
    }
    
    // Fallback to lookup table (Phase 1)
    float confidence;
    if (mlLogger->getOptimalGains(lastInput, ambientTemp, hourOfDay, season, 
                                  kp_ml, ki_ml, kd_ml, confidence)) {
        if (confidence > 0.7f) {
            // ... existing lookup table logic ...
        }
    }
}
```

### Requirements

**Hardware**:
- ESP32-S3 with AI acceleration support
- Minimum 4MB flash for model storage
- PSRAM recommended for larger models

**Software**:
- TensorFlow Lite for Microcontrollers library
- Python 3.8+ with TensorFlow 2.x for training
- Training dataset: minimum 1,000 high-performing samples (score > 70)

**Dependencies** (`platformio.ini`):
```ini
lib_deps =
    ...existing...
    https://github.com/tensorflow/tflite-micro-arduino-examples
```

### Expected Benefits

1. **Interpolation**: Predictions for unseen conditions between trained samples
2. **Generalization**: Better handling of novel environmental combinations
3. **Smoother Adaptation**: Continuous predictions vs discrete lookup buckets
4. **Reduced Storage**: Model (~50KB) vs full sample database (4MB)
5. **Faster Inference**: Single forward pass vs table search

### Risks & Mitigations

**Risk 1**: Model overfitting on limited data
- **Mitigation**: Dropout layers, early stopping, minimum 1,000 samples, cross-validation

**Risk 2**: Poor real-world performance
- **Mitigation**: Confidence thresholding, fallback to Phase 1 lookup table, gradual blending

**Risk 3**: Memory constraints on ESP32
- **Mitigation**: Quantized INT8 model, optimized tensor arena size, PSRAM utilization

**Risk 4**: Training data bias
- **Mitigation**: Balanced sampling across seasons/times, weighted loss by score

### Testing Strategy

1. **Offline Validation**: Test model on held-out test set (20% of data)
2. **Shadow Mode**: Run model inference alongside Phase 1 for 1 week, log predictions but don't apply
3. **A/B Testing**: Enable NN on one controller (temp), keep other on lookup table (pH), compare performance
4. **Gradual Rollout**: Start with 50% blending, increase to 80% if performance improves

---

## Phase 3: Predictive Control & Disturbance Rejection (FUTURE 🔮)

### Objective

Implement advanced predictive control that anticipates disturbances and proactively adjusts parameters before errors occur.

### Architectural Concepts

#### 1. LSTM-Based Predictor

**Purpose**: Predict future temperature/pH trajectory based on current state and planned actions.

**Model Architecture**:
```
Input Sequence: [t-10, t-9, ..., t-1, t] (10 timesteps)
  Each timestep: [value, ambient, output, hour, season]
  
LSTM Layer 1: 64 units
LSTM Layer 2: 32 units
Dense Layer: 16 units (ReLU)
Output: [predicted_value_t+1, predicted_value_t+5, predicted_value_t+10]
```

**Training Data**:
- Time series of actual measurements
- Multi-step ahead predictions
- Loss function: MSE on prediction horizon

**Use Case**: Predict if current PID output will cause overshoot 5-10 minutes in future.

#### 2. Disturbance Detection & Classification

**Common Disturbances**:
1. **Feeding Event**: Sudden organic load increase → pH drop
2. **Water Change**: Large temperature shift → thermal shock
3. **Light Cycle**: CO2 changes → pH rise (photosynthesis)
4. **Equipment Failure**: Heater/chiller malfunction
5. **Door Opening**: Room temperature change → heat loss

**Detection Methods**:

**A. Pattern Recognition**:
```cpp
class DisturbanceDetector {
private:
    enum DisturbanceType {
        NONE,
        FEEDING,
        WATER_CHANGE,
        LIGHT_CYCLE,
        EQUIPMENT_FAILURE,
        THERMAL_SHOCK
    };
    
    struct DisturbancePattern {
        float errorSpikeThreshold;
        float errorRateChange;
        float ambientCorrelation;
        uint32_t typicalDuration;
    };
    
    std::map<DisturbanceType, DisturbancePattern> patterns;
    
public:
    DisturbanceType detectDisturbance(
        float* errorHistory, 
        float* ambientHistory,
        float* outputHistory,
        int historyLength
    );
};
```

**B. Anomaly Detection**:
- Track normal operating patterns per hour/day
- Flag deviations > 3 standard deviations
- Use LSTM autoencoder to learn normal behavior

**C. External Event Integration**:
```cpp
// Manual disturbance signaling
void AdaptivePID::signalDisturbance(DisturbanceType type, uint32_t expectedDuration) {
    disturbanceActive = true;
    disturbanceType = type;
    disturbanceStartTime = millis();
    disturbanceDuration = expectedDuration;
    
    // Load disturbance-specific PID profile
    applyDisturbanceProfile(type);
}

// Example profiles
void applyDisturbanceProfile(DisturbanceType type) {
    switch(type) {
        case FEEDING:
            // Increase Ki to handle sustained pH drop
            tempKi = ki;
            ki *= 1.5;
            break;
            
        case WATER_CHANGE:
            // Reduce Kp to prevent overshoot during large change
            tempKp = kp;
            kp *= 0.5;
            // Disable integral temporarily
            integral = 0;
            break;
            
        case THERMAL_SHOCK:
            // Increase Kd to dampen oscillations
            tempKd = kd;
            kd *= 2.0;
            break;
    }
}
```

#### 3. Model Predictive Control (MPC)

**Concept**: Optimize control actions over a prediction horizon to minimize future error.

**Optimization Problem**:
```
Minimize: Σ(t=0 to N) [ Q*(predicted_value[t] - target)² + R*control_effort[t]² ]

Subject to:
  - predicted_value[t+1] = f(predicted_value[t], control[t], disturbances[t])
  - control_min ≤ control[t] ≤ control_max
  - |control[t] - control[t-1]| ≤ max_rate_change

Where:
  N = prediction horizon (e.g., 10 minutes)
  Q = tracking error weight
  R = control effort weight (energy cost)
  f() = system dynamics model (from LSTM predictor)
```

**Implementation Strategy**:
```cpp
class MPCController {
private:
    MLInference* predictor;  // LSTM model
    int horizonSteps;        // N
    float Q, R;              // Weights
    
    struct ControlSequence {
        float outputs[10];   // Planned control actions
        float predictions[10]; // Predicted values
        float cost;          // Total cost
    };
    
    float optimizeControlSequence(
        float currentValue,
        float target,
        float ambientTemp,
        DisturbanceType disturbance
    );
    
    float evaluateCost(ControlSequence& seq);
    
public:
    float computeMPC(float input, float dt, float ambient, DisturbanceType disturbance);
};

float MPCController::computeMPC(float input, float dt, float ambient, DisturbanceType disturbance) {
    // Use gradient descent or genetic algorithm to find optimal control sequence
    ControlSequence bestSeq;
    float bestCost = INFINITY;
    
    // Initialize with current PID output as baseline
    float baseline = adaptivePID.compute(input, dt);
    
    // Optimize over 50 iterations
    for (int iter = 0; iter < 50; iter++) {
        ControlSequence candidate;
        
        // Generate candidate sequence (random perturbation around baseline)
        for (int i = 0; i < horizonSteps; i++) {
            candidate.outputs[i] = baseline + random(-5, 5);
        }
        
        // Predict future trajectory using LSTM
        float predicted = input;
        for (int i = 0; i < horizonSteps; i++) {
            predictor->predict(predicted, target, ambient, ..., predicted);
            candidate.predictions[i] = predicted;
        }
        
        // Evaluate cost
        candidate.cost = evaluateCost(candidate);
        
        if (candidate.cost < bestCost) {
            bestCost = candidate.cost;
            bestSeq = candidate;
        }
    }
    
    // Apply first action of optimal sequence (receding horizon)
    return bestSeq.outputs[0];
}
```

#### 4. Reinforcement Learning (Advanced)

**Concept**: Learn optimal control policy through trial-and-error interaction with environment.

**Agent-Environment Loop**:
```
State: [current_value, error, error_rate, ambient, hour, season, disturbance_type]
Action: [kp_adjustment, ki_adjustment, kd_adjustment]
Reward: -1 * (|error| + 0.1*|output_change| + 10*overshoot_penalty)
```

**Algorithm**: Soft Actor-Critic (SAC) or Proximal Policy Optimization (PPO)

**Training Strategy**:
1. **Simulation Environment**: Build thermal/chemical model of aquarium
2. **Offline Learning**: Train on historical data + simulation
3. **Online Fine-tuning**: Gradual updates during real operation (safe exploration)
4. **Reward Shaping**: Heavily penalize dangerous actions (overshoot, oscillation)

**Safety Constraints**:
- Maximum gain change per step: ±5%
- Fallback to PID if Q-value confidence < threshold
- Human override always available
- Emergency stop on safety violations

#### 5. Multi-Objective Optimization

**Competing Objectives**:
1. **Performance**: Minimize tracking error
2. **Energy**: Minimize heater/chiller runtime
3. **Stability**: Minimize oscillations
4. **Longevity**: Reduce equipment wear

**Pareto Optimization**:
```cpp
struct MultiObjectiveReward {
    float performance;  // -|error|
    float energy;       // -output²
    float stability;    // -variance
    float longevity;    // -|output_change|
    
    float weighted_sum(float w_perf, float w_energy, float w_stable, float w_longevity) {
        return w_perf * performance + w_energy * energy + 
               w_stable * stability + w_longevity * longevity;
    }
};

// User-configurable priorities
enum ControlMode {
    PERFORMANCE_PRIORITY,  // Racing: w_perf=0.7, others=0.1
    ENERGY_EFFICIENT,      // Eco mode: w_energy=0.7, w_perf=0.2, others=0.05
    BALANCED,              // All equal: 0.25 each
    EQUIPMENT_PROTECTION   // Longevity: w_longevity=0.6, w_stable=0.3, others=0.05
};
```

### Implementation Roadmap

**Stage 1: Predictive Modeling** (2-3 months)
- [ ] Implement LSTM-based value predictor
- [ ] Train on historical data
- [ ] Validate prediction accuracy (RMSE < 0.5°C)
- [ ] Integrate with AdaptivePID for lookahead

**Stage 2: Disturbance Handling** (1-2 months)
- [ ] Build disturbance detector
- [ ] Create disturbance profiles database
- [ ] Implement automatic profile switching
- [ ] Add manual disturbance signaling to web UI

**Stage 3: MPC Integration** (3-4 months)
- [ ] Implement MPC optimizer
- [ ] Tune Q, R weights
- [ ] A/B test vs Phase 2 neural network
- [ ] Measure energy savings

**Stage 4: RL Experimentation** (6+ months)
- [ ] Build simulation environment
- [ ] Train RL agent offline
- [ ] Shadow mode testing (3 months)
- [ ] Gradual online deployment

### Hardware Requirements (Phase 3)

- **ESP32-S3 with PSRAM** (8MB minimum)
- **Faster CPU** or **External AI Accelerator** (optional)
- **Additional Sensors**:
  - Current sensor (monitor heater/chiller power)
  - Occupancy sensor (detect feeding, maintenance)
  - Light sensor (detect day/night cycles)

### Data Requirements

- **Minimum 6 months** of continuous operation data
- **Diverse conditions**: All seasons, various disturbances
- **High-quality labels**: Manually annotated disturbance events
- **Simulation data**: Augment real data with synthetic scenarios

### Expected Benefits

1. **Proactive Control**: Prevent errors before they occur (30% error reduction)
2. **Energy Savings**: Optimize for efficiency (20-40% energy reduction)
3. **Disturbance Rejection**: Robust handling of feeding, water changes (50% faster recovery)
4. **Equipment Protection**: Reduce cycling, extend lifespan (2x longevity)
5. **Multi-Tank Learning**: Transfer knowledge between aquariums

### Risks & Challenges

**Technical**:
- Computational complexity (MPC optimization expensive)
- Model accuracy (LSTM predictions may drift)
- Training data quality (need diverse scenarios)
- Overfitting to specific tank conditions

**Operational**:
- User trust in autonomous system
- Debugging complex ML pipeline
- Over-optimization for metrics vs real fish health
- Catastrophic forgetting in online RL

**Safety**:
- Exploration during RL training risks fish health
- Model failures could cause harm
- Need robust fallback mechanisms
- Regulatory concerns for commercial aquariums

### Success Metrics

- **Tracking Performance**: Mean absolute error < 0.2°C / 0.05 pH
- **Settling Time**: < 5 minutes for 2°C step change
- **Overshoot**: < 5% of step size
- **Energy Efficiency**: 30% reduction in heater runtime
- **Disturbance Recovery**: < 10 minutes to return to setpoint
- **Robustness**: 99.9% uptime without emergency stops

---

## Getting Started

### Current Status (Phase 1)

The system is **production-ready** for data collection and lookup-based optimization:

1. **Enable ML**: `tempPID.enableMLAdaptation(true)`
2. **Collect Data**: System logs performance every 10 minutes
3. **Monitor Progress**: Check `/api/ml/stats` for sample count
4. **Wait for Learning**: Minimum 50 samples per condition bucket
5. **Observe Adaptation**: ML recommendations appear after ~1 week

### Preparing for Phase 2

To prepare for neural network training:

1. **Collect Diverse Data**: 
   - Run system through all seasons
   - Simulate disturbances (feeding, water changes)
   - Cover full temperature/pH operating range

2. **Export Data**: 
   - Use `/api/ml/export` to download CSV
   - Aim for 1,000+ samples with score > 70

3. **Set Up Training Environment**:
   ```bash
   pip install tensorflow pandas scikit-learn matplotlib
   git clone https://github.com/tensorflow/tflite-micro
   ```

4. **Monitor Performance**:
   - Track lookup table confidence levels
   - Identify conditions with low confidence (gaps in data)
   - Focus data collection on weak areas

### Contributing

If you implement Phase 2 or Phase 3, please:
- Document your training process
- Share model architectures and hyperparameters
- Contribute training scripts to `tools/ml/`
- Update this document with lessons learned

---

## References

### Academic Papers

1. **PID Control**:
   - Åström, K. J., & Hägglund, T. (1995). *PID Controllers: Theory, Design, and Tuning*
   - Ziegler, J. G., & Nichols, N. B. (1942). *Optimum Settings for Automatic Controllers*

2. **Machine Learning for Control**:
   - Recht, B. (2019). *A Tour of Reinforcement Learning: The View from Continuous Control*
   - Levine, S., et al. (2016). *End-to-End Training of Deep Visuomotor Policies*

3. **Model Predictive Control**:
   - Camacho, E. F., & Alba, C. B. (2013). *Model Predictive Control*
   - Schwenzer, M., et al. (2021). *Review on Model Predictive Control: An Engineering Perspective*

4. **Neural Network Controllers**:
   - Barto, A. G., et al. (1983). *Neuronlike Adaptive Elements That Can Solve Difficult Learning Control Problems*
   - Lillicrap, T. P., et al. (2015). *Continuous Control with Deep Reinforcement Learning*

### Open Source Projects

- [TensorFlow Lite Micro](https://github.com/tensorflow/tflite-micro)
- [ESP-NN](https://github.com/espressif/esp-nn) - Optimized neural network library for ESP32
- [uTensor](https://github.com/uTensor/uTensor) - AI inference library for microcontrollers
- [Stable Baselines3](https://github.com/DLR-RM/stable-baselines3) - Reinforcement learning algorithms

### Online Resources

- [TinyML Book](https://www.oreilly.com/library/view/tinyml/9781492052036/)
- [Deep Learning for Coders](https://course.fast.ai/)
- [Control Bootcamp (Steve Brunton)](https://www.youtube.com/playlist?list=PLMrJAkhIeNNR20Mz-VpzgfQs5zrYi085m)
- [Reinforcement Learning Course (David Silver)](https://www.davidsilver.uk/teaching/)

---

## Revision History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | Oct 2025 | Initial roadmap, Phase 1 complete | System |
| | | Phase 2 & 3 specifications | |

---

**Questions or suggestions?** Open an issue on GitHub or contact the development team.
