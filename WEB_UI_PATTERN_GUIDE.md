# 🧠 Pattern Learning Web Interface - Quick Reference

## 📱 Web UI Features

### Access the Pattern Learning Tab
Navigate to: `http://<YOUR-ESP32-IP>` → Click **"🧠 Pattern Learning"** tab

---

## 🎨 Interface Sections

### 1. **System Status Dashboard**
Real-time overview of the pattern learning system:

- **Learning Status**
  - ✅ Active = Patterns established, anomaly detection enabled
  - 📚 Learning = Still collecting baseline data (wait 7+ days)

- **Confidence Meter**
  - 🟢 Green (80-100%) = High confidence, reliable detection
  - 🟡 Yellow (50-79%) = Building confidence
  - 🔴 Red (0-49%) = Not enough data yet

- **Days Learning** = How long system has been collecting data
- **Total Samples** = Number of sensor readings processed

### 2. **🌡️ Seasonal Analysis**
Automatic season detection and 7-day rolling averages:

- **Current Season** with emoji indicator:
  - ❄️ Winter (< 18°C ambient)
  - 🌸 Spring (18-22°C)
  - ☀️ Summer (22-27°C)
  - 🔥 Hot Summer (> 27°C)

- **Environmental Averages**:
  - Ambient temperature (7-day average)
  - Water temperature
  - Average pH

### 3. **⚠️ Recent Anomalies Table**
Shows last 20 detected anomalies with:

- **Time** = When anomaly occurred
- **Type** = Temperature, pH, or TDS
- **Actual** = Measured value
- **Expected** = Pattern prediction
- **Deviation** = How many sigma (σ) from normal
- **Severity** = Color-coded priority:
  - 🟡 **LOW** (2.0-2.9σ) - Minor deviation
  - 🟠 **MEDIUM** (3.0-3.9σ) - Unusual reading
  - 🔴 **HIGH** (4.0-4.9σ) - Very unusual
  - ⚫ **CRITICAL** (≥5.0σ) - Equipment failure likely

**Actions:**
- Click **"Clear History"** to reset anomaly list (patterns remain)

### 4. **📊 24-Hour Pattern Chart**
Visualize learned hourly patterns:

**Controls:**
- **Temperature** button = Show temp patterns
- **pH** button = Show pH patterns  
- **TDS** button = Show TDS patterns

**Chart Shows:**
- Hour-by-hour expected values
- Standard deviation ranges (±σ)
- Number of samples per hour

**Example:**
```
Hour  Value    Range (±StdDev)   Samples
────  ───────  ────────────────  ───────
00:00   24.82          ±0.23       35
06:00   24.56          ±0.31       38
12:00   25.34          ±0.42       42
18:00   25.12          ±0.28       40
```

### 5. **⚙️ Configuration Panel**

#### Quick Toggles:
- ✅ **Enable Pattern Learning**
  - Turn entire system on/off
  - Default: ON
  
- ✅ **Auto Seasonal Adaptation**
  - Automatically detect season changes
  - Suggest PID adjustments
  - Default: ON

- ✅ **Send Anomaly Alerts**
  - Publish anomalies to MQTT
  - Default: ON

#### Sensitivity Thresholds:
Adjust how sensitive anomaly detection is:

**Temperature Threshold** (default: 2.5σ)
- Higher value = Fewer alerts, only major issues
- Lower value = More alerts, catch subtle problems
- Range: 1.5 to 5.0 sigma

**pH Threshold** (default: 2.0σ)
- pH varies more naturally (photosynthesis, respiration)
- Lower default for tighter monitoring

**TDS Threshold** (default: 2.5σ)
- Gradual changes are normal
- Higher threshold reduces false alarms

**Buttons:**
- **Save Configuration** = Apply changes
- **🔄 Reset All Patterns** = Clear everything, start fresh

---

## 🚀 Common Use Cases

### Check If System is Ready
**Goal:** Know when anomaly detection activates

1. Go to Pattern Learning tab
2. Check **"Learning Status"**:
   - ✅ Active = Ready!
   - 📚 Learning = Wait a few more days
3. Check **"Confidence"**:
   - Need 80%+ for reliable detection
   - < 50% = Keep collecting data

---

### Review Recent Anomalies
**Goal:** Understand what triggered alerts

1. Scroll to **"Recent Anomalies"** table
2. Look at **Severity** column:
   - Multiple CRITICAL = Equipment problem
   - Occasional LOW/MEDIUM = Normal variation
3. Check **Type**:
   - All same type = Specific sensor/equipment issue
   - Mixed types = Environmental change or calibration drift

---

### Reduce False Alarms
**Goal:** Stop getting alerts for normal events (e.g., feeding time pH drops)

**Option 1: Increase Thresholds**
1. Go to Configuration section
2. Increase threshold for problematic sensor:
   - pH drops after feeding → Increase "pH Threshold" to 2.5σ
   - Temperature varies with room → Increase "Temp Threshold" to 3.0σ
3. Click **"Save Configuration"**

**Option 2: Wait Longer**
- System learns patterns over time
- After 30 days, it will know "pH always drops at noon"
- False alarms reduce automatically

---

### Understand Daily Patterns
**Goal:** See how your tank behaves throughout the day

1. Click **"Temperature"** button in Pattern Chart
2. Observe pattern:
   - Low point = Usually early morning (6 AM)
   - High point = Afternoon (2-4 PM)
   - Variation = How stable your environment is
3. Switch to **"pH"**:
   - Rises during lights-on (photosynthesis)
   - Drops during lights-off (respiration)
4. Switch to **"TDS"**:
   - Gradual increase = Normal (waste accumulation)
   - Sudden spike = Overfeeding or dead organism

---

### Troubleshoot Sensor Issues
**Goal:** Verify sensor accuracy

1. Check Pattern Chart for sensor in question
2. Compare current reading to expected pattern
3. Large difference every hour = Sensor drift/calibration needed

**Example:**
```
pH Pattern shows: 6.85 at 2 PM
Current reading:  7.20 at 2 PM
Difference: +0.35 (consistently high)
Action: Recalibrate pH probe
```

---

### Start Fresh After Changes
**Goal:** Reset patterns after major tank changes

**When to Reset:**
- Changed tank location (different room temperature)
- Major equipment replacement (new heater, filter)
- Changed lighting schedule
- Added/removed significant fish load

**How to Reset:**
1. Go to Configuration section
2. Click **"🔄 Reset All Patterns"**
3. Confirm warning
4. System starts learning from scratch (7-14 day period)

---

## 📊 Interpreting the Data

### What "Good" Looks Like:
- ✅ Confidence: 85-100%
- ✅ Days Learning: 14+
- ✅ Total Samples: 2000+
- ✅ Recent Anomalies: 0-2 per week
- ✅ Pattern Chart: Smooth curves, small std deviations

### Warning Signs:
- ⚠️ Confidence < 70% after 2+ weeks = Unstable environment
- ⚠️ Many HIGH/CRITICAL anomalies = Equipment problem
- ⚠️ Large std deviations = Inconsistent control
- ⚠️ Patterns not matching reality = Recalibrate sensors

---

## 💡 Pro Tips

### 1. Monitor During First Week
- Check anomalies daily
- Adjust thresholds if too many false alarms
- Let system learn your tank's personality

### 2. Use Charts to Optimize Schedule
- See when temp is lowest → Schedule water changes
- See when pH peaks → Optimize CO2 injection timing
- See TDS trends → Plan maintenance

### 3. Seasonal Transitions
- Watch for season change notifications
- Spring/Fall = Most stable periods
- Summer/Winter = May need PID tuning

### 4. Compare Before/After Equipment Changes
1. Note pattern before changing equipment
2. Make change
3. Compare new pattern after 7 days
4. Verify improvement or diagnose issues

### 5. Export Data (Future Feature)
Currently view-only, but patterns are stored in ESP32 NVS:
- Survives restarts/power loss
- Can be queried via API for custom logging

---

## 🔧 Troubleshooting

### "Patterns Not Establishing"
**Symptoms:** Status stuck on "📚 Learning" for 2+ weeks

**Possible Causes:**
1. **Not enough data** = Check "Total Samples"
   - Need 168+ samples minimum (7 days)
   - Should have 20+ samples per hour
2. **Extremely variable environment**
   - Large room temperature swings
   - Inconsistent control
   - Fix underlying stability issues first

**Solution:**
- Wait longer (up to 30 days for very variable tanks)
- Improve environmental stability (room temperature, lighting)

---

### "Too Many Anomalies"
**Symptoms:** Anomaly table fills up quickly, many LOW/MEDIUM alerts

**Solution:**
1. **Check confidence level**
   - < 80% = Still learning, expect false positives
2. **Increase thresholds**
   - Temp: 2.5σ → 3.0σ
   - pH: 2.0σ → 2.5σ
3. **Wait for learning**
   - Patterns refine over time
   - False alarms decrease naturally

---

### "No Anomalies Ever Detected"
**Symptoms:** Table always empty, even during known problems

**Possible Causes:**
1. **Thresholds too high**
   - System only alerts on 5σ+ events (extremely rare)
2. **Patterns not established**
   - Check "Learning Status"
3. **Anomaly detection disabled**
   - Check "Send Anomaly Alerts" toggle

**Solution:**
1. Lower thresholds to 2.0σ for testing
2. Verify patterns established
3. Ensure detection enabled in config

---

### "Chart Shows Unexpected Pattern"
**Example:** "Temperature should be stable, but chart shows 2°C swing"

**This is GOOD!**
- Chart shows what's **actually happening**
- You just discovered your tank's real behavior
- Options:
  1. **Accept it** = Normal for your environment
  2. **Investigate** = Room temperature affecting tank?
  3. **Improve** = Better heater, insulation, PID tuning

---

## 🎯 Success Metrics

After 30 days, aim for:
- ✅ Confidence > 90%
- ✅ < 1 anomaly per day
- ✅ StdDev < 0.3°C for temp
- ✅ StdDev < 0.15 for pH
- ✅ Seasonal detection working
- ✅ Smooth, predictable patterns

---

## 📞 Quick Reference Card

| Task | Steps |
|------|-------|
| **Check if ready** | Pattern tab → Status = "✅ Active" |
| **View anomalies** | Pattern tab → Scroll to table |
| **See daily pattern** | Pattern tab → Click Temp/pH/TDS button |
| **Reduce alerts** | Config → Increase thresholds → Save |
| **Reset learning** | Config → Reset All Patterns → Confirm |
| **Check season** | Seasonal Analysis section → Icon + name |
| **Verify confidence** | Status Dashboard → Confidence meter |

---

**Built with ❤️ for smarter aquarium monitoring!** 🐠🧠

Access full documentation: [PATTERN_LEARNING.md](PATTERN_LEARNING.md)
