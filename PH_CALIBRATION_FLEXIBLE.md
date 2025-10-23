# Flexible pH Calibration System

## Overview

The pH sensor calibration system now supports **1-point, 2-point, or 3-point calibration**, allowing users to calibrate with whatever buffer solutions they have available.

## Calibration Options

### 1-Point Calibration (pH 7.0 only)
- **Use Case:** You only have neutral buffer solution
- **Accuracy:** Basic offset calibration
- **Method:** Single-point calibration with assumed Nernstian slope (~59.16 mV/pH at 25°C)
- **Best For:** Quick calibration, budget constraints, or maintenance of already-calibrated probes
- **Limitation:** Assumes ideal probe slope, won't correct for probe aging or drift

### 2-Point Calibration (Two of: pH 4.0, 7.0, or 10.0)
- **Use Case:** You have two buffer solutions
- **Accuracy:** Good accuracy for most aquarium applications
- **Method:** Linear interpolation between two known points
- **Recommended Combinations:**
  - **pH 4.0 + 7.0** - Best for acidic to neutral range (most freshwater aquariums)
  - **pH 7.0 + 10.0** - Best for neutral to alkaline range (saltwater/African cichlids)
  - **pH 4.0 + 10.0** - Wide range but less accurate in middle
- **Best For:** Most home aquarium use cases
- **Benefit:** Corrects for probe slope variations

### 3-Point Calibration (pH 4.0 + 7.0 + 10.0)
- **Use Case:** You have all three buffer solutions
- **Accuracy:** Optimal accuracy across full range
- **Method:** Piecewise linear calibration curve
- **Best For:** Professional applications, critical measurements, scientific accuracy
- **Benefit:** Corrects for non-linearities in probe response

## Implementation Details

### Code Changes

**Header File (`PHSensor.h`):**
```cpp
// Track which points have been calibrated
bool acidCalibrated;
bool neutralCalibrated;
bool baseCalibrated;
int numCalibrationPoints;

// New accessor methods
int getCalibrationPointCount() { return numCalibrationPoints; }
bool hasAcidPoint() { return acidCalibrated; }
bool hasNeutralPoint() { return neutralCalibrated; }
bool hasBasePoint() { return baseCalibrated; }
```

**Implementation (`PHSensor.cpp`):**

1. **Constructor:** Initialize new tracking variables
2. **voltageTopH():** Implement different conversion algorithms based on number of points
3. **startCalibration():** Clear all calibration flags and show options
4. **endCalibration():** Mark as calibrated if ≥1 point, show accuracy level
5. **calibratePoint():** Track individual points and increment counter
6. **saveCalibration():** Save point flags to NVS
7. **loadCalibration():** Load and display calibrated points
8. **resetCalibration():** Clear all calibration data

### Conversion Algorithms

**1-Point Calibration:**
```cpp
if (neutralCalibrated) {
    rawPH = 7.0 - ((voltage - neutralVoltage) / 0.165);
} else if (acidCalibrated) {
    rawPH = 4.0 - ((voltage - acidVoltage) / 0.165);
} else {
    rawPH = 10.0 - ((voltage - baseVoltage) / 0.165);
}
// 0.165V = Nernstian slope (~59.16mV/pH) scaled for 3.3V ADC
```

**2-Point Calibration:**
```cpp
if (neutralCalibrated && acidCalibrated) {
    float slope = (7.0 - 4.0) / (neutralVoltage - acidVoltage);
    rawPH = 4.0 + (voltage - acidVoltage) * slope;
}
// Linear interpolation between two points
```

**3-Point Calibration:**
```cpp
if (voltage > neutralVoltage) {
    // Acid to neutral segment
    float slope = (7.0 - 4.0) / (neutralVoltage - acidVoltage);
    rawPH = 4.0 + (voltage - acidVoltage) * slope;
} else {
    // Neutral to base segment
    float slope = (10.0 - 7.0) / (baseVoltage - neutralVoltage);
    rawPH = 7.0 + (voltage - neutralVoltage) * slope;
}
// Piecewise linear curve
```

## Web Interface Updates

### API Endpoint Enhancement

**`/api/calibrate/voltage` now returns:**
```json
{
  "ambientTemp": 24.5,
  "waterTemp": 25.2,
  "currentPH": 7.15,
  "calibrating": true,
  "calibrationPoints": 2,
  "hasAcid": true,
  "hasNeutral": true,
  "hasBase": false
}
```

### UI Features

1. **Calibration Options Banner:**
   - Explains 1-point, 2-point, and 3-point options
   - Helps users decide based on available buffers
   - Color-coded information box

2. **Points Status Display:**
   - Shows "X of 3 calibrated" with quality indicator
   - 0 points: Red "uncalibrated"
   - 1 point: Orange "basic"
   - 2 points: Blue "good"
   - 3 points: Green "optimal"

3. **Individual Point Indicators:**
   - Each calibration card shows:
     - ✓ Calibrated (green border)
     - ○ Not calibrated (gray border)
   - Real-time updates after calibration
   - Visual feedback for completion

4. **Enhanced Feedback:**
   - Live status updates during calibration
   - Success messages show point count
   - Immediate visual confirmation

## Serial Output Examples

### Starting Calibration:
```
Starting pH calibration (using ambient air temperature)...
You can calibrate with 1, 2, or 3 points:
  1-point: pH 7.0 (neutral) - basic offset calibration
  2-point: pH 4.0 + pH 7.0 OR pH 7.0 + pH 10.0 - slope calibration
  3-point: pH 4.0 + pH 7.0 + pH 10.0 - full curve calibration (recommended)
```

### Calibrating Points:
```
✓ Calibrated neutral point: pH 7.00 at 24.5°C = 1.523V (Ref pH: 7.00 at 25.0°C)
  Total points: 1/3

✓ Calibrated acid point: pH 4.00 at 24.3°C = 2.145V (Ref pH: 4.01 at 25.0°C)
  Total points: 2/3
```

### Ending Calibration:
```
Ending pH calibration mode - 2 point(s) calibrated
✓ Calibration complete (switching to water temperature)
  Note: Two-point calibration provides good accuracy
```

### Loading Saved Calibration:
```
pH calibration loaded (2-point) with temp compensation:
  Acid: 2.145V at 24.3°C (ref pH 4.01 at 25.0°C)
  Neutral: 1.523V at 24.5°C (ref pH 7.00 at 25.0°C)
  Age: 5 days since last calibration ✓
```

## User Workflow Examples

### Scenario 1: User Only Has pH 7.0 Buffer
1. Click "Start Calibration"
2. Place probe in pH 7.0 buffer
3. Enter temperature and calibrate Point 2 (neutral)
4. Click "Save Calibration" then "End Calibration"
5. System uses 1-point calibration with assumed slope

### Scenario 2: User Has pH 4.0 and 7.0 Buffers
1. Click "Start Calibration"
2. Place probe in pH 4.0 buffer → Calibrate Point 1
3. Rinse probe, place in pH 7.0 buffer → Calibrate Point 2
4. Click "Save Calibration" then "End Calibration"
5. System uses 2-point linear calibration (ideal for freshwater)

### Scenario 3: User Has All Three Buffers (Recommended)
1. Click "Start Calibration"
2. Calibrate all three points (pH 4.0, 7.0, 10.0)
3. Click "Save Calibration" then "End Calibration"
4. System uses 3-point piecewise calibration (best accuracy)

## Advantages

1. **Flexible:** Works with whatever buffers user has available
2. **User-Friendly:** Clear guidance on which option to choose
3. **Backward Compatible:** Existing 3-point calibration still works
4. **Cost-Effective:** Users don't need to buy all three buffers
5. **Visual Feedback:** Clear status indicators show calibration completeness
6. **Persistent:** Calibration points stored in NVS with flags
7. **Temperature Compensation:** Works with all calibration modes

## Technical Notes

### Nernstian Slope
The 1-point calibration uses the theoretical Nernstian slope:
- **59.16 mV/pH unit at 25°C**
- Scaled for 3.3V ADC: **0.165V per pH unit**
- This is the theoretical response of an ideal pH electrode
- Real probes may vary by ±5%, affecting 1-point accuracy

### Calibration Storage (NVS)
```cpp
prefs->putFloat("acidV", acidVoltage);          // All modes
prefs->putFloat("neutralV", neutralVoltage);    // All modes
prefs->putFloat("baseV", baseVoltage);          // All modes
prefs->putBool("acidCal", acidCalibrated);      // New flag
prefs->putBool("neutralCal", neutralCalibrated);// New flag
prefs->putBool("baseCal", baseCalibrated);      // New flag
prefs->putInt("numPoints", numCalibrationPoints);// New counter
```

### Accuracy Comparison

| Mode | Typical Accuracy | Use Case |
|------|-----------------|----------|
| **Uncalibrated** | ±0.5 pH | Emergency use only |
| **1-Point** | ±0.2 pH | Basic monitoring |
| **2-Point** | ±0.1 pH | Normal aquarium use |
| **3-Point** | ±0.05 pH | Professional/scientific |

## Future Enhancements (Optional)

- [ ] Support custom pH values (not just 4.0, 7.0, 10.0)
- [ ] Probe health diagnostics (slope quality assessment)
- [ ] Automatic mode selection based on calibrated points
- [ ] Calibration reminder based on point count
- [ ] Export/import calibration data

## Files Modified

1. **`include/PHSensor.h`** - Added calibration tracking variables and methods
2. **`src/PHSensor.cpp`** - Implemented flexible calibration logic
3. **`src/WebServer.cpp`** - Enhanced API endpoint with point status
4. **`data/index.html`** - Updated UI with visual indicators and guidance

## Compatibility

- ✅ Backward compatible with existing 3-point calibration workflows
- ✅ Stored calibration data automatically includes new flags
- ✅ Old calibrations will load with `numCalibrationPoints = 0` but still work
- ✅ Web interface gracefully handles missing data (shows "0 of 3")

---

**Summary:** The pH calibration system is now significantly more user-friendly, allowing calibration with 1, 2, or 3 buffer solutions while maintaining accuracy appropriate for each mode. Users can calibrate with what they have, making the system more accessible without sacrificing functionality for those with complete calibration sets.
