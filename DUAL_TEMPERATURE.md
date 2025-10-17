# Dual Temperature System: Water & Ambient Sensors

## Overview

The aquarium controller now includes **two separate temperature sensors**:

1. **Water Temperature Sensor** (GPIO 4) - Monitors tank water temperature
2. **Ambient Temperature Sensor** (GPIO 5) - Monitors air/room temperature

This dual-sensor setup provides critical benefits for accurate pH measurement and calibration.

## Why Two Temperature Sensors?

### Problem: Temperature-Dependent pH Calibration

When you calibrate your pH sensor:
- You're typically calibrating in **room temperature** buffer solutions (not in the tank)
- Buffer solutions are at **ambient/air temperature** (20-25°C typically)
- Your tank water might be at a **different temperature** (26-28°C for tropical fish)

**Without ambient sensor:**
- You'd record the wrong temperature during calibration
- pH readings would have systematic errors
- Temperature compensation would be incorrect

**With ambient sensor:**
- ✅ Calibration uses actual buffer solution temperature (ambient)
- ✅ Normal readings use actual tank water temperature
- ✅ Temperature compensation is accurate in both cases

## How It Works

### During pH Calibration

When you start pH calibration:

```cpp
phSensor->startCalibration();  // Sets isCalibrating = true
```

The pH sensor **automatically uses ambient temperature**:

```cpp
// Internal logic in PHSensor::readPH()
float tempToUse = isCalibrating ? ambientTemp : waterTemp;
```

**Calibration workflow:**
1. Place pH probe in buffer solution (at room temperature ~23°C)
2. System reads **ambient sensor** (23°C) ← Correct!
3. Records voltage and temperature together
4. Moves to next buffer solution
5. Repeats for all calibration points

### During Normal Operation

After calibration is complete:

```cpp
phSensor->endCalibration();  // Sets isCalibrating = false
```

The pH sensor **automatically uses water temperature**:

```cpp
// Normal operation
float ph = phSensor->readPH(waterTemp, ambientTemp);
// Uses waterTemp internally since not calibrating
```

**Normal operation workflow:**
1. System reads **water sensor** (26°C)
2. System reads **ambient sensor** (22°C) for reference
3. pH sensor uses **water temp** for temperature compensation
4. Returns accurate pH for current tank conditions

## Sensor Placement

### Water Temperature Sensor (GPIO 4)

**Location:** Fully submerged in aquarium water

```
┌─────────────────────────┐
│                         │
│  Aquarium               │
│                         │
│     🌡️ ← Water temp    │
│     sensor              │
│     (submerged)         │
│                         │
└─────────────────────────┘
```

**Purpose:**
- Monitor actual tank water temperature
- Used for heater PID control
- Used for pH temperature compensation during normal readings
- Used for TDS temperature compensation

### Ambient Temperature Sensor (GPIO 5)

**Location:** Near the aquarium but NOT in water

```
┌─────────────────────────┐
│ 🌡️                      │ ← Ambient sensor
│ (in air, near tank)     │    mounted outside
│                         │
│  Aquarium               │
│                         │
│     🌡️                  │
│     (in water)          │
│                         │
└─────────────────────────┘
```

**Purpose:**
- Monitor room/air temperature
- Used during pH calibration (buffer solutions at room temp)
- Provides environmental monitoring
- Can detect room HVAC issues

**Best mounting locations:**
- On the side of the aquarium stand
- Near the equipment but not directly on heated devices
- Protected from water splashes
- At similar height to pH calibration cups

## Calibration Example

### Before (Single Sensor - Incorrect)

```
Calibrating pH 7.00 buffer:
  Buffer solution actual temp: 22°C
  ESP32 reads tank water: 27°C ❌ WRONG!
  Records: pH 7.00 @ 27°C
  
Later, reading tank pH:
  Tank water: 27°C
  Applies compensation for (27-27=0°C) ← No correction
  But calibration was actually at 22°C!
  pH reading ERROR: ~0.15 pH units off
```

### After (Dual Sensor - Correct)

```
Calibrating pH 7.00 buffer:
  phSensor->startCalibration()  // Switches to ambient sensor
  Buffer solution actual temp: 22°C
  ESP32 reads ambient air: 22°C ✓ CORRECT!
  Records: pH 7.00 @ 22°C
  
Later, reading tank pH:
  phSensor->endCalibration()  // Switches to water sensor
  Tank water: 27°C
  Applies compensation for (27-22=5°C)
  pH reading: Accurate! ✓
```

## API Usage

### Initialization

```cpp
// In main.cpp setup()
tempSensor = new TemperatureSensor(4);      // Water sensor on GPIO 4
ambientSensor = new AmbientTempSensor(5);   // Ambient sensor on GPIO 5

tempSensor->begin();
ambientSensor->begin();
```

### Automatic Temperature Selection

The system **automatically** selects the correct temperature:

```cpp
// In sensor task (runs continuously)
float waterTemp = tempSensor->readTemperature();
float ambientTemp = ambientSensor->readTemperature();

// pH sensor automatically chooses:
// - ambientTemp during calibration
// - waterTemp during normal operation
float ph = phSensor->readPH(waterTemp, ambientTemp);
```

### Calibration Workflow

```cpp
// Start calibration
phSensor->startCalibration();  // Now uses ambient temp

// Calibrate pH 7
phSensor->calibratePoint(7.0, ambientTemp);
// Note: ambientTemp is passed explicitly for display/logging
// but internally readPH() already uses it

// Calibrate pH 4
phSensor->calibratePoint(4.0, ambientTemp);

// Calibrate pH 10
phSensor->calibratePoint(10.0, ambientTemp);

// Save calibration
phSensor->saveCalibration();

// End calibration (switch back to water temp)
phSensor->endCalibration();  // Now uses water temp for readings
```

### Check Calibration Mode

```cpp
if (phSensor->inCalibrationMode()) {
    Serial.println("Using ambient temperature for pH");
} else {
    Serial.println("Using water temperature for pH");
}
```

## Serial Monitor Output

### During Calibration

```
[SENSORS] Water: 27.2°C, Ambient: 22.8°C, pH: 7.05, TDS: 245 ppm
Starting pH calibration (using ambient air temperature)...
Calibrated neutral point: pH 7.00 at 22.8°C = 1.523V (Ref pH: 7.00 at 25.0°C)
Calibrated acid point: pH 4.00 at 22.6°C = 2.156V (Ref pH: 4.01 at 25.0°C)
Calibrated base point: pH 10.00 at 23.1°C = 0.892V (Ref pH: 10.01 at 25.0°C)
pH calibration with temperature compensation saved to NVS
Ending pH calibration mode (switching to water temperature)
```

### Normal Operation

```
[SENSORS] Water: 27.2°C, Ambient: 22.8°C, pH: 6.85, TDS: 245 ppm
[CONTROL] Temp PID: 45.2%, CO2 PID: 32.1%, Heater: ON, CO2: ON
```

## MQTT Topics

Both temperatures are published separately:

```
aquarium/temperature → 27.2          (water temperature)
aquarium/ambient_temperature → 22.8  (air temperature)
aquarium/ph → 6.85
aquarium/tds → 245
```

## Temperature Ranges

### Water Temperature
- **Typical range:** 22-30°C (tropical aquariums)
- **Validation range:** 15-40°C
- **Read interval:** 1 second (thermal mass requires slower averaging)
- **Moving average:** 10 samples

### Ambient Temperature
- **Typical range:** 18-28°C (room temperature)
- **Validation range:** 10-45°C
- **Read interval:** 5 seconds (air temp changes slowly)
- **Moving average:** 5 samples

## Troubleshooting

### Ambient sensor not found

```
WARNING: Ambient temperature sensor initialization failed!
```

**Solutions:**
1. Check wiring to GPIO 5
2. Verify 4.7kΩ pullup resistor is installed
3. Test sensor with simple 1-Wire scan
4. Ensure sensor is not submerged (water can damage it)

### Temperatures seem swapped

**Symptoms:**
- Water temp reads like room temp (22°C)
- Ambient temp reads like tank temp (27°C)

**Solution:**
- Sensors are physically swapped
- Swap the GPIO pins in configuration or swap physical connections

### pH calibration still inaccurate

**Check:**
1. Verify `startCalibration()` was called
2. Measure actual buffer temperature with separate thermometer
3. Ensure ambient sensor is near calibration cups
4. Buffer solutions should be within 5°C of ambient sensor reading

### Large temperature difference between sensors

**Normal:**
- Water: 27°C, Ambient: 22°C (5°C difference) ✓
- Heater is working to maintain tank temperature

**Abnormal:**
- Water: 27°C, Ambient: 15°C (12°C difference)
- Check ambient sensor placement
- May indicate room HVAC issues

## Web Interface Updates

The web interface displays both temperatures:

```html
<div class="sensor-card">
  <h3>Water Temperature</h3>
  <div class="value">27.2°C</div>
  <div class="target">Target: 25.0°C</div>
</div>

<div class="sensor-card">
  <h3>Ambient Temperature</h3>
  <div class="value">22.8°C</div>
  <div class="info">Room temperature</div>
</div>

<div class="sensor-card">
  <h3>pH</h3>
  <div class="value">6.85</div>
  <div class="info">
    Using: Water temp (27.2°C)
    <!-- During calibration: Using: Ambient temp (22.8°C) -->
  </div>
</div>
```

## Benefits Summary

✅ **Accurate pH calibration** - Uses actual buffer solution temperature
✅ **Accurate pH readings** - Uses actual tank water temperature  
✅ **Temperature monitoring** - Track both tank and room conditions
✅ **HVAC monitoring** - Detect room temperature problems
✅ **Automatic switching** - No manual intervention needed
✅ **Data logging** - Both temperatures available via MQTT
✅ **Future expansion** - Can add room temp alerts or control

## Technical Implementation

### SensorData Structure

```cpp
struct SensorData {
    float temperature;      // Water temperature
    float ambientTemp;      // Air/room temperature
    float ph;
    float tds;
    // ... other fields
};
```

### Temperature Selection Logic

```cpp
// In PHSensor::readPH(float waterTemp, float ambientTemp)
float tempToUse = isCalibrating ? ambientTemp : waterTemp;
float ph = voltageTopH(voltage, tempToUse);
```

### Calibration Flag

```cpp
private:
    bool isCalibrating;

public:
    void startCalibration() { isCalibrating = true; }
    void endCalibration() { isCalibrating = false; }
    bool inCalibrationMode() { return isCalibrating; }
```

## Hardware Requirements

### Additional Parts Needed

- **1x DS18B20 temperature sensor** (for ambient temp)
- **1x 4.7kΩ resistor** (pullup for new sensor)
- **Wire** to route ambient sensor outside tank

### Cost

- DS18B20 sensor: ~$2-5 USD
- Resistor: ~$0.10 USD
- Total additional cost: **~$5 USD**

### Installation Time

- ~10 minutes to wire additional sensor
- ~5 minutes to mount sensor near tank
- Total: **~15 minutes**

## Future Enhancements

Possible additions with dual temperature sensors:

1. **Room temperature alerts**
   - Alert if room gets too cold (HVAC failure)
   - Alert if room gets too hot (fire risk)

2. **Heater efficiency monitoring**
   - Track water-to-ambient temperature delta
   - Detect poorly insulated tanks
   - Calculate heating costs

3. **Seasonal adjustments**
   - Adjust PID parameters based on ambient temp
   - Reduce heating in summer

4. **Equipment cooling**
   - Monitor if equipment area is overheating
   - Alert if ambient temp near equipment is high

---

**With dual temperature sensors, you get research-grade pH accuracy! 🌡️🌡️🎯**
