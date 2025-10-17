# Web Interface pH Calibration Guide

## Overview

The web interface now supports **custom pH calibration** with temperature compensation. You can enter your exact buffer solution values, including:

- Solution pH (from bottle label)
- Solution temperature (measured with thermometer)  
- Reference pH at 25°C (from technical data sheet)

This provides maximum accuracy for your specific calibration solutions.

## Accessing the Calibration Interface

1. **Connect to the ESP32**
   - WiFi network: Your configured SSID
   - Or direct: `aquarium-controller` AP (if WiFi not configured)

2. **Open web browser**
   - Navigate to: `http://<esp32-ip-address>`
   - Or: `http://192.168.4.1` (if using AP mode)

3. **Click the Calibration Tab**
   - Look for 🔬 **pH Calibration** button at the top

## Calibration Interface

### Current Readings Display

At the top of the calibration screen, you'll see live readings:

```
📊 Current Readings
Ambient Temperature: 22.8°C
Water Temperature: 27.2°C
Current pH: 7.05
Status: Not calibrating
```

These update automatically when calibration mode is active.

### Calibration Steps

The interface guides you through the complete process:

1. ✅ **Start Calibration** - Switches to ambient temperature mode
2. 📏 **Measure buffer temperature** - Use a thermometer
3. 🧪 **Enter solution values** - Fill in the forms below
4. 💧 **Rinse between buffers** - Use distilled water
5. 💾 **Save Calibration** - Store to non-volatile memory
6. ✓ **End Calibration** - Return to water temperature mode

## Using the Interface

### Step 1: Start Calibration Mode

Click **🚀 Start Calibration**

**What happens:**
- System switches from water temp to ambient temp
- Status changes to: `CALIBRATING (ambient temp mode)`
- Temperature fields auto-fill with current ambient temperature
- Real-time readings start updating every 500ms

### Step 2: Calibrate Point 1 (Acidic)

#### Default Values
```
Point 1: Acidic Buffer (pH 4.0)
Solution pH (nominal): 4.0
Solution Temperature: 25.0°C (auto-filled with ambient)
Reference pH at 25°C: 4.01
```

#### Customize Your Values

**Example: Using actual buffer at 22.8°C**

1. **Check buffer bottle label**
   - Nominal pH: 4.00
   - Reference pH at 25°C: 4.008 (from technical sheet)

2. **Measure solution temperature**
   - Use thermometer in buffer cup
   - Reading: 22.8°C

3. **Fill in the form:**
   ```
   Solution pH: 4.0        (what you're calibrating for)
   Temperature: 22.8       (actual measured temperature)
   Reference pH: 4.008     (from bottle/data sheet)
   ```

4. **Place probe in buffer**
   - Wait 30-60 seconds for stabilization
   - Watch "Current pH" reading settle

5. **Click "Calibrate Point 1"**
   - Success message appears
   - Point 1 is now calibrated

### Step 3: Calibrate Point 2 (Neutral)

1. **Rinse probe thoroughly**
   - Use distilled or RO water
   - Pat dry (don't wipe)

2. **Place in pH 7.00 buffer**

3. **Fill in form:**
   ```
   Solution pH: 7.0
   Temperature: 22.6       (measure this!)
   Reference pH: 6.998     (from bottle)
   ```

4. **Click "Calibrate Point 2"**

### Step 4: Calibrate Point 3 (Alkaline)

1. **Rinse probe thoroughly**

2. **Place in pH 10.00 buffer**

3. **Fill in form:**
   ```
   Solution pH: 10.0
   Temperature: 23.1       (measure this!)
   Reference pH: 10.012    (from bottle)
   ```

4. **Click "Calibrate Point 3"**

### Step 5: Save Calibration

Click **💾 Save Calibration to NVS**

**What happens:**
- All calibration data saved to non-volatile storage
- Survives power cycles and reboots
- Includes all temperature compensation data

### Step 6: End Calibration Mode

Click **✓ End Calibration**

**What happens:**
- System switches back to water temperature
- Status returns to: `Not calibrating`
- pH readings now use tank water temperature
- Real-time polling stops

## Field Descriptions

### Solution pH (nominal)

**What it is:** The pH value you're calibrating for (4.0, 7.0, or 10.0)

**When to change:**
- Using non-standard buffers (e.g., pH 6.86 instead of 7.0)
- Custom calibration points
- Specialized applications

**Example:**
```
Standard: 4.0
Custom: 4.25 (if using pH 4.25 buffer)
```

### Solution Temperature (°C)

**What it is:** The actual temperature of the buffer solution RIGHT NOW

**How to measure:**
1. Pour buffer into calibration cup
2. Insert thermometer
3. Wait 30 seconds for equilibration
4. Read temperature

**Important:**
- ⚠️ Don't guess or assume room temperature
- ⚠️ Temperature affects pH significantly
- ✓ Measure each buffer (they may differ)
- ✓ Record to 0.1°C precision

**Example:**
```
Measured: 22.8°C (actual thermometer reading)
Not: 25.0°C (assumed)
```

### Reference pH at 25°C (optional)

**What it is:** The exact pH of the buffer at the standard reference temperature (25°C)

**Where to find it:**
- Buffer bottle label
- Technical data sheet
- Manufacturer website
- Certificate of analysis

**When to use:**
- High-precision calibration
- Research applications
- Certified buffers

**When to skip:**
- Standard/economy buffers (leave at default)
- Accuracy ±0.1 pH is acceptable
- Value not provided by manufacturer

**Example:**
```
Standard: 4.01 (generic buffer)
Certified: 4.008 (NIST-traceable)
```

## Auto-Fill Feature

When you click **Start Calibration**, the temperature fields automatically fill with the current ambient temperature:

```
Before Start:
Solution Temperature: 25.0°C (default)

After Start (ambient = 22.8°C):
Solution Temperature: 22.8°C (auto-filled)
```

You can:
- ✓ Use the auto-filled value if accurate
- ✓ Manually adjust if you measured different temperature
- ✓ Update for each buffer (they may differ)

## Real-Time Feedback

While calibrating, the interface shows:

### Live pH Reading
```
Current pH: 4.03
```

Watch this value:
- Should be close to your target pH
- Should stabilize within 30-60 seconds
- Indicates probe is working correctly

### Temperature Display
```
Ambient Temperature: 22.8°C
Water Temperature: 27.2°C
```

- Ambient is used during calibration
- Water is used for normal readings
- Both update every 500ms

### Status Indicator
```
Status: CALIBRATING (ambient temp mode)
```

Shows current operating mode:
- `Not calibrating` - Normal operation (water temp)
- `CALIBRATING` - Calibration mode (ambient temp)

## Success Messages

After each calibration point:

```
✓ Point 1 calibrated: pH 4.00 at 22.8°C
✓ Point 2 calibrated: pH 7.00 at 22.6°C
✓ Point 3 calibrated: pH 10.00 at 23.1°C
✓ Calibration saved
✓ Calibration ended - Switched to water temperature
```

## Error Messages

### Missing Required Fields
```
✗ Calibration failed: Missing required fields: ph, temp
```

**Fix:** Ensure pH and Temperature fields are filled in

### Invalid pH Range
```
✗ Calibration failed: Unknown calibration pH value. Use 4.0, 7.0, or 10.0
```

**Fix:** Use pH values close to 4.0, 7.0, or 10.0 (±0.5)

### Network Error
```
✗ Calibration failed: Network error
```

**Fix:**
- Check WiFi connection
- Verify ESP32 is responding
- Refresh page and try again

## Tips for Best Results

### 1. Temperature Matters Most

**Do:**
- ✓ Measure each buffer temperature
- ✓ Wait for thermal equilibration
- ✓ Use a calibrated thermometer

**Don't:**
- ✗ Assume room temperature
- ✗ Use the same temperature for all buffers
- ✗ Calibrate with hot/cold solutions

### 2. Buffer Quality

**Fresh buffers:**
- Use within expiration date
- Store properly (sealed, cool, dark)
- Don't contaminate (use clean cups)

**Old buffers:**
- May have drifted from nominal pH
- Contaminated buffers give false readings
- When in doubt, use fresh buffer

### 3. Probe Care

**Before calibration:**
- Remove probe from storage solution
- Rinse with distilled water
- Gently shake off excess water

**Between buffers:**
- Rinse thoroughly with distilled water
- Pat dry with lint-free tissue
- Don't wipe (damages membrane)

**After calibration:**
- Rinse with distilled water
- Store in storage solution (not water!)

### 4. Order Matters

**Recommended order:**
1. pH 7.0 (neutral) - Establishes zero point
2. pH 4.0 (acid) - Establishes slope
3. pH 10.0 (base) - Confirms linearity

This order gives best results with 3-point calibration.

### 5. Verification

After calibration:
1. Rinse probe
2. Place back in pH 7.0 buffer
3. Check reading is within ±0.05 pH
4. If not, recalibrate

## Advanced Usage

### Using Non-Standard Buffers

You can calibrate with any pH values:

```
Point 1: pH 4.25 buffer
Solution pH: 4.25
Temperature: 22.5
Reference pH: 4.25 (or actual value)
```

The system accepts pH values within ranges:
- Point 1: 3.5-4.5 (acidic)
- Point 2: 6.5-7.5 (neutral)
- Point 3: 9.5-10.5 (alkaline)

### Two-Point Calibration

For basic accuracy, you can skip Point 3:

1. Calibrate Point 1 (pH 4)
2. Calibrate Point 2 (pH 7)
3. Save (skip Point 3)

Accuracy: ±0.1 pH in range 4-8

### Single-Point Calibration

For quick checks, calibrate only Point 2 (pH 7):

1. Calibrate Point 2 (pH 7)
2. Save

Accuracy: ±0.2 pH in range 6-8

## Serial Monitor Output

While calibrating via web interface, watch Serial Monitor (115200 baud):

```
Web: pH calibration started (using ambient temperature)
Web: Calibrated pH 4.00 at 22.8°C (ref pH: 4.01)
Web: Calibrated pH 7.00 at 22.6°C (ref pH: 7.00)
Web: Calibrated pH 10.00 at 23.1°C (ref pH: 10.01)
Web: pH calibration saved to NVS
pH calibration with temperature compensation saved to NVS
Web: pH calibration ended (switched to water temperature)
```

This confirms:
- Commands received
- Values recorded
- Storage successful

## API Endpoints

For advanced users or automation:

### Start Calibration
```
POST /api/calibrate/start
Response: {"status":"ok"}
```

### Calibrate Point
```
POST /api/calibrate/point
Body: {
  "ph": 4.0,
  "temp": 22.8,
  "refPH": 4.01
}
Response: {"status":"ok"}
```

### End Calibration
```
POST /api/calibrate/end
Response: {"status":"ok"}
```

### Save Calibration
```
POST /api/calibrate/save
Response: {"status":"ok"}
```

### Get Current Readings
```
GET /api/calibrate/voltage
Response: {
  "ambientTemp": 22.8,
  "waterTemp": 27.2,
  "currentPH": 7.05,
  "calibrating": true
}
```

### Reset Calibration
```
POST /api/calibrate/reset
Response: {"status":"ok"}
```

## Troubleshooting

### Readings don't stabilize

**Cause:** Probe not equilibrated

**Fix:**
- Wait longer (60-90 seconds)
- Stir buffer gently
- Check probe is fully submerged
- Ensure reference junction is wet

### Auto-fill temperature seems wrong

**Cause:** Ambient sensor reading incorrectly

**Fix:**
- Check ambient sensor placement
- Verify ambient sensor is working
- Manually enter measured temperature

### Calibration doesn't save

**Cause:** NVS partition full or corrupted

**Fix:**
- Click Reset Calibration
- Try saving again
- Check Serial Monitor for errors

### Web interface freezes

**Cause:** Network issue or ESP32 busy

**Fix:**
- Refresh page
- Check WiFi signal strength
- Restart ESP32 if needed

## Best Practices Summary

✅ **Do:**
- Measure buffer temperatures accurately
- Use fresh, uncontaminated buffers
- Rinse thoroughly between buffers
- Save calibration after all points
- Verify calibration with known buffer

❌ **Don't:**
- Assume temperatures
- Use old or contaminated buffers
- Wipe the pH probe
- Skip the rinse step
- Forget to save calibration

---

**With custom calibration values and temperature compensation, you can achieve ±0.02 pH accuracy! 🎯📊**
