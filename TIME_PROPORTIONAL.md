# Time Proportional Relay Control

## Overview

The relay controller now supports **Time Proportional Control** in addition to simple threshold-based on/off control. This provides much finer control over devices like heaters and CO2 solenoids by switching them on and off in proportion to the PID output over a time window.

## How It Works

### Simple Threshold Mode (Default)
- **Threshold**: 50%
- If PID output > 50% → Relay ON
- If PID output ≤ 50% → Relay OFF

**Problem**: This creates aggressive "bang-bang" control with only two states.

### Time Proportional Mode
- **Window**: Configurable time period (default 10 seconds)
- **Duty Cycle**: PID output percentage
- Relay cycles ON/OFF within the window proportional to duty cycle

**Example**: 
- PID output = 35%
- Window = 10 seconds
- Relay is ON for 3.5 seconds, OFF for 6.5 seconds
- Repeats each window

## Benefits

1. **Smoother Control**: Gradual power delivery instead of full on/off
2. **Less Overshoot**: Prevents temperature/pH from overshooting target
3. **Equipment Longevity**: Reduces stress on relays and devices
4. **Better PID Performance**: Allows PID to use full 0-100% output range
5. **Aquarium Safety**: More stable conditions for aquatic life

## Usage

### Basic Setup

```cpp
// Create relay controller
RelayController heaterRelay(26, "Heater");

// Initialize
heaterRelay.begin();

// Enable time proportional mode
heaterRelay.setMode(TIME_PROPORTIONAL);

// Optional: Set custom window size (default is 10 seconds)
heaterRelay.setWindowSize(15000); // 15 second window
```

### In Control Loop

```cpp
void loop() {
    // Get PID output (0-100%)
    float pidOutput = tempPID->compute(currentTemp, dt);
    
    // Set duty cycle
    heaterRelay.setDutyCycle(pidOutput);
    
    // IMPORTANT: Call update() regularly for time proportional mode
    heaterRelay.update();
    
    delay(100); // Call update frequently
}
```

### Switching Modes

```cpp
// Use simple threshold mode
heaterRelay.setMode(SIMPLE_THRESHOLD);

// Use time proportional mode
heaterRelay.setMode(TIME_PROPORTIONAL);

// Check current mode
if (heaterRelay.getMode() == TIME_PROPORTIONAL) {
    Serial.println("Using time proportional control");
}
```

## Configuration

### Window Size Selection

Choose window size based on your application:

| Device | Recommended Window | Reason |
|--------|-------------------|--------|
| Heater | 10-30 seconds | Thermal mass, avoid frequent cycling |
| CO2 Solenoid | 5-15 seconds | Faster response needed |
| Light Dimming | 1-5 seconds | Quick response for visible changes |

```cpp
// Conservative (slow, stable)
heaterRelay.setWindowSize(30000); // 30 seconds

// Balanced (default)
heaterRelay.setWindowSize(10000); // 10 seconds

// Responsive (fast, more switching)
heaterRelay.setWindowSize(5000); // 5 seconds
```

### Duty Cycle Examples

| PID Output | Window | ON Time | OFF Time |
|------------|--------|---------|----------|
| 10% | 10s | 1s | 9s |
| 25% | 10s | 2.5s | 7.5s |
| 50% | 10s | 5s | 5s |
| 75% | 10s | 7.5s | 2.5s |
| 90% | 10s | 9s | 1s |

## Integration with PID

The time proportional control works seamlessly with the adaptive PID controller:

```cpp
// PID computes 0-100% output
float pidOutput = tempPID->compute(currentTemp, dt);

// Time proportional converts this to precise on/off timing
heaterRelay.setDutyCycle(pidOutput);
heaterRelay.update();
```

### PID Tuning Implications

With time proportional control:
- **Lower Kp**: Can use smaller proportional gains
- **Better Integral**: Integral term works more effectively
- **Less Derivative**: May need less derivative action
- **Minimal Overshoot**: Automatically reduces overshoot

## Safety Features

Time proportional mode includes all safety features:

1. **Safety Disable Override**: Emergency stop forces relay OFF regardless of duty cycle
2. **Minimum Toggle Interval**: Prevents excessive relay wear
3. **Bounds Checking**: Duty cycle constrained to 0-100%

```cpp
// Safety disable immediately turns off relay
heaterRelay.safetyDisable();

// Even with high duty cycle, relay stays OFF
heaterRelay.setDutyCycle(100.0);
heaterRelay.update(); // Relay remains OFF

// Re-enable when safe
heaterRelay.safetyEnable();
```

## Performance Characteristics

### Timing Accuracy
- Resolution: ~100ms (depends on update() call frequency)
- Window accuracy: ±1 update cycle
- Duty cycle accuracy: ±1%

### Update Frequency
Call `update()` at least:
- **Minimum**: Every 500ms
- **Recommended**: Every 100-200ms
- **Optimal**: Every 50-100ms

```cpp
// In FreeRTOS task
void controlTask(void* parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(100); // 100ms
    
    while (true) {
        // PID computation
        float output = pid->compute(sensor->read(), dt);
        
        // Update relay
        relay->setDutyCycle(output);
        relay->update();
        
        vTaskDelay(xDelay);
    }
}
```

## Comparison: Simple vs Time Proportional

### Temperature Control Example

**Simple Threshold Mode:**
```
Target: 25.0°C
PID Output: 45% → Relay OFF
Temperature: 24.5°C (too cold!)
PID Output: 55% → Relay ON (full power)
Temperature: 25.5°C (overshoot!)
PID Output: 40% → Relay OFF
... cycling continues ...
```

**Time Proportional Mode:**
```
Target: 25.0°C
PID Output: 45% → Relay ON for 4.5s, OFF for 5.5s
Temperature: slowly rises to 24.9°C
PID Output: 52% → Relay ON for 5.2s, OFF for 4.8s
Temperature: slowly rises to 25.0°C (perfect!)
PID Output: 50% → Relay ON for 5s, OFF for 5s
... stable at target ...
```

## Best Practices

### 1. Start with Default Settings
```cpp
heaterRelay.setMode(TIME_PROPORTIONAL);
// Use default 10 second window initially
```

### 2. Tune Window Size
- Too small: Excessive relay switching, wear
- Too large: Slow response, oscillation
- Start with 10s, adjust based on system response

### 3. Monitor Relay Cycles
```cpp
static int cycleCount = 0;
if (heaterRelay.getState() != previousState) {
    cycleCount++;
    Serial.printf("Relay cycles: %d\n", cycleCount);
}
```

Aim for:
- **< 360 cycles/hour** (6/minute) for relay longevity
- **> 6 cycles/hour** (1/10 minutes) for responsive control

### 4. Combine with Adaptive PID
```cpp
tempPID->begin(); // Load learned parameters
heaterRelay.setMode(TIME_PROPORTIONAL);

// Let PID learn optimal parameters for time proportional control
```

## Troubleshooting

### Relay Chattering (Too Frequent Switching)
```cpp
// Increase window size
heaterRelay.setWindowSize(20000); // 20 seconds
```

### Slow Response
```cpp
// Decrease window size
heaterRelay.setWindowSize(5000); // 5 seconds

// Or increase PID gains
```

### Temperature Oscillation
```cpp
// Increase window size for more stability
heaterRelay.setWindowSize(30000);

// Reduce PID derivative gain
tempPID->setParameters(kp, ki, kd * 0.8);
```

### Relay Not Switching
```cpp
// Check that update() is being called
heaterRelay.update();

// Verify mode is set
Serial.println(heaterRelay.getMode() == TIME_PROPORTIONAL ? "TP" : "Simple");

// Check duty cycle
Serial.printf("Duty cycle: %.1f%%\n", heaterRelay.getDutyCycle());
```

## Technical Details

### Algorithm

```cpp
void updateTimeProportional() {
    unsigned long now = millis();
    unsigned long elapsed = now - windowStartTime;
    
    // Reset window if expired
    if (elapsed >= windowSize) {
        windowStartTime = now;
        elapsed = 0;
    }
    
    // Calculate on-time
    unsigned long onTime = (dutyCycle / 100.0) * windowSize;
    
    // Switch relay based on position in window
    if (elapsed < onTime) {
        on();  // Within ON portion
    } else {
        off(); // Within OFF portion
    }
}
```

### State Machine

```
[Window Start] → [ON Period] → [OFF Period] → [Window Reset] → [repeat]
     0s             0-5s          5-10s           10s
                (for 50% duty)
```

## Testing

Comprehensive tests included in `test/test_time_proportional.cpp`:

```bash
# Run time proportional tests
pio test -e native -f test_time_proportional
```

Tests cover:
- ✅ Various duty cycles (0%, 25%, 50%, 75%, 100%)
- ✅ Window reset logic
- ✅ Different window sizes
- ✅ Precision and bounds checking
- ✅ Mode switching
- ✅ PID integration
- ✅ Safety override

## Migration from Simple Mode

```cpp
// Old code (simple threshold)
heaterRelay.setDutyCycle(pidOutput);

// New code (time proportional)
heaterRelay.setMode(TIME_PROPORTIONAL);
heaterRelay.setDutyCycle(pidOutput);
heaterRelay.update(); // Add this in your loop
```

No other changes needed - fully backward compatible!

## API Reference

```cpp
enum RelayMode {
    SIMPLE_THRESHOLD,    // On/off at 50% threshold
    TIME_PROPORTIONAL    // Proportional switching
};

// Set control mode
void setMode(RelayMode mode);

// Get current mode
RelayMode getMode();

// Set time window (milliseconds)
void setWindowSize(unsigned long sizeMs);

// Set duty cycle (0-100%)
void setDutyCycle(float percentage);

// Update relay state (call regularly!)
void update();

// Get current duty cycle
float getDutyCycle();
```

## Example: Complete Setup

```cpp
#include "RelayController.h"
#include "AdaptivePID.h"

RelayController heater(26, "Heater");
AdaptivePID tempPID("temp-pid");

void setup() {
    // Initialize relay with time proportional control
    heater.begin();
    heater.setMode(TIME_PROPORTIONAL);
    heater.setWindowSize(15000); // 15 second window
    
    // Initialize PID
    tempPID.begin();
    tempPID.setTarget(25.0);
}

void loop() {
    // Read sensor
    float temp = sensor.readTemperature();
    
    // Compute PID
    float output = tempPID.compute(temp, 0.1);
    
    // Control heater
    heater.setDutyCycle(output);
    heater.update();
    
    // Log status
    Serial.printf("Temp: %.1f°C, PID: %.1f%%, Heater: %s\n",
                  temp, output, heater.getState() ? "ON" : "OFF");
    
    delay(100);
}
```

---

**Enjoy smoother, more precise control! 🎛️**
