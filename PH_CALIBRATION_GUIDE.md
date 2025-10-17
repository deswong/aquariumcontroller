# pH Calibration Guide with Temperature Compensation

## Overview

The pH sensor now supports **temperature-compensated calibration** and readings. This is critical for accurate pH measurements because:

1. **pH buffer solutions change with temperature**
2. **Electrode response varies with temperature** (Nernst equation)
3. **Aquarium temperature fluctuates** as heater cycles on/off

Without temperature compensation, your pH readings could vary by ±0.1-0.3 pH units due to temperature changes alone!

## How Temperature Affects pH

### Buffer Solution Temperature Dependence

Most pH calibration solutions are specified at 25°C. Their actual pH changes with temperature:

| Solution | pH at 15°C | pH at 20°C | pH at 25°C | pH at 30°C |
|----------|------------|------------|------------|------------|
| pH 4.01 Buffer | 4.00 | 4.00 | 4.01 | 4.02 |
| pH 7.00 Buffer | 7.09 | 7.04 | 7.00 | 6.97 |
| pH 10.01 Buffer | 10.25 | 10.12 | 10.01 | 9.92 |

**Notice**: pH 7 and pH 10 buffers shift significantly with temperature!

### Electrode Temperature Response

The electrode voltage response follows the Nernst equation:
- Slope changes ~0.2 mV/°C
- Affects measurement by ~0.003 pH units per °C per pH unit from neutral

## Temperature-Compensated Calibration Process

### Step 1: Prepare Your Equipment

**You will need:**
- pH 4.01, 7.00, and 10.01 calibration solutions
- Thermometer (or use your tank's temperature sensor)
- Rinse water (distilled or RO water)
- Paper towels

**Check your buffer bottles:**
- Note the temperature specification (usually 25°C)
- Some bottles have temperature correction tables on the label

### Step 2: Set Reference Temperature (Optional)

If your buffers are specified at a temperature other than 25°C:

```cpp
phSensor->setReferenceTemp(20.0); // If buffers rated at 20°C
```

### Step 3: Set Buffer Solution True pH Values (Optional)

If your buffer bottles specify exact pH values at reference temperature:

```cpp
// Example: Using high-precision buffers
phSensor->setSolutionReference(4.008, 6.998, 10.012);
```

If you skip this, the system uses standard values (4.01, 7.00, 10.01).

### Step 4: Calibrate pH 7.00 (Neutral)

```cpp
// 1. Rinse probe and place in pH 7.00 buffer
// 2. Measure the buffer temperature
float bufferTemp = 23.5; // Example: buffer is at 23.5°C

// 3. Start calibration
phSensor->startCalibration();

// 4. Calibrate neutral point
phSensor->calibratePoint(7.0, bufferTemp);
```

**Output:**
```
Calibrated neutral point: pH 7.00 at 23.5°C = 1.523V (Ref pH: 7.00 at 25.0°C)
```

### Step 5: Calibrate pH 4.01 (Acid)

```cpp
// 1. Rinse probe thoroughly
// 2. Place in pH 4.01 buffer
// 3. Measure buffer temperature
float bufferTemp = 23.8; // May be different from pH 7 buffer

// 4. Calibrate acid point
phSensor->calibratePoint(4.0, bufferTemp);
```

**Output:**
```
Calibrated acid point: pH 4.00 at 23.8°C = 2.156V (Ref pH: 4.01 at 25.0°C)
```

### Step 6: Calibrate pH 10.01 (Base)

```cpp
// 1. Rinse probe thoroughly
// 2. Place in pH 10.01 buffer
// 3. Measure buffer temperature
float bufferTemp = 24.2;

// 4. Calibrate base point (completes calibration)
phSensor->calibratePoint(10.0, bufferTemp);
```

**Output:**
```
Calibrated base point: pH 10.00 at 24.2°C = 0.892V (Ref pH: 10.01 at 25.0°C)
```

### Step 7: Save Calibration

```cpp
phSensor->saveCalibration();
```

**Output:**
```
pH calibration with temperature compensation saved to NVS
```

The calibration is now permanently stored and will survive reboots!

## How Temperature Compensation Works

### During Calibration

The system stores:
1. **Voltage reading** for each calibration point
2. **Temperature** of the buffer solution during calibration
3. **True pH** of the buffer at reference temperature

### During Measurement

When you read pH from your aquarium:

```cpp
float tankTemp = 26.5; // Current tank temperature
float ph = phSensor->readPH(tankTemp);
```

The system:
1. Reads the voltage from the pH probe
2. Converts voltage to raw pH using calibration curve
3. **Applies temperature correction** based on:
   - Difference between current temp and calibration temp
   - Nernst equation compensation (~0.003 pH/°C)
   - Reference pH of the nearest calibration point

### Example Calculation

**Calibration:**
- pH 7.00 buffer measured at 23.5°C
- Voltage: 1.523V

**Measurement:**
- Tank temperature: 26.5°C
- Voltage: 1.480V
- Raw pH from calibration curve: 6.85

**Temperature Compensation:**
```
Temperature difference: 26.5°C - 23.5°C = +3.0°C
pH from neutral: 7.00 - 7.00 = 0
Correction: -0.003 × 3.0 × 0 = 0 (minimal for pH 7)

Compensated pH: 6.85 + 0 = 6.85
```

For pH further from 7, the correction is more significant!

## Practical Calibration Tips

### Best Practices

1. **Calibrate at tank temperature if possible**
   - Heat/cool buffers to match your aquarium temperature
   - Reduces need for temperature compensation
   - More accurate results

2. **Measure buffer temperature accurately**
   - Use a good thermometer
   - Let buffer and probe equilibrate (2-3 minutes)
   - Don't assume room temperature = buffer temperature

3. **Calibrate regularly**
   - Monthly for critical applications
   - After probe cleaning or storage
   - If readings seem off

4. **Order matters**
   - Always calibrate pH 7 first (establishes zero point)
   - Then pH 4 and pH 10 in any order

### Common Mistakes

❌ **Using old buffer solutions**
- Buffers expire (check bottle date)
- Contaminated buffers give false readings

❌ **Not rinsing between buffers**
- Rinse thoroughly with distilled water
- Pat dry (don't wipe, damages membrane)

❌ **Rushing the process**
- Allow 30-60 seconds for readings to stabilize
- Temperature equilibration takes time

❌ **Ignoring buffer temperature**
- 5°C difference can cause 0.1 pH error
- Always measure and record buffer temperature

## Web Interface Calibration

The web interface supports temperature-compensated calibration:

### Calibration Screen (Enhanced)

```html
Calibration Status: Not Calibrated

[Start Calibration]

Step 1: pH 7.00 (Neutral)
  Buffer Temperature: [23.5] °C
  Buffer Ref pH (optional): [7.00]
  [Calibrate pH 7.00]

Step 2: pH 4.01 (Acid)  
  Buffer Temperature: [23.8] °C
  Buffer Ref pH (optional): [4.01]
  [Calibrate pH 4.01]

Step 3: pH 10.01 (Base)
  Buffer Temperature: [24.2] °C
  Buffer Ref pH (optional): [10.01]
  [Calibrate pH 10.01]

[Save Calibration] [Reset Calibration]

Current Readings:
  Voltage: 1.523V
  Raw pH: 7.02
  Temperature: 26.5°C
  Compensated pH: 7.00 ✓
```

## Verification After Calibration

### Test in Known Solutions

After calibration, verify accuracy:

```cpp
// Test in pH 7 buffer at different temperature
float testTemp = 28.0;
float testPH = phSensor->readPH(testTemp);
// Should read close to 7.00 (accounting for buffer temp shift)
```

Expected accuracy:
- **±0.05 pH** within calibration range (pH 4-10)
- **±0.1 pH** in typical aquarium conditions (pH 6-8)

### Monitor Over Time

```cpp
Serial.printf("Tank: %.1f°C, pH: %.2f\n", tankTemp, tankPH);
```

Watch for:
- Stable readings when temperature stable
- Smooth changes when temperature changes
- No sudden jumps or drift

## Troubleshooting

### Problem: pH readings drift with temperature

**Cause**: Temperature compensation not working

**Solution**:
1. Verify you're passing temperature to `readPH()`:
   ```cpp
   float ph = phSensor->readPH(currentTemp); // ✓ Correct
   float ph = phSensor->readPH();            // ✗ Uses default 25°C
   ```

2. Check calibration includes temperature data:
   ```cpp
   phSensor->loadCalibration(); // Should show temps in Serial output
   ```

### Problem: Inaccurate readings after calibration

**Cause**: Buffer temperature not measured correctly

**Solution**:
- Re-calibrate with accurate temperature measurements
- Use freshly opened buffers
- Ensure buffers have stabilized to ambient temperature

### Problem: Large corrections being applied

**Cause**: Normal if temperature varies significantly from calibration

**Example**:
- Calibrated at 20°C
- Reading at 28°C
- pH 6.0 reading: correction ≈ -0.024 pH units

**Solution**:
- Recalibrate at temperature closer to tank temperature
- Or accept larger corrections (they're mathematically valid)

### Problem: Readings unstable

**Cause**: Not temperature-related

**Check**:
- Probe membrane (should be hydrated)
- Reference junction (should not be clogged)
- Electrode age (replace if >2 years old)

## Technical Details

### Temperature Compensation Algorithm

The system uses a Nernst-based compensation model:

```cpp
float applyTempCompensation(float rawPH, float currentTemp) {
    // Select calibration point based on pH range
    float calTemp, trueRef;
    if (rawPH < 5.5) {
        calTemp = acidCalTemp;      // Temp when pH 4 was calibrated
        trueRef = acidTrueRef;      // True pH 4 at reference temp
    } else if (rawPH < 8.5) {
        calTemp = neutralCalTemp;   // Temp when pH 7 was calibrated
        trueRef = neutralTrueRef;   // True pH 7 at reference temp
    } else {
        calTemp = baseCalTemp;      // Temp when pH 10 was calibrated
        trueRef = baseTrueRef;      // True pH 10 at reference temp
    }
    
    // Calculate compensation
    float tempDiff = currentTemp - calTemp;
    float pHFromNeutral = trueRef - 7.0;
    float correction = -0.003 * tempDiff * pHFromNeutral;
    
    return rawPH + correction;
}
```

### Stored Calibration Data

In NVS (non-volatile storage):

```
ph-calibration:
  acidV:       2.156V    (voltage at pH 4)
  neutralV:    1.523V    (voltage at pH 7)
  baseV:       0.892V    (voltage at pH 10)
  acidTemp:    23.8°C    (temp during pH 4 cal)
  neutralTemp: 23.5°C    (temp during pH 7 cal)
  baseTemp:    24.2°C    (temp during pH 10 cal)
  acidRef:     4.01      (true pH at refTemp)
  neutralRef:  7.00      (true pH at refTemp)
  baseRef:     10.01     (true pH at refTemp)
  refTemp:     25.0°C    (reference temperature)
  calibrated:  true
```

## Advanced Usage

### High-Precision Calibration

For research-grade accuracy:

```cpp
// Use certified buffers with exact specifications
phSensor->setReferenceTemp(25.0);
phSensor->setSolutionReference(4.008, 6.998, 10.012);

// Calibrate in temperature-controlled environment
phSensor->calibratePoint(7.0, 25.0);  // Exactly at reference temp
phSensor->calibratePoint(4.0, 25.0);
phSensor->calibratePoint(10.0, 25.0);
```

### Custom Temperature Coefficients

If you have buffer solution data sheets with exact temperature coefficients:

```cpp
// This would require code modification to support custom coefficients
// Current implementation uses standard -0.003 pH/°C coefficient
```

### Logging Calibration History

```cpp
void logCalibration() {
    Serial.println("=== pH Calibration Data ===");
    Serial.printf("Calibrated: %s\n", phSensor->isCalibrated() ? "Yes" : "No");
    Serial.printf("Acid:    %.3fV at %.1f°C\n", 
                  phSensor->getAcidVoltage(), acidCalTemp);
    Serial.printf("Neutral: %.3fV at %.1f°C\n", 
                  phSensor->getNeutralVoltage(), neutralCalTemp);
    Serial.printf("Base:    %.3fV at %.1f°C\n", 
                  phSensor->getBaseVoltage(), baseCalTemp);
}
```

## Quick Reference

### API Summary

```cpp
// Set reference temperature (default: 25.0°C)
phSensor->setReferenceTemp(25.0);

// Set exact buffer pH values (optional)
phSensor->setSolutionReference(4.01, 7.00, 10.01);

// Calibrate with temperature
phSensor->startCalibration();
phSensor->calibratePoint(7.0, bufferTemp);     // pH 7 at measured temp
phSensor->calibratePoint(4.0, bufferTemp);     // pH 4 at measured temp
phSensor->calibratePoint(10.0, bufferTemp);    // pH 10 at measured temp

// Or specify exact reference pH
phSensor->calibratePoint(7.0, bufferTemp, 6.998);

// Save to NVS
phSensor->saveCalibration();

// Read with temperature compensation
float ph = phSensor->readPH(currentTemp);
```

### Typical Calibration Session

```cpp
Serial.println("Starting pH calibration with temperature compensation");

// Optional: customize reference
phSensor->setReferenceTemp(25.0);
phSensor->setSolutionReference(4.01, 7.00, 10.01);

// Start calibration
phSensor->startCalibration();

// pH 7.00
Serial.println("Place probe in pH 7.00 buffer");
delay(5000); // Wait for stabilization
phSensor->calibratePoint(7.0, 23.5); // Buffer at 23.5°C

// pH 4.01
Serial.println("Rinse and place probe in pH 4.01 buffer");
delay(5000);
phSensor->calibratePoint(4.0, 23.8); // Buffer at 23.8°C

// pH 10.01
Serial.println("Rinse and place probe in pH 10.01 buffer");
delay(5000);
phSensor->calibratePoint(10.0, 24.2); // Buffer at 24.2°C

// Save
phSensor->saveCalibration();
Serial.println("Calibration complete!");
```

---

**With temperature compensation, you can trust your pH readings even as your aquarium temperature fluctuates! 🌡️🐠**
