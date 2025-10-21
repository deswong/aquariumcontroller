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
      mlLogger(nullptr), mlEnabled(false), mlConfidence(0.0f) {
    
    prefs = new Preferences();
    
    // Initialize error history
    for (int i = 0; i < 100; i++) {
        errorHistory[i] = 0;
    }
}

AdaptivePID::~AdaptivePID() {
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

void AdaptivePID::begin() {
    loadParameters();
    lastTime = millis();
    performanceWindowStart = millis(); // Initialize ML performance window
    Serial.printf("AdaptivePID '%s' initialized with Kp=%.3f, Ki=%.3f, Kd=%.3f\n", 
                  namespace_name.c_str(), kp, ki, kd);
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
    
    // Calculate error (using effective target for ramping)
    float error = effectiveTarget - input;
    
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
    
    // Store error for learning
    errorHistory[errorIndex] = error;
    errorIndex = (errorIndex + 1) % 100;
    
    // Update performance metric (average absolute error)
    float errorSum = 0;
    for (int i = 0; i < 100; i++) {
        errorSum += abs(errorHistory[i]);
    }
    performanceMetric = errorSum / 100.0;
    
    // Adapt parameters based on performance
    adaptParameters();
    
    // Update state
    lastError = error;
    lastOutput = output;
    lastInput = input;
    controlActions++;
    
    return output;
}

void AdaptivePID::adaptParameters() {
    // Simple adaptive algorithm: adjust gains based on error magnitude and trend
    // This runs periodically to fine-tune the PID
    
    static unsigned long lastAdaptTime = 0;
    static const unsigned long ADAPT_INTERVAL = 60000; // Adapt every minute
    
    if (millis() - lastAdaptTime < ADAPT_INTERVAL) return;
    lastAdaptTime = millis();
    
    // Calculate error variance
    float errorMean = 0;
    for (int i = 0; i < 100; i++) {
        errorMean += errorHistory[i];
    }
    errorMean /= 100.0;
    
    float errorVariance = 0;
    for (int i = 0; i < 100; i++) {
        float diff = errorHistory[i] - errorMean;
        errorVariance += diff * diff;
    }
    errorVariance /= 100.0;
    
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
    
    // Query the ML lookup table for optimal gains based on current conditions
    float kp_ml, ki_ml, kd_ml, confidence;
    bool found = mlLogger->getOptimalGains(lastInput, ambientTemp, hourOfDay, season, 
                                          kp_ml, ki_ml, kd_ml, confidence);
    
    if (found && confidence > 0.7f) {
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
        
        Serial.printf("PID '%s': ML adaptation applied (confidence: %.2f%%)\n", 
                     namespace_name.c_str(), confidence * 100.0f);
        Serial.printf("  New gains: Kp=%.3f Ki=%.3f Kd=%.3f\n", kp, ki, kd);
    } else if (found && confidence > 0.5f) {
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
