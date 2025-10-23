#include "AdaptivePID.h"
#include "MLDataLogger.h"

AdaptivePID::AdaptivePID(String namespaceName, float initialKp, float initialKi, float initialKd)
    : kp(initialKp), ki(initialKi), kd(initialKd), target(0), 
      outputMin(0), outputMax(100), integral(0), lastError(0), lastOutput(0), lastInput(0),
      lastTime(0), absoluteMax(100), safetyThreshold(10), emergencyStop(false),
      emergencyStopTime(0),
      useDerivativeFilter(true), derivativeFilterCoeff(0.7), filteredDerivative(0),
      useSetpointRamping(false), rampRate(1.0), effectiveTarget(0),
      useIntegralWindupPrevention(true), integralMax(100.0),
      useFeedForward(false), feedForwardGain(0.5),
      autoTuning(false), relayAmplitude(50), tuneStartTime(0),
      oscillationAmplitude(0), oscillationPeriod(0), oscillationCount(0),
      lastPeakValue(0), lastPeakTime(0), peakDetected(false),
      errorIndex(0), performanceMetric(0), useMLAdaptation(false),
      performanceWindowStart(0), performanceWindowErrorSum(0), performanceWindowErrorSqSum(0), 
      performanceWindowSamples(0), performanceWindowOutputSum(0),
      settlingStartTime(0), isSettled(false), maxOvershoot(0), 
      steadyStateError(0), settlingTime(0), controlActions(0),
      namespace_name(namespaceName),
      mlLogger(nullptr), mlEnabled(false), mlConfidence(0.0f),
      // PHASE 1: Initialize new members
      historySize(PID_HISTORY_SIZE),
      mlCacheValidityMs(300000),  // 5 minutes default
      mlCacheHits(0), mlCacheMisses(0),
      controlTimer(nullptr), useHardwareTimer(false), controlPeriodUs(0), computeReady(false),
      enableProfiling(false),
      // PHASE 2: Initialize dual-core and Kalman
      mlTaskHandle(nullptr), paramMutex(nullptr), mlWakeupSemaphore(nullptr),
      useDualCore(false), mlTaskRunning(false), useKalman(false),
      // PHASE 3: Initialize health monitoring and feed-forward
      useHealthMonitoring(false) {
    
    prefs = new Preferences();
    
    // PHASE 1: Allocate history buffers (PSRAM if available)
    allocateHistoryBuffers();
    
    // Initialize ML cache
    mlCache.valid = false;
    mlCache.confidence = 0.0f;
    mlCache.lastUpdateTime = 0;
    
    // Initialize performance profile
    memset(&profile, 0, sizeof(PerformanceProfile));
    profile.minComputeTimeUs = 0xFFFFFFFF;  // Start with max value
    
    // PHASE 2: Initialize ML task data
    mlTaskData.needsUpdate = false;
    mlTaskData.ambientTemp = 0.0f;
    mlTaskData.hour = 0;
    mlTaskData.season = 0;
    mlTaskData.tankVolume = 100.0f;
    mlTaskData.tds = 0.0f;
    
    // PHASE 2: Initialize Kalman filter
    kalman.initialized = false;
    kalman.x = 0.0f;
    kalman.p = 1.0f;
    kalman.q = 0.001f;
    kalman.r = 0.1f;
    
    // PHASE 3: Initialize parameter transition
    paramTransition.inTransition = false;
    
    // PHASE 3: Initialize health metrics
    memset(&health, 0, sizeof(HealthMetrics));
    health.lastHealthCheck = millis();
    health.hasError = false;
    
    // PHASE 3: Initialize feed-forward model
    feedForward.enabled = false;
    feedForward.tdsInfluence = 0.1f;
    feedForward.ambientTempInfluence = 0.3f;
    feedForward.phInfluence = 0.2f;
}

void AdaptivePID::allocateHistoryBuffers() {
#if PID_USE_PSRAM
    // Allocate in PSRAM for large buffers
    errorHistory = (float*)ps_malloc(historySize * sizeof(float));
    outputHistory = (float*)ps_malloc(historySize * sizeof(float));
    inputHistory = (float*)ps_malloc(historySize * sizeof(float));
    
    if (!errorHistory || !outputHistory || !inputHistory) {
        Serial.printf("ERROR: Failed to allocate PSRAM for PID '%s' history buffers\n", 
                     namespace_name.c_str());
        // Fallback to smaller SRAM allocation
        if (errorHistory) free(errorHistory);
        if (outputHistory) free(outputHistory);
        if (inputHistory) free(inputHistory);
        
        historySize = 100;  // Fallback to smaller size
        errorHistory = (float*)malloc(historySize * sizeof(float));
        outputHistory = (float*)malloc(historySize * sizeof(float));
        inputHistory = (float*)malloc(historySize * sizeof(float));
    } else {
        Serial.printf("PID '%s': Allocated %u samples in PSRAM (%.1f KB)\n",
                     namespace_name.c_str(), historySize, 
                     (historySize * 3 * sizeof(float)) / 1024.0f);
    }
#else
    // Standard SRAM allocation for smaller buffers
    errorHistory = (float*)malloc(historySize * sizeof(float));
    outputHistory = (float*)malloc(historySize * sizeof(float));
    inputHistory = (float*)malloc(historySize * sizeof(float));
    
    Serial.printf("PID '%s': Allocated %u samples in SRAM\n",
                 namespace_name.c_str(), historySize);
#endif
    
    // Initialize buffers to zero
    if (errorHistory && outputHistory && inputHistory) {
        for (uint16_t i = 0; i < historySize; i++) {
            errorHistory[i] = 0;
            outputHistory[i] = 0;
            inputHistory[i] = 0;
        }
    } else {
        Serial.printf("CRITICAL: PID '%s' history buffer allocation failed!\n",
                     namespace_name.c_str());
    }
}

void AdaptivePID::freeHistoryBuffers() {
    if (errorHistory) {
#if PID_USE_PSRAM
        free(errorHistory);  // ps_malloc uses standard free()
#else
        free(errorHistory);
#endif
        errorHistory = nullptr;
    }
    
    if (outputHistory) {
        free(outputHistory);
        outputHistory = nullptr;
    }
    
    if (inputHistory) {
        free(inputHistory);
        inputHistory = nullptr;
    }
}

AdaptivePID::~AdaptivePID() {
    // PHASE 2: Stop ML task if running
    if (useDualCore && mlTaskHandle) {
        mlTaskRunning = false;
        vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit
        vTaskDelete(mlTaskHandle);
        mlTaskHandle = nullptr;
    }
    
    // PHASE 2: Clean up semaphores
    if (paramMutex) {
        vSemaphoreDelete(paramMutex);
        paramMutex = nullptr;
    }
    if (mlWakeupSemaphore) {
        vSemaphoreDelete(mlWakeupSemaphore);
        mlWakeupSemaphore = nullptr;
    }
    
    // PHASE 1: Clean up hardware timer
    if (controlTimer) {
        timerEnd(controlTimer);
        controlTimer = nullptr;
    }
    
    // PHASE 1: Free history buffers
    freeHistoryBuffers();
    
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

void AdaptivePID::begin() {
    loadParameters();
    loadCacheFromNVS();  // PHASE 1: Load ML cache from NVS
    lastTime = millis();
    performanceWindowStart = millis(); // Initialize ML performance window
    
    Serial.printf("AdaptivePID '%s' initialized with Kp=%.3f, Ki=%.3f, Kd=%.3f\n", 
                  namespace_name.c_str(), kp, ki, kd);
    Serial.printf("  History: %u samples, PSRAM: %s, ML Cache: %s\n",
                  historySize,
                  PID_USE_PSRAM ? "YES" : "NO",
                  mlCache.valid ? "VALID" : "EMPTY");
}

void AdaptivePID::setTarget(float setpoint) {
    target = setpoint;
    
    // Initialize effective target for ramping
    if (useSetpointRamping && effectiveTarget == 0) {
        effectiveTarget = lastInput; // Start from current position
    } else if (!useSetpointRamping) {
        effectiveTarget = setpoint;
    }
    
    // Reset integral when target changes to prevent windup
    integral = 0;
    
    // Reset performance metrics
    settlingStartTime = millis();
    isSettled = false;
    maxOvershoot = 0;
    
    Serial.printf("PID '%s' target set to %.2f\n", namespace_name.c_str(), target);
}

void AdaptivePID::setSafetyLimits(float maxValue, float safetyMargin) {
    absoluteMax = maxValue;
    safetyThreshold = safetyMargin;
}

void AdaptivePID::setOutputLimits(float min, float max) {
    outputMin = min;
    outputMax = max;
}

float AdaptivePID::compute(float input, float dt) {
    // PHASE 1: Start performance profiling
    uint32_t startTime = 0;
    if (enableProfiling) {
        startTime = esp_timer_get_time();  // Microsecond precision
    }
    
    // PHASE 2: Apply Kalman filter to input if enabled
    float filteredInput = input;
    if (useKalman) {
        filteredInput = kalmanFilterInput(input);
    }
    
    // PHASE 3: Update parameter transition if in progress
    updateParameterTransition();
    
    // Update setpoint ramping
    if (useSetpointRamping) {
        updateSetpointRamp(dt);
    } else {
        effectiveTarget = target;
    }
    
    // Safety check: Emergency stop if significantly over target
    if (input > target + safetyThreshold) {
        if (!emergencyStop) {
            emergencyStop = true;
            emergencyStopTime = millis();
            Serial.printf("EMERGENCY STOP: '%s' value %.2f exceeds safety threshold (target: %.2f + %.2f)\n",
                         namespace_name.c_str(), input, target, safetyThreshold);
        }
        return 0; // Force output to minimum
    }
    
    // Calculate error (using effective target for ramping, with filtered input)
    float error = effectiveTarget - filteredInput;
    
    // Auto-tuning relay feedback
    if (autoTuning) {
        detectOscillation(error);
        // Simple relay control for auto-tuning
        float output = (error > 0) ? relayAmplitude : 0;
        lastOutput = output;
        lastInput = input;
        return output;
    }
    
    // Proportional term
    float pTerm = kp * error;
    
    // Integral term with advanced windup prevention
    integral += error * dt;
    if (useIntegralWindupPrevention) {
        // Clamp integral to prevent windup
        integral = constrain(integral, -integralMax, integralMax);
        
        // Back-calculation anti-windup: stop integrating if output is saturated
        if ((lastOutput >= outputMax && error > 0) || 
            (lastOutput <= outputMin && error < 0)) {
            integral -= error * dt; // Undo the integration
        }
    }
    float iTerm = ki * integral;
    
    // Derivative term with filtering and derivative-on-measurement
    float dTerm = 0;
    if (useDerivativeFilter) {
        // Derivative on measurement (not error) to avoid derivative kick
        dTerm = computeDerivative(input, dt);
    } else {
        // Standard derivative on error
        float derivative = (dt > 0) ? (error - lastError) / dt : 0;
        dTerm = kd * derivative;
    }
    
    // Feed-forward term
    float ffTerm = 0;
    if (useFeedForward) {
        ffTerm = feedForwardGain * effectiveTarget;
    }
    
    // Calculate output
    float output = pTerm + iTerm + dTerm + ffTerm;
    output = constrain(output, outputMin, outputMax);
    
    // Update performance metrics
    updatePerformanceMetrics(error, input);
    
    // Track performance window for ML logging
    if (mlLogger) {
        performanceWindowErrorSum += abs(error);
        performanceWindowErrorSqSum += error * error;
        performanceWindowOutputSum += output;
        performanceWindowSamples++;
    }
    
    // PHASE 1: Store error, output, and input for learning (with bounds checking)
    if (errorHistory && outputHistory && inputHistory) {
        errorHistory[errorIndex] = error;
        outputHistory[errorIndex] = output;
        inputHistory[errorIndex] = input;
        errorIndex = (errorIndex + 1) % historySize;
    }
    
    // Update performance metric (average absolute error)
    float errorSum = 0;
    if (errorHistory) {
        for (uint16_t i = 0; i < historySize; i++) {
            errorSum += abs(errorHistory[i]);
        }
        performanceMetric = errorSum / (float)historySize;
    }
    
    // Adapt parameters based on performance
    adaptParameters();
    
    // PHASE 3: Update health monitoring
    if (useHealthMonitoring) {
        updateHealthMetrics();
    }
    
    // Update state
    lastError = error;
    lastOutput = output;
    lastInput = input;
    controlActions++;
    
    // PHASE 1: End performance profiling
    if (enableProfiling) {
        uint32_t computeTime = esp_timer_get_time() - startTime;
        profile.computeTimeUs = computeTime;
        
        if (computeTime > profile.maxComputeTimeUs) {
            profile.maxComputeTimeUs = computeTime;
        }
        if (profile.minComputeTimeUs == 0xFFFFFFFF || computeTime < profile.minComputeTimeUs) {
            profile.minComputeTimeUs = computeTime;
        }
        
        // Running average
        profile.avgComputeTimeUs = (profile.avgComputeTimeUs * profile.computeCount + computeTime) 
                                   / (profile.computeCount + 1);
        profile.computeCount++;
        
        // Check for overruns (>10% of control period)
        if (useHardwareTimer && computeTime > (controlPeriodUs * 0.1)) {
            profile.overrunCount++;
        }
        
        // Calculate CPU usage
        if (dt > 0) {
            profile.cpuUsagePercent = (computeTime / (dt * 1000000.0f)) * 100.0f;
        }
    }
    
    return output;
}

void AdaptivePID::adaptParameters() {
    // Simple adaptive algorithm: adjust gains based on error magnitude and trend
    // This runs periodically to fine-tune the PID
    
    static unsigned long lastAdaptTime = 0;
    static const unsigned long ADAPT_INTERVAL = 60000; // Adapt every minute
    
    if (millis() - lastAdaptTime < ADAPT_INTERVAL) return;
    if (!errorHistory) return;  // PHASE 1: Safety check
    lastAdaptTime = millis();
    
    // PHASE 1: Calculate error variance over history buffer
    float errorMean = 0;
    for (uint16_t i = 0; i < historySize; i++) {
        errorMean += errorHistory[i];
    }
    errorMean /= (float)historySize;
    
    float errorVariance = 0;
    for (uint16_t i = 0; i < historySize; i++) {
        float diff = errorHistory[i] - errorMean;
        errorVariance += diff * diff;
    }
    errorVariance /= (float)historySize;
    
    // If error is consistently high, increase proportional gain
    if (abs(errorMean) > 1.0 && errorVariance < 0.5) {
        kp *= 1.05; // Increase by 5%
        Serial.printf("Adapted Kp to %.3f (high steady error)\n", kp);
    }
    
    // If oscillating (high variance), reduce derivative gain
    if (errorVariance > 2.0) {
        kd *= 0.95; // Decrease by 5%
        Serial.printf("Adapted Kd to %.3f (oscillation detected)\n", kd);
    }
    
    // Keep parameters within reasonable bounds
    kp = constrain(kp, 0.1, 20.0);
    ki = constrain(ki, 0.01, 5.0);
    kd = constrain(kd, 0.01, 10.0);
    
    // Save adapted parameters so they persist across restarts
    saveParameters();
    Serial.printf("PID '%s' adapted parameters saved to NVS\n", namespace_name.c_str());
}

void AdaptivePID::detectOscillation(float error) {
    // Detect peaks in error signal for auto-tuning
    static float lastErrorValue = 0;
    static bool risingEdge = false;
    
    if (error > lastErrorValue) {
        risingEdge = true;
    } else if (error < lastErrorValue && risingEdge) {
        // Peak detected
        unsigned long now = millis();
        
        if (peakDetected) {
            oscillationPeriod = now - lastPeakTime;
            oscillationAmplitude = abs(error - lastPeakValue);
            oscillationCount++;
            
            // After 3 oscillations, calculate PID parameters
            if (oscillationCount >= 3) {
                calculateAutoTuneParams();
                stopAutoTune();
            }
        }
        
        lastPeakTime = now;
        lastPeakValue = error;
        peakDetected = true;
        risingEdge = false;
    }
    
    lastErrorValue = error;
}

void AdaptivePID::calculateAutoTuneParams() {
    // Ziegler-Nichols method
    float Ku = (4.0 * relayAmplitude) / (PI * oscillationAmplitude); // Ultimate gain
    float Tu = oscillationPeriod / 1000.0; // Ultimate period in seconds
    
    // PID parameters using Ziegler-Nichols tuning rules
    kp = 0.6 * Ku;
    ki = 1.2 * Ku / Tu;
    kd = 0.075 * Ku * Tu;
    
    Serial.printf("Auto-tune complete: Ku=%.3f, Tu=%.3f\n", Ku, Tu);
    Serial.printf("New parameters: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", kp, ki, kd);
    
    saveParameters();
}

void AdaptivePID::startAutoTune(float amplitude) {
    autoTuning = true;
    relayAmplitude = amplitude;
    oscillationCount = 0;
    peakDetected = false;
    tuneStartTime = millis();
    Serial.printf("Starting auto-tune for '%s' with amplitude %.2f\n", 
                  namespace_name.c_str(), amplitude);
}

void AdaptivePID::stopAutoTune() {
    autoTuning = false;
    Serial.printf("Auto-tune stopped for '%s'\n", namespace_name.c_str());
}

void AdaptivePID::reset() {
    integral = 0;
    lastError = 0;
    lastOutput = 0;
    for (int i = 0; i < 100; i++) {
        errorHistory[i] = 0;
    }
    errorIndex = 0;
}

void AdaptivePID::clearEmergencyStop() {
    emergencyStop = false;
    integral = 0; // Reset integral to prevent sudden jumps
    Serial.printf("Emergency stop cleared for '%s'\n", namespace_name.c_str());
}

void AdaptivePID::setParameters(float p, float i, float d) {
    kp = p;
    ki = i;
    kd = d;
    Serial.printf("PID '%s' parameters manually set: Kp=%.3f, Ki=%.3f, Kd=%.3f\n",
                  namespace_name.c_str(), kp, ki, kd);
}

void AdaptivePID::getParameters(float& p, float& i, float& d) {
    p = kp;
    i = ki;
    d = kd;
}

void AdaptivePID::saveParameters() {
    if (!prefs->begin(namespace_name.c_str(), false)) {
        Serial.printf("ERROR: Failed to save PID parameters for '%s'\n", namespace_name.c_str());
        return;
    }
    
    prefs->putFloat("kp", kp);
    prefs->putFloat("ki", ki);
    prefs->putFloat("kd", kd);
    prefs->putFloat("target", target);
    
    prefs->end();
    Serial.printf("PID '%s' parameters saved\n", namespace_name.c_str());
}

void AdaptivePID::loadParameters() {
    if (!prefs->begin(namespace_name.c_str(), true)) {
        Serial.printf("No saved parameters for '%s', using defaults\n", namespace_name.c_str());
        return;
    }
    
    kp = prefs->getFloat("kp", kp);
    ki = prefs->getFloat("ki", ki);
    kd = prefs->getFloat("kd", kd);
    target = prefs->getFloat("target", target);
    
    prefs->end();
    Serial.printf("PID '%s' parameters loaded from NVS\n", namespace_name.c_str());
}

// Advanced PID feature implementations

void AdaptivePID::enableDerivativeFilter(bool enable, float coefficient) {
    useDerivativeFilter = enable;
    derivativeFilterCoeff = constrain(coefficient, 0.0, 1.0);
    if (enable) {
        Serial.printf("PID '%s': Derivative filter enabled (coefficient: %.2f)\n", 
                      namespace_name.c_str(), derivativeFilterCoeff);
    }
}

void AdaptivePID::enableSetpointRamping(bool enable, float rate) {
    useSetpointRamping = enable;
    rampRate = rate;
    if (enable) {
        effectiveTarget = lastInput; // Start from current position
        Serial.printf("PID '%s': Setpoint ramping enabled (rate: %.2f units/sec)\n", 
                      namespace_name.c_str(), rampRate);
    }
}

void AdaptivePID::enableIntegralWindupPrevention(bool enable, float maxIntegral) {
    useIntegralWindupPrevention = enable;
    integralMax = maxIntegral;
    if (enable) {
        Serial.printf("PID '%s': Integral windup prevention enabled (max: %.2f)\n", 
                      namespace_name.c_str(), integralMax);
    }
}

void AdaptivePID::enableFeedForward(bool enable, float gain) {
    useFeedForward = enable;
    feedForwardGain = gain;
    if (enable) {
        Serial.printf("PID '%s': Feed-forward control enabled (gain: %.2f)\n", 
                      namespace_name.c_str(), feedForwardGain);
    }
}

float AdaptivePID::computeDerivative(float input, float dt) {
    if (dt <= 0) return filteredDerivative;
    
    // Derivative on measurement (inverted because error = target - input)
    float rawDerivative = -(input - lastInput) / dt;
    
    // Low-pass filter: filtered = alpha * new + (1-alpha) * old
    filteredDerivative = derivativeFilterCoeff * rawDerivative + 
                        (1.0 - derivativeFilterCoeff) * filteredDerivative;
    
    return kd * filteredDerivative;
}

void AdaptivePID::updateSetpointRamp(float dt) {
    if (dt <= 0) return;
    
    float maxChange = rampRate * dt;
    float targetDiff = target - effectiveTarget;
    
    if (abs(targetDiff) <= maxChange) {
        effectiveTarget = target; // Close enough, snap to target
    } else if (targetDiff > 0) {
        effectiveTarget += maxChange; // Ramp up
    } else {
        effectiveTarget -= maxChange; // Ramp down
    }
}

void AdaptivePID::updatePerformanceMetrics(float error, float input) {
    // Track settling time (within 2% of target)
    float settlingBand = target * 0.02;
    if (abs(error) <= settlingBand) {
        if (!isSettled) {
            settlingTime = millis() - settlingStartTime;
            isSettled = true;
        }
        steadyStateError = error; // Update steady-state error
    } else {
        isSettled = false;
    }
    
    // Track maximum overshoot
    if (input > target) {
        float overshoot = ((input - target) / target) * 100.0; // Percentage
        if (overshoot > maxOvershoot) {
            maxOvershoot = overshoot;
        }
    }
}

// ============================================================================
// ML Integration Methods
// ============================================================================

void AdaptivePID::setMLLogger(MLDataLogger* logger) {
    mlLogger = logger;
    if (mlLogger) {
        Serial.printf("PID '%s': ML data logger attached\n", namespace_name.c_str());
    }
}

float AdaptivePID::computeWithContext(float input, float dt, float ambientTemp, uint8_t hourOfDay, 
                                     uint8_t season, float tankVolume) {
    // First compute the normal PID output
    float output = compute(input, dt);
    
    // If ML is enabled and we have a logger, try to adapt parameters
    if (mlEnabled && mlLogger) {
        adaptParametersWithML(ambientTemp, hourOfDay, season);
    }
    
    // Log performance data every settling period for ML learning
    if (mlLogger && isSettled) {
        logPerformanceToML(ambientTemp, hourOfDay, season, tankVolume);
    }
    
    return output;
}

void AdaptivePID::enableMLAdaptation(bool enable) {
    mlEnabled = enable;
    if (enable && mlLogger) {
        Serial.printf("PID '%s': ML adaptation enabled\n", namespace_name.c_str());
    } else if (enable && !mlLogger) {
        Serial.printf("WARNING: PID '%s' ML adaptation requested but no logger attached\n", 
                     namespace_name.c_str());
        mlEnabled = false;
    } else {
        Serial.printf("PID '%s': ML adaptation disabled\n", namespace_name.c_str());
    }
}

void AdaptivePID::adaptParametersWithML(float ambientTemp, uint8_t hourOfDay, uint8_t season) {
    if (!mlLogger || !mlEnabled) return;
    
    // PHASE 1: Check ML parameter cache validity (conditions haven't changed much)
    uint32_t now = millis();
    bool cacheValid = mlCache.valid && 
                     (now - mlCache.lastUpdateTime < mlCacheValidityMs) &&
                     (abs(mlCache.ambientTemp - ambientTemp) < 1.0f) &&
                     (mlCache.hour == hourOfDay) &&
                     (mlCache.season == season);
    
    if (cacheValid) {
        // PHASE 1: Cache HIT - use cached parameters (fast path)
        mlCacheHits++;
        
        float blend = 0.7f;
        kp = kp * (1.0f - blend) + mlCache.kp_ml * blend;
        ki = ki * (1.0f - blend) + mlCache.ki_ml * blend;
        kd = kd * (1.0f - blend) + mlCache.kd_ml * blend;
        
        // Keep within safety bounds
        kp = constrain(kp, 0.1, 20.0);
        ki = constrain(ki, 0.01, 5.0);
        kd = constrain(kd, 0.01, 10.0);
        
        mlConfidence = mlCache.confidence;
        return;  // Fast exit - no ML query needed
    }
    
    // PHASE 1: Cache MISS - query ML system
    mlCacheMisses++;
    
    // Query the ML lookup table for optimal gains based on current conditions
    float kp_ml, ki_ml, kd_ml, confidence;
    bool found = mlLogger->getOptimalGains(lastInput, ambientTemp, hourOfDay, season, 
                                          kp_ml, ki_ml, kd_ml, confidence);
    
    if (found && confidence > 0.7f) {
        // PHASE 1: Update cache with new ML parameters
        mlCache.ambientTemp = ambientTemp;
        mlCache.hour = hourOfDay;
        mlCache.season = season;
        mlCache.kp_ml = kp_ml;
        mlCache.ki_ml = ki_ml;
        mlCache.kd_ml = kd_ml;
        mlCache.confidence = confidence;
        mlCache.lastUpdateTime = now;
        mlCache.valid = true;
        
        // Persist cache to NVS for fast startup
        saveCacheToNVS();
        
        // High confidence: blend ML recommendations with current gains
        // Use 70% ML, 30% current to smooth transitions
        float blend = 0.7f;
        kp = kp * (1.0f - blend) + kp_ml * blend;
        ki = ki * (1.0f - blend) + ki_ml * blend;
        kd = kd * (1.0f - blend) + kd_ml * blend;
        
        // Keep within safety bounds
        kp = constrain(kp, 0.1, 20.0);
        ki = constrain(ki, 0.01, 5.0);
        kd = constrain(kd, 0.01, 10.0);
        
        mlConfidence = confidence;
        
        Serial.printf("PID '%s': ML adaptation applied (confidence: %.2f%%, cache updated)\n", 
                     namespace_name.c_str(), confidence * 100.0f);
        Serial.printf("  New gains: Kp=%.3f Ki=%.3f Kd=%.3f\n", kp, ki, kd);
    } else if (found && confidence > 0.5f) {
        // PHASE 1: Update cache even for medium confidence
        mlCache.ambientTemp = ambientTemp;
        mlCache.hour = hourOfDay;
        mlCache.season = season;
        mlCache.kp_ml = kp_ml;
        mlCache.ki_ml = ki_ml;
        mlCache.kd_ml = kd_ml;
        mlCache.confidence = confidence;
        mlCache.lastUpdateTime = now;
        mlCache.valid = true;
        saveCacheToNVS();
        
        // Medium confidence: use as fallback only if current performance is poor
        if (performanceMetric > 2.0f) { // High average error
            float blend = 0.3f; // More conservative blending
            kp = kp * (1.0f - blend) + kp_ml * blend;
            ki = ki * (1.0f - blend) + ki_ml * blend;
            kd = kd * (1.0f - blend) + kd_ml * blend;
            
            kp = constrain(kp, 0.1, 20.0);
            ki = constrain(ki, 0.01, 5.0);
            kd = constrain(kd, 0.01, 10.0);
            
            mlConfidence = confidence;
            
            Serial.printf("PID '%s': ML adaptation (moderate confidence: %.2f%%, poor performance)\n", 
                         namespace_name.c_str(), confidence * 100.0f);
        }
    } else {
        // Low or no confidence: don't adapt, but track confidence
        mlConfidence = confidence;
        mlCache.valid = false;  // Invalidate cache
    }
}

void AdaptivePID::logPerformanceToML(float ambientTemp, uint8_t hourOfDay, 
                                     uint8_t season, float tankVolume) {
    if (!mlLogger) return;
    
    // Check if we've completed a performance window
    unsigned long now = millis();
    unsigned long windowDuration = now - performanceWindowStart;
    
    // Log every 10 minutes of settled operation
    const unsigned long LOG_INTERVAL = 600000; // 10 minutes
    if (windowDuration < LOG_INTERVAL) return;
    
    // Calculate windowed performance metrics
    float avgError = (performanceWindowSamples > 0) ? (performanceWindowErrorSum / performanceWindowSamples) : 0;
    float errorVariance = 0;
    if (performanceWindowSamples > 0) {
        float avgSqError = performanceWindowErrorSqSum / performanceWindowSamples;
        errorVariance = avgSqError - (avgError * avgError);
        if (errorVariance < 0) errorVariance = 0; // Numerical stability
    }
    float avgOutput = (performanceWindowSamples > 0) ? (performanceWindowOutputSum / performanceWindowSamples) : lastOutput;
    
    // Create performance sample
    PIDPerformanceSample sample;
    sample.timestamp = now;
    sample.currentValue = lastInput;
    sample.targetValue = target;
    sample.ambientTemp = ambientTemp;
    sample.hourOfDay = hourOfDay;
    sample.season = season;
    sample.tankVolume = tankVolume;
    
    sample.kp = kp;
    sample.ki = ki;
    sample.kd = kd;
    
    sample.errorMean = avgError;
    sample.errorVariance = errorVariance;
    sample.settlingTime = settlingTime / 1000.0f; // Convert to seconds
    sample.overshoot = maxOvershoot;
    sample.steadyStateError = steadyStateError;
    sample.averageOutput = avgOutput;
    sample.cycleCount = performanceWindowSamples;
    
    // Let MLDataLogger calculate the performance score
    sample.score = 0; // Will be calculated in logSample()
    
    // Log the sample
    if (mlLogger->logSample(sample)) {
        Serial.printf("PID '%s': Performance sample logged (window: %lu sec, samples: %u)\n",
                     namespace_name.c_str(), windowDuration / 1000, performanceWindowSamples);
        Serial.printf("  Settling: %.1fs, Overshoot: %.2f%%, SSE: %.3f, Variance: %.4f\n",
                     sample.settlingTime, sample.overshoot, sample.steadyStateError, 
                     sample.errorVariance);
    } else {
        Serial.printf("WARNING: PID '%s' failed to log ML sample\n", namespace_name.c_str());
    }
    
    // Reset performance window
    performanceWindowStart = now;
    performanceWindowErrorSum = 0;
    performanceWindowErrorSqSum = 0;
    performanceWindowOutputSum = 0;
    performanceWindowSamples = 0;
    
    // Reset performance metrics for next window
    maxOvershoot = 0;
    settlingTime = 0;
    isSettled = false;
    settlingStartTime = now;
}

// ============================================================================
// PHASE 1: Hardware Timer Implementation
// ============================================================================

// Static instance pointer for ISR callback
static AdaptivePID* g_pidInstance = nullptr;

void IRAM_ATTR onControlTimerISR() {
    if (g_pidInstance) {
        g_pidInstance->computeReady = true;
    }
}

void AdaptivePID::enableHardwareTimer(uint32_t periodMs) {
    if (controlTimer) {
        Serial.printf("WARNING: PID '%s' hardware timer already enabled\n", namespace_name.c_str());
        return;
    }
    
    controlPeriodUs = periodMs * 1000;
    g_pidInstance = this;  // Store instance for ISR
    
    // Create hardware timer (use timer 0, 80 prescaler = 1MHz = 1μs per tick)
    controlTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(controlTimer, &onControlTimerISR, true);
    timerAlarmWrite(controlTimer, controlPeriodUs, true);  // Auto-reload
    timerAlarmEnable(controlTimer);
    
    useHardwareTimer = true;
    computeReady = false;
    
    Serial.printf("PID '%s': Hardware timer enabled (%u ms period, %u μs)\n", 
                  namespace_name.c_str(), periodMs, controlPeriodUs);
}

void AdaptivePID::disableHardwareTimer() {
    if (controlTimer) {
        timerEnd(controlTimer);
        controlTimer = nullptr;
        useHardwareTimer = false;
        Serial.printf("PID '%s': Hardware timer disabled\n", namespace_name.c_str());
    }
}

float AdaptivePID::computeIfReady(float input) {
    if (!useHardwareTimer) {
        // Timer not enabled, compute immediately with default dt
        float dt = (millis() - lastTime) / 1000.0f;
        lastTime = millis();
        return compute(input, dt);
    }
    
    if (!computeReady) {
        return lastOutput;  // Not yet time to compute
    }
    
    computeReady = false;
    float dt = controlPeriodUs / 1000000.0f;  // Convert to seconds
    return compute(input, dt);
}

// ============================================================================
// PHASE 1: Performance Profiling
// ============================================================================

void AdaptivePID::enablePerformanceProfiling(bool enable) {
    enableProfiling = enable;
    
    if (enable) {
        // Reset profile statistics
        memset(&profile, 0, sizeof(PerformanceProfile));
        profile.minComputeTimeUs = 0xFFFFFFFF;
        Serial.printf("PID '%s': Performance profiling ENABLED\n", namespace_name.c_str());
    } else {
        Serial.printf("PID '%s': Performance profiling DISABLED\n", namespace_name.c_str());
    }
}

String AdaptivePID::getProfileReport() {
    String report = "╔════════════════════════════════════════════════════════════╗\n";
    report += "║ PID '" + namespace_name + "' Performance Profile\n";
    report += "╠════════════════════════════════════════════════════════════╣\n";
    
    if (!enableProfiling || profile.computeCount == 0) {
        report += "║ Profiling disabled or no data collected\n";
        report += "╚════════════════════════════════════════════════════════════╝\n";
        return report;
    }
    
    report += "║ Compute Time (μs):\n";
    report += "║   Current:  " + String(profile.computeTimeUs) + " μs\n";
    report += "║   Min:      " + String(profile.minComputeTimeUs) + " μs\n";
    report += "║   Average:  " + String(profile.avgComputeTimeUs) + " μs\n";
    report += "║   Max:      " + String(profile.maxComputeTimeUs) + " μs\n";
    report += "║ \n";
    report += "║ CPU Usage:   " + String(profile.cpuUsagePercent, 2) + " %\n";
    report += "║ \n";
    report += "║ Statistics:\n";
    report += "║   Compute Count: " + String(profile.computeCount) + "\n";
    report += "║   Overruns:      " + String(profile.overrunCount);
    
    if (profile.overrunCount > 0) {
        float overrunRate = ((float)profile.overrunCount / (float)profile.computeCount) * 100.0f;
        report += " (" + String(overrunRate, 2) + "%)";
    }
    report += "\n";
    
    if (useHardwareTimer) {
        report += "║   Timer Period:  " + String(controlPeriodUs / 1000) + " ms\n";
    }
    
    report += "╚════════════════════════════════════════════════════════════╝\n";
    return report;
}

// ============================================================================
// PHASE 1: ML Cache Management
// ============================================================================

void AdaptivePID::saveCacheToNVS() {
    if (!prefs->begin(namespace_name.c_str(), false)) {
        Serial.printf("ERROR: Failed to save ML cache for '%s'\n", namespace_name.c_str());
        return;
    }
    
    prefs->putFloat("ml_kp", mlCache.kp_ml);
    prefs->putFloat("ml_ki", mlCache.ki_ml);
    prefs->putFloat("ml_kd", mlCache.kd_ml);
    prefs->putFloat("ml_conf", mlCache.confidence);
    prefs->putFloat("ml_temp", mlCache.ambientTemp);
    prefs->putUChar("ml_hour", mlCache.hour);
    prefs->putUChar("ml_season", mlCache.season);
    prefs->putUInt("ml_time", mlCache.lastUpdateTime);
    prefs->putBool("ml_valid", mlCache.valid);
    
    prefs->end();
}

void AdaptivePID::loadCacheFromNVS() {
    if (!prefs->begin(namespace_name.c_str(), true)) {
        Serial.printf("No ML cache found for '%s'\n", namespace_name.c_str());
        mlCache.valid = false;
        return;
    }
    
    mlCache.kp_ml = prefs->getFloat("ml_kp", 0);
    mlCache.ki_ml = prefs->getFloat("ml_ki", 0);
    mlCache.kd_ml = prefs->getFloat("ml_kd", 0);
    mlCache.confidence = prefs->getFloat("ml_conf", 0);
    mlCache.ambientTemp = prefs->getFloat("ml_temp", 0);
    mlCache.hour = prefs->getUChar("ml_hour", 0);
    mlCache.season = prefs->getUChar("ml_season", 0);
    mlCache.lastUpdateTime = prefs->getUInt("ml_time", 0);
    mlCache.valid = prefs->getBool("ml_valid", false);
    
    prefs->end();
    
    if (mlCache.valid) {
        Serial.printf("PID '%s': ML cache loaded (Kp=%.3f, Ki=%.3f, Kd=%.3f, conf=%.2f%%)\n",
                     namespace_name.c_str(), mlCache.kp_ml, mlCache.ki_ml, mlCache.kd_ml,
                     mlCache.confidence * 100.0f);
    }
}

// ============================================================================
// PHASE 1: History Buffer Access
// ============================================================================

float AdaptivePID::getErrorHistory(uint16_t index) {
    if (index >= historySize || !errorHistory) return 0.0f;
    return errorHistory[index];
}

float AdaptivePID::getOutputHistory(uint16_t index) {
    if (index >= historySize || !outputHistory) return 0.0f;
    return outputHistory[index];
}

float AdaptivePID::getInputHistory(uint16_t index) {
    if (index >= historySize || !inputHistory) return 0.0f;
    return inputHistory[index];
}

// ============================================================================
// PHASE 2: Dual-Core ML Processing
// ============================================================================

void AdaptivePID::enableDualCoreML(bool enable) {
    if (enable && !useDualCore) {
        // Create mutex for parameter protection
        paramMutex = xSemaphoreCreateMutex();
        mlWakeupSemaphore = xSemaphoreCreateBinary();
        
        if (!paramMutex || !mlWakeupSemaphore) {
            Serial.printf("ERROR: Failed to create semaphores for PID '%s'\n", namespace_name.c_str());
            if (paramMutex) vSemaphoreDelete(paramMutex);
            if (mlWakeupSemaphore) vSemaphoreDelete(mlWakeupSemaphore);
            return;
        }
        
        // Create ML adaptation task on Core 0 (Core 1 reserved for control loop)
        mlTaskRunning = true;
        BaseType_t result = xTaskCreatePinnedToCore(
            mlAdaptationTask,        // Task function
            "ML_Adaptation",         // Task name
            8192,                    // Stack size (8KB)
            this,                    // Pass PID instance as parameter
            2,                       // Priority (lower than control task)
            &mlTaskHandle,           // Task handle
            0                        // Core 0 (Core 1 for control loop)
        );
        
        if (result == pdPASS) {
            useDualCore = true;
            Serial.printf("PID '%s': Dual-core ML processing enabled (Core 0)\n", namespace_name.c_str());
        } else {
            Serial.printf("ERROR: Failed to create ML task for PID '%s'\n", namespace_name.c_str());
            vSemaphoreDelete(paramMutex);
            vSemaphoreDelete(mlWakeupSemaphore);
            paramMutex = nullptr;
            mlWakeupSemaphore = nullptr;
        }
    } else if (!enable && useDualCore) {
        // Disable dual-core processing
        mlTaskRunning = false;
        if (mlTaskHandle) {
            vTaskDelay(pdMS_TO_TICKS(100));  // Let task exit cleanly
            vTaskDelete(mlTaskHandle);
            mlTaskHandle = nullptr;
        }
        if (paramMutex) {
            vSemaphoreDelete(paramMutex);
            paramMutex = nullptr;
        }
        if (mlWakeupSemaphore) {
            vSemaphoreDelete(mlWakeupSemaphore);
            mlWakeupSemaphore = nullptr;
        }
        useDualCore = false;
        Serial.printf("PID '%s': Dual-core ML processing disabled\n", namespace_name.c_str());
    }
}

void AdaptivePID::triggerMLAdaptation(float ambientTemp, uint8_t hour, uint8_t season, float tankVolume, float tds) {
    if (!useDualCore) {
        // Fallback: synchronous adaptation (Phase 1 behavior)
        adaptParametersWithML(ambientTemp, hour, season);
        return;
    }
    
    // Update ML task data and signal the task
    mlTaskData.ambientTemp = ambientTemp;
    mlTaskData.hour = hour;
    mlTaskData.season = season;
    mlTaskData.tankVolume = tankVolume;
    mlTaskData.tds = tds;
    mlTaskData.needsUpdate = true;
    
    // Wake up ML task
    xSemaphoreGive(mlWakeupSemaphore);
}

void AdaptivePID::mlAdaptationTask(void* pvParameters) {
    AdaptivePID* pid = static_cast<AdaptivePID*>(pvParameters);
    
    Serial.printf("ML Adaptation task started for PID '%s' on Core %d\n", 
                  pid->namespace_name.c_str(), xPortGetCoreID());
    
    while (pid->mlTaskRunning) {
        // Wait for wakeup signal (with timeout)
        if (xSemaphoreTake(pid->mlWakeupSemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
            if (pid->mlTaskData.needsUpdate) {
                // Perform ML adaptation (may take 2-5ms for MQTT query)
                pid->adaptParametersWithML(
                    pid->mlTaskData.ambientTemp,
                    pid->mlTaskData.hour,
                    pid->mlTaskData.season
                );
                
                pid->mlTaskData.needsUpdate = false;
            }
        }
        
        // Small delay to prevent watchdog issues
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    Serial.printf("ML Adaptation task exiting for PID '%s'\n", pid->namespace_name.c_str());
    vTaskDelete(NULL);
}

// ============================================================================
// PHASE 2: Kalman Filter
// ============================================================================

void AdaptivePID::enableKalmanFilter(bool enable, float processNoise, float measurementNoise) {
    useKalman = enable;
    if (enable) {
        initKalmanFilter(processNoise, measurementNoise);
        Serial.printf("PID '%s': Kalman filter enabled (Q=%.4f, R=%.4f)\n", 
                     namespace_name.c_str(), processNoise, measurementNoise);
    } else {
        Serial.printf("PID '%s': Kalman filter disabled\n", namespace_name.c_str());
    }
}

void AdaptivePID::initKalmanFilter(float processNoise, float measurementNoise) {
    kalman.q = processNoise;
    kalman.r = measurementNoise;
    kalman.p = 1.0f;
    kalman.x = 0.0f;
    kalman.initialized = false;
}

float AdaptivePID::kalmanFilterInput(float measurement) {
    if (!kalman.initialized) {
        // Initialize with first measurement
        kalman.x = measurement;
        kalman.initialized = true;
        return measurement;
    }
    
    // Prediction step
    // x_pred = x (no process model, assumes constant)
    // p_pred = p + q
    float p_pred = kalman.p + kalman.q;
    
    // Update step
    // K = p_pred / (p_pred + r)  // Kalman gain
    // x = x_pred + K * (measurement - x_pred)  // State update
    // p = (1 - K) * p_pred  // Covariance update
    float K = p_pred / (p_pred + kalman.r);
    kalman.x = kalman.x + K * (measurement - kalman.x);
    kalman.p = (1.0f - K) * p_pred;
    
    return kalman.x;
}

// ============================================================================
// PHASE 3: Bumpless Transfer
// ============================================================================

void AdaptivePID::setParametersSmooth(float p, float i, float d, uint32_t durationMs) {
    if (paramMutex) {
        // Thread-safe parameter access
        xSemaphoreTake(paramMutex, portMAX_DELAY);
    }
    
    paramTransition.kp_start = kp;
    paramTransition.ki_start = ki;
    paramTransition.kd_start = kd;
    paramTransition.kp_target = p;
    paramTransition.ki_target = i;
    paramTransition.kd_target = d;
    paramTransition.transitionStartTime = millis();
    paramTransition.transitionDuration = durationMs;
    paramTransition.inTransition = true;
    
    if (paramMutex) {
        xSemaphoreGive(paramMutex);
    }
    
    Serial.printf("PID '%s': Starting smooth parameter transition over %u ms\n",
                  namespace_name.c_str(), durationMs);
    Serial.printf("  Kp: %.3f → %.3f\n", paramTransition.kp_start, p);
    Serial.printf("  Ki: %.3f → %.3f\n", paramTransition.ki_start, i);
    Serial.printf("  Kd: %.3f → %.3f\n", paramTransition.kd_start, d);
}

void AdaptivePID::updateParameterTransition() {
    if (!paramTransition.inTransition) return;
    
    uint32_t elapsed = millis() - paramTransition.transitionStartTime;
    
    if (elapsed >= paramTransition.transitionDuration) {
        // Transition complete
        if (paramMutex) xSemaphoreTake(paramMutex, portMAX_DELAY);
        
        kp = paramTransition.kp_target;
        ki = paramTransition.ki_target;
        kd = paramTransition.kd_target;
        paramTransition.inTransition = false;
        
        if (paramMutex) xSemaphoreGive(paramMutex);
        
        Serial.printf("PID '%s': Parameter transition complete\n", namespace_name.c_str());
    } else {
        // Linear interpolation
        float progress = (float)elapsed / (float)paramTransition.transitionDuration;
        
        if (paramMutex) xSemaphoreTake(paramMutex, portMAX_DELAY);
        
        kp = paramTransition.kp_start + (paramTransition.kp_target - paramTransition.kp_start) * progress;
        ki = paramTransition.ki_start + (paramTransition.ki_target - paramTransition.ki_start) * progress;
        kd = paramTransition.kd_start + (paramTransition.kd_target - paramTransition.kd_start) * progress;
        
        if (paramMutex) xSemaphoreGive(paramMutex);
    }
}

// ============================================================================
// PHASE 3: Health Monitoring
// ============================================================================

void AdaptivePID::enableHealthMonitoring(bool enable) {
    useHealthMonitoring = enable;
    if (enable) {
        memset(&health, 0, sizeof(HealthMetrics));
        health.lastHealthCheck = millis();
        health.hasError = false;
        Serial.printf("PID '%s': Health monitoring enabled\n", namespace_name.c_str());
    } else {
        Serial.printf("PID '%s': Health monitoring disabled\n", namespace_name.c_str());
    }
}

void AdaptivePID::updateHealthMetrics() {
    unsigned long now = millis();
    
    // Check every 30 seconds
    if (now - health.lastHealthCheck < 30000) return;
    health.lastHealthCheck = now;
    
    // Reset error flag
    health.hasError = false;
    health.errorMessage = "";
    
    // 1. Check for stuck output (output not changing despite error)
    if (abs(lastOutput - health.lastOutputValue) < 0.5f && abs(lastError) > 1.0f) {
        health.stuckOutputCount++;
        if (health.stuckOutputCount > 3) {  // 3 consecutive checks (90 seconds)
            health.outputStuck = true;
            health.hasError = true;
            health.errorMessage += "Output stuck despite error; ";
        }
    } else {
        health.stuckOutputCount = 0;
        health.outputStuck = false;
    }
    health.lastOutputValue = lastOutput;
    
    // 2. Check for persistent high error
    if (abs(lastError) > 5.0f) {  // Error > 5 units for extended period
        health.persistentHighError = true;
        health.hasError = true;
        health.errorMessage += "Persistent high error (>5.0); ";
    } else {
        health.persistentHighError = false;
    }
    
    // 3. Check for output saturation
    bool saturated = (lastOutput >= outputMax - 1.0f) || (lastOutput <= outputMin + 1.0f);
    if (saturated) {
        health.saturationCount++;
        if (health.saturationCount > 10) {  // Saturated for 5 minutes
            health.outputSaturation = true;
            health.hasError = true;
            health.errorMessage += "Output saturated too often; ";
        }
    } else {
        health.saturationCount = max(0, (int)health.saturationCount - 1);  // Slowly decay
        health.outputSaturation = false;
    }
    
    if (health.hasError) {
        Serial.printf("HEALTH WARNING [%s]: %s\n", namespace_name.c_str(), health.errorMessage.c_str());
    }
}

String AdaptivePID::getHealthReport() {
    String report = "\n╔══════════════════════════════════════════════════════════╗\n";
    report += "║         PID Health Report: " + namespace_name + "\n";
    report += "╠══════════════════════════════════════════════════════════╣\n";
    
    if (!useHealthMonitoring) {
        report += "║  Health monitoring: DISABLED\n";
        report += "╚══════════════════════════════════════════════════════════╝\n";
        return report;
    }
    
    report += "║  Overall Status: " + String(health.hasError ? "⚠️ WARNING" : "✓ HEALTHY") + "\n";
    report += "║ \n";
    report += "║  Diagnostics:\n";
    report += "║    Output Stuck:        " + String(health.outputStuck ? "YES" : "NO") + "\n";
    report += "║    Persistent Error:    " + String(health.persistentHighError ? "YES" : "NO") + "\n";
    report += "║    Output Saturation:   " + String(health.outputSaturation ? "YES" : "NO") + "\n";
    report += "║ \n";
    report += "║  Metrics:\n";
    report += "║    Current Error:       " + String(lastError, 2) + "\n";
    report += "║    Current Output:      " + String(lastOutput, 1) + " %\n";
    report += "║    Saturation Count:    " + String(health.saturationCount) + "\n";
    report += "║    Stuck Output Count:  " + String(health.stuckOutputCount) + "\n";
    
    if (health.hasError) {
        report += "║ \n";
        report += "║  Error Details:\n";
        report += "║    " + health.errorMessage + "\n";
    }
    
    report += "╚══════════════════════════════════════════════════════════╝\n";
    return report;
}

// ============================================================================
// PHASE 3: Predictive Feed-Forward with Sensor Data
// ============================================================================

void AdaptivePID::enableFeedForwardModel(bool enable, float tdsInfluence, float ambientInfluence, float phInfluence) {
    feedForward.enabled = enable;
    feedForward.tdsInfluence = tdsInfluence;
    feedForward.ambientTempInfluence = ambientInfluence;
    feedForward.phInfluence = phInfluence;
    
    // Initialize baseline values (will be updated on first computation)
    feedForward.baselineTDS = 300.0f;        // Typical aquarium TDS
    feedForward.baselineAmbientTemp = 22.0f; // Typical room temperature
    feedForward.baselinePH = 7.0f;           // Neutral pH
    
    // Initialize tracking
    feedForward.lastTdsContribution = 0.0f;
    feedForward.lastAmbientContribution = 0.0f;
    feedForward.lastPhContribution = 0.0f;
    feedForward.lastTotalContribution = 0.0f;
    
    if (enable) {
        Serial.printf("PID '%s': Feed-forward model enabled\n", namespace_name.c_str());
        Serial.printf("  TDS influence: %.3f\n", tdsInfluence);
        Serial.printf("  Ambient temp influence: %.3f\n", ambientInfluence);
        Serial.printf("  pH influence: %.3f\n", phInfluence);
    } else {
        Serial.printf("PID '%s': Feed-forward model disabled\n", namespace_name.c_str());
    }
}

float AdaptivePID::computeFeedForward(float tds, float ambientTemp, float ph) {
    if (!feedForward.enabled) {
        feedForward.lastTdsContribution = 0.0f;
        feedForward.lastAmbientContribution = 0.0f;
        feedForward.lastPhContribution = 0.0f;
        feedForward.lastTotalContribution = 0.0f;
        return 0.0f;
    }
    
    float ffCorrection = 0.0f;
    
    // TDS influence: Changes in TDS affect heat capacity
    // Lower TDS (after water change) = lower heat capacity = easier to heat
    feedForward.lastTdsContribution = 0.0f;
    if (tds > 0 && feedForward.baselineTDS > 0) {
        float tdsDelta = feedForward.baselineTDS - tds;  // Positive when TDS drops
        feedForward.lastTdsContribution = tdsDelta * feedForward.tdsInfluence;
        ffCorrection += feedForward.lastTdsContribution;
    }
    
    // Ambient temperature influence: Colder ambient = more heat loss
    // Need more heating when ambient is lower than baseline
    feedForward.lastAmbientContribution = 0.0f;
    if (ambientTemp > 0 && feedForward.baselineAmbientTemp > 0) {
        float ambientDelta = feedForward.baselineAmbientTemp - ambientTemp;  // Positive when colder
        feedForward.lastAmbientContribution = ambientDelta * feedForward.ambientTempInfluence;
        ffCorrection += feedForward.lastAmbientContribution;
    }
    
    // pH influence (mainly for CO2 control)
    // Higher pH = less dissolved CO2 = need more injection
    feedForward.lastPhContribution = 0.0f;
    if (ph > 0 && feedForward.baselinePH > 0) {
        float phDelta = ph - feedForward.baselinePH;  // Positive when pH rises
        feedForward.lastPhContribution = phDelta * feedForward.phInfluence;
        ffCorrection += feedForward.lastPhContribution;
    }
    
    // Convert to percentage (0-100)
    feedForward.lastTotalContribution = ffCorrection * 100.0f;
    
    return ffCorrection;
}

float AdaptivePID::computeWithSensorContext(float input, float dt, float ambientTemp, uint8_t hour, 
                                             uint8_t season, float tankVolume, float tds, float ph) {
    // Trigger ML adaptation (async if dual-core enabled)
    if (mlEnabled && mlLogger) {
        if (useDualCore) {
            triggerMLAdaptation(ambientTemp, hour, season, tankVolume, tds);
        } else {
            // Synchronous adaptation
            adaptParametersWithML(ambientTemp, hour, season);
        }
    }
    
    // Compute standard PID output
    float pidOutput = compute(input, dt);
    
    // Add feed-forward correction if enabled
    if (feedForward.enabled) {
        float ffCorrection = computeFeedForward(tds, ambientTemp, ph);
        pidOutput += ffCorrection;
        pidOutput = constrain(pidOutput, outputMin, outputMax);
    }
    
    return pidOutput;
}
