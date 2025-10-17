# 🧠 Pattern Learning System Documentation

## Overview

The Pattern Learning system adds intelligent anomaly detection and seasonal adaptation to your aquarium controller using statistical machine learning. It learns your tank's normal patterns over time and alerts you to unusual conditions that may indicate equipment problems or environmental issues.

## 🎯 Key Features

### 1. **Hourly Pattern Learning**
- Tracks temperature, pH, and TDS for each hour of the day
- Builds statistical models (mean ± standard deviation)
- Adapts to your tank's natural daily cycles

### 2. **Anomaly Detection**
- Detects unusual sensor readings based on learned patterns
- Severity levels: Low, Medium, High, Critical
- Configurable sensitivity thresholds (sigma multipliers)

### 3. **Seasonal Adaptation**
- Automatically detects seasonal changes (winter/spring/summer/fall)
- Suggests PID parameter adjustments for optimal control
- Tracks 7-day rolling averages

### 4. **Smart Alerting**
- Context-aware anomaly detection reduces false alarms
- Distinguishes normal variations from real problems
- MQTT integration for alerts

---

## 🚀 Quick Start

### Initial Setup

The pattern learner is **enabled by default** and starts collecting data automatically. No configuration required!

```cpp
// In main.cpp - already configured
patternLearner = new PatternLearner();
patternLearner->begin();
```

### Learning Period

- **Minimum**: 7 days to establish patterns
- **Optimal**: 30 days for high confidence
- **Status**: Check pattern confidence via web interface

**During learning period:**
- Patterns are being established
- Anomaly detection is disabled
- System learns your tank's normal behavior

**After patterns established:**
- Anomaly detection activates automatically
- Alerts begin for unusual conditions
- Seasonal adaptation becomes available

---

## 📊 How It Works

### Pattern Learning Algorithm

The system uses **Exponential Moving Average (EMA)** to learn patterns:

1. **Collect Data**: Every sensor reading is categorized by hour
2. **Update Pattern**: 
   ```
   pattern[hour] = pattern[hour] * (1-α) + newValue * α
   ```
   - Early learning: α = 0.2 (fast adaptation)
   - Mature patterns: α = 0.05 (slow refinement)

3. **Calculate Variance**:
   ```
   stdDev[hour] = rolling average of |actual - expected|
   ```

4. **Detect Anomalies**:
   ```
   sigma = (actual - expected) / stdDev
   if |sigma| > threshold: ANOMALY
   ```

### Example: Daily Temperature Pattern

```
Hour    Expected    StdDev    Samples
----    --------    ------    -------
00:00   24.8°C     ±0.2°C      35
06:00   24.6°C     ±0.3°C      38
12:00   25.3°C     ±0.4°C      42
18:00   25.2°C     ±0.3°C      40
```

**Scenario**: At 2 PM, temperature reads **27.0°C**

```
Expected: 25.3°C
StdDev: 0.4°C
Deviation: (27.0 - 25.3) / 0.4 = 4.25 sigma
Severity: CRITICAL (>4.0 sigma)
Alert: "Temperature anomaly - heater may be stuck ON"
```

---

## ⚙️ Configuration

### Default Settings

```cpp
// Automatically configured on first run
enabled: true
minSamplesForAnomaly: 7        // Need 7 samples per hour
tempAnomalyThreshold: 2.5σ     // 2.5 standard deviations
phAnomalyThreshold: 2.0σ       // 2.0 standard deviations
tdsAnomalyThreshold: 2.5σ      // 2.5 standard deviations
autoSeasonalAdapt: true        // Enable seasonal PID adjustment
alertOnAnomaly: true           // Send MQTT alerts
maxAnomalyHistory: 100         // Keep last 100 anomalies
```

### Adjusting Sensitivity

**More Sensitive** (catch smaller deviations):
```json
POST /api/pattern/config
{
  "tempThreshold": 2.0,
  "phThreshold": 1.5,
  "tdsThreshold": 2.0
}
```

**Less Sensitive** (reduce false alarms):
```json
POST /api/pattern/config
{
  "tempThreshold": 3.0,
  "phThreshold": 2.5,
  "tdsThreshold": 3.0
}
```

### Disable/Enable Pattern Learning

```json
POST /api/pattern/config
{
  "enabled": false  // Stop learning and anomaly detection
}
```

---

## 📡 API Reference

### GET `/api/pattern/status`
Get current pattern learning status

**Response:**
```json
{
  "enabled": true,
  "established": true,
  "totalSamples": 2436,
  "confidence": 0.95,
  "daysLearning": 14,
  "season": "summer",
  "avgAmbient": 26.3,
  "totalAnomalies": 3,
  "recentAnomalies": 1
}
```

### GET `/api/pattern/hourly`
Get hourly pattern data (all 24 hours)

**Response:**
```json
{
  "hours": [
    {
      "hour": 0,
      "temp": 24.8,
      "tempStdDev": 0.2,
      "ph": 6.95,
      "phStdDev": 0.12,
      "tds": 215,
      "tdsStdDev": 15,
      "samples": 35
    },
    // ... hours 1-23
  ]
}
```

### GET `/api/pattern/anomalies?count=20`
Get recent anomalies

**Response:**
```json
[
  {
    "timestamp": 1729184400,
    "type": "temperature",
    "actual": 27.0,
    "expected": 25.3,
    "deviation": 4.25,
    "severity": "critical"
  },
  {
    "timestamp": 1729180800,
    "type": "ph",
    "actual": 6.3,
    "expected": 6.85,
    "deviation": 2.75,
    "severity": "high"
  }
]
```

### GET `/api/pattern/seasonal`
Get seasonal statistics

**Response:**
```json
{
  "season": "summer",
  "avgAmbient": 26.3,
  "avgWater": 25.1,
  "avgPH": 6.92,
  "avgTDS": 218,
  "daysCollected": 14
}
```

### POST `/api/pattern/config`
Update configuration

**Request:**
```json
{
  "enabled": true,
  "tempThreshold": 2.5,
  "phThreshold": 2.0,
  "tdsThreshold": 2.5,
  "autoSeasonalAdapt": true,
  "alertOnAnomaly": true
}
```

### POST `/api/pattern/reset`
Reset all learned patterns (start fresh)

### POST `/api/pattern/anomalies/clear`
Clear anomaly history

---

## 🔍 Understanding Anomalies

### Severity Levels

| Severity | Sigma Range | Meaning | Action |
|----------|-------------|---------|--------|
| **Low** | 2.0-2.9σ | Slightly unusual | Monitor |
| **Medium** | 3.0-3.9σ | Moderately unusual | Check sensors |
| **High** | 4.0-4.9σ | Very unusual | Inspect equipment |
| **Critical** | ≥5.0σ | Extremely rare | Immediate attention |

### Common Anomaly Scenarios

#### 1. **Temperature Spike**
```
Type: temperature
Actual: 27.5°C
Expected: 25.2°C ± 0.3°C
Deviation: 7.67σ
Severity: CRITICAL
```
**Likely Causes:**
- Heater stuck ON
- Thermostat failure
- Relay malfunction

**Action:**
- Emergency stop heater
- Manually verify relay state
- Check PID emergency stop triggered

---

#### 2. **pH Drop After Feeding**
```
Type: ph
Actual: 6.65
Expected: 6.90 ± 0.15
Deviation: 1.67σ
Severity: normal (below threshold)
```
**Pattern Learner Response:**
- NO ALERT (within normal variation)
- Pattern adapts to post-feeding dip
- Different from abnormal CO2 malfunction

**This is why pattern learning reduces false alarms!**

---

#### 3. **TDS Gradual Increase**
```
Day 1:  210 ppm (expected 200±20) - Normal
Day 7:  245 ppm (expected 210±18) - 1.9σ (Low)
Day 14: 285 ppm (expected 220±20) - 3.25σ (High)
```
**Pattern Learner Response:**
- Adapts to gradual changes
- Alerts when rate of change accelerates
- Suggests water change timing

---

## 🌡️ Seasonal Adaptation

### How It Works

The system tracks 7-day rolling average ambient temperature to detect seasons:

| Season | Ambient Temp Range | PID Adjustments |
|--------|-------------------|-----------------|
| **Winter** | < 18°C | Kp ×1.2, Ki ×1.3, Kd ×1.1 |
| **Spring** | 18-22°C | No adjustment (baseline) |
| **Summer** | 22-27°C | Kp ×0.8, Ki ×0.7, Kd ×1.2 |
| **Hot Summer** | > 27°C | Kp ×0.6, Ki ×0.5, Kd ×1.3 |

### Example: Winter Mode

**Detected:** Ambient temperature drops to 17°C for 7 days

```cpp
Season changed: summer → winter
Recommended PID adjustments:
  Kp: 2.0 → 2.4  (+20% more aggressive)
  Ki: 0.5 → 0.65 (+30% faster integral)
  Kd: 1.0 → 1.1  (+10% better damping)
```

**Benefits:**
- Faster response to heat loss
- Reduced undershoot overnight
- Better temperature stability

### Manual PID Adjustment

If `autoSeasonalAdapt` is enabled, the system **logs recommendations** but doesn't auto-apply them yet (future feature).

Current implementation:
```cpp
// Check seasonal PID multipliers
float kpMult, kiMult, kdMult;
patternLearner->getSeasonalPIDMultipliers(kpMult, kiMult, kdMult);

// Log recommendation
Serial.printf("Seasonal PID suggestion: Kp×%.2f, Ki×%.2f, Kd×%.2f\n",
              kpMult, kiMult, kdMult);
```

---

## 📈 Practical Use Cases

### 1. **Equipment Failure Detection**

**Scenario:** Heater relay stuck ON

**Without Pattern Learning:**
- PID sees "temp > target"
- Reduces output to 0%
- Heater stays ON due to relay failure
- Tank overheats slowly
- **Detection:** When temp exceeds safety limit (could be too late)

**With Pattern Learning:**
- At 2 AM: Expected 24.7°C, actual 25.9°C (4.0σ)
- **Alert triggered immediately**
- "Temperature anomaly at unusual time - check heater"
- Event logged, MQTT alert sent
- **User can intervene before safety limit reached**

---

### 2. **Water Change Optimization**

**Scenario:** Learn optimal water change frequency

**Pattern Analysis:**
```
Week 1: TDS increases 10 ppm/day
Week 2: TDS increases 12 ppm/day (higher feeding)
Week 3: TDS increases 8 ppm/day (vacation, no feeding)
```

**Recommendation:**
```
Normal feeding: Change water at day 8 (TDS ~290)
Heavy feeding: Change at day 7 (TDS ~300)
Vacation: Can extend to day 10 (TDS ~280)
```

---

### 3. **pH Sensor Drift Detection**

**Scenario:** pH probe aging

**Pattern Detection:**
```
Month 1: Readings align with pattern (±0.1 pH)
Month 2: Consistent +0.15 pH offset
Month 3: Consistent +0.30 pH offset
```

**Alert:** "pH sensor showing drift - calibration recommended"

---

### 4. **Lighting Cycle Optimization**

**Observation:**
```
Lights ON (8 AM):  pH rises from 6.85 → 7.10 (photosynthesis)
Lights OFF (8 PM): pH drops from 6.95 → 6.80 (respiration)
```

**Pattern Learner:**
- Learns this is NORMAL daily variation
- Does NOT alert on pH swings
- Only alerts if pattern breaks (e.g., pH doesn't rise after lights ON = algae/plant problem)

---

## 🛠️ Troubleshooting

### Problem: Too Many False Alarms

**Solution 1:** Increase thresholds
```json
POST /api/pattern/config
{
  "tempThreshold": 3.0,
  "phThreshold": 2.5
}
```

**Solution 2:** Wait for more data
- Patterns need 30+ samples per hour for accuracy
- Check `confidence` in `/api/pattern/status`
- If confidence < 0.8, wait a few more days

---

### Problem: Missing Real Anomalies

**Solution:** Decrease thresholds
```json
POST /api/pattern/config
{
  "tempThreshold": 2.0,
  "phThreshold": 1.5
}
```

---

### Problem: Patterns Not Establishing

**Check:**
```
GET /api/pattern/status

{
  "established": false,
  "totalSamples": 145,
  "confidence": 0.15
}
```

**Cause:** Need more data
- Minimum: 7 samples per hour × 24 hours = 168 samples
- Current: 145 samples (need 23 more)
- **Solution:** Wait ~1 more day

---

### Problem: Want to Start Fresh

**Reset all patterns:**
```
POST /api/pattern/reset
```

**Use when:**
- Changed tank setup significantly (new fish, plants)
- Moved tank to different room
- Changed lighting schedule
- After major equipment replacement

---

## 📊 Serial Monitor Output

### During Learning
```
Initializing Pattern Learner...
Pattern learning ENABLED
Anomaly thresholds: Temp=2.5σ, pH=2.0σ, TDS=2.5σ
Learning mode: Collecting data to establish patterns...

[After 7 days]
✓ Patterns established! Anomaly detection now active.
Pattern confidence: 85.3%
```

### When Anomaly Detected
```
⚠️ TEMP ANOMALY [high]: 27.12°C (expected 25.20±0.35, 5.5σ deviation)
⚠️ pH ANOMALY [medium]: 6.42 (expected 6.85±0.15, 2.9σ deviation)
```

### Season Changes
```
Season change detected: spring → summer (ambient: 24.2°C)
Seasonal PID suggestion: Kp×0.80, Ki×0.70, Kd×1.20
```

---

## 🔗 Integration Examples

### MQTT Alert on Anomaly

When an anomaly is detected, MQTT alerts are automatically sent:

```
Topic: aquarium/alerts/pattern
Payload: {
  "severity": "high",
  "type": "temperature",
  "message": "Temperature 4.2σ above normal at 02:30",
  "actual": 27.1,
  "expected": 25.2,
  "action": "Check heater relay"
}
```

### Home Assistant Integration

```yaml
sensor:
  - platform: mqtt
    name: "Aquarium Pattern Status"
    state_topic: "aquarium/pattern/status"
    value_template: "{{ value_json.confidence * 100 }}"
    unit_of_measurement: "%"
    
automation:
  - alias: "Aquarium Anomaly Alert"
    trigger:
      platform: mqtt
      topic: "aquarium/alerts/pattern"
    condition:
      condition: template
      value_template: "{{ trigger.payload_json.severity in ['high', 'critical'] }}"
    action:
      - service: notify.mobile_app
        data:
          title: "🐠 Aquarium Alert"
          message: "{{ trigger.payload_json.message }}"
          data:
            priority: high
```

---

## 📖 Understanding Sigma (σ) Deviations

**What is Sigma?**
- Measures "how unusual" a reading is
- Based on standard deviation from expected pattern
- Higher sigma = more unusual

**Statistical Probability:**

| Sigma | Probability | In Plain English |
|-------|-------------|------------------|
| 1σ | 68% | Common (happens often) |
| 2σ | 95% | Uncommon (happens occasionally) |
| 3σ | 99.7% | Rare (happens rarely) |
| 4σ | 99.99% | Very rare (almost never) |
| 5σ | 99.9999% | Extremely rare (equipment failure likely) |

**Example:**
- **2.5σ threshold** means: Only alert if reading is more unusual than 98.8% of normal readings
- **Higher threshold** = fewer alerts, only catch major problems
- **Lower threshold** = more alerts, catch subtle issues

---

## 🎯 Best Practices

### 1. **Give It Time**
- Wait 7-14 days before relying on anomaly detection
- Confidence builds over time (check `/api/pattern/status`)
- Seasonal patterns need 4-6 weeks

### 2. **Start Conservative**
- Use default thresholds (2.5σ temp, 2.0σ pH)
- Adjust based on YOUR tank's behavior
- Planted tanks may have more pH variation

### 3. **Reset After Major Changes**
- New fish load → different waste production
- New lighting schedule → different daily patterns
- Equipment changes → different patterns

```
POST /api/pattern/reset
```

### 4. **Monitor Pattern Confidence**
```javascript
async function checkPatternReadiness() {
  const status = await fetch('/api/pattern/status');
  const data = await status.json();
  
  if (data.confidence < 0.8) {
    console.log('Patterns still learning... ' + 
                (data.confidence * 100) + '% confidence');
  } else {
    console.log('Patterns established! Anomaly detection active.');
  }
}
```

### 5. **Review Anomalies Weekly**
- Check `/api/pattern/anomalies`
- Look for patterns in false alarms
- Adjust thresholds if needed

---

## 💡 Tips & Tricks

### Reduce False Alarms from Feeding
The pattern learner automatically adapts to regular feeding times! Just feed consistently:

```
Week 1: Feed at 12:00 PM daily
Week 2: Pattern learns "pH drops 0.2 at noon"
Week 3: No more feeding alerts!
```

### Predict Maintenance Needs
Check seasonal stats to optimize maintenance:

```javascript
const seasonal = await fetch('/api/pattern/seasonal').then(r => r.json());

if (seasonal.avgTDS > 250) {
  alert('TDS trending high - water change recommended soon');
}
```

### Debug Sensor Issues
Compare actual vs expected to validate sensor accuracy:

```javascript
const hourly = await fetch('/api/pattern/hourly').then(r => r.json());
const currentHour = new Date().getHours();
const pattern = hourly.hours[currentHour];

console.log('Expected pH:', pattern.ph);
console.log('Actual pH:', currentPH);
console.log('Difference:', Math.abs(currentPH - pattern.ph));
```

---

## 🔮 Future Enhancements

Planned features (not yet implemented):

1. **Auto-PID Tuning** - Apply seasonal PID adjustments automatically
2. **Predictive Maintenance** - Predict equipment failure before it happens
3. **Water Change Predictions** - ML-driven water change scheduling
4. **Feeding Detection** - Auto-learn feeding times and patterns
5. **Long-term Trend Analysis** - Monthly/yearly pattern visualization

---

## 📞 Support & Feedback

**Pattern learning not working as expected?**

1. Check configuration: `GET /api/pattern/config`
2. Verify patterns established: `GET /api/pattern/status`
3. Review recent anomalies: `GET /api/pattern/anomalies?count=50`
4. Check serial monitor for debug output
5. Reset and start fresh if needed: `POST /api/pattern/reset`

**Want to contribute?**
- Submit issues/PRs on GitHub
- Share your pattern learning insights
- Help improve anomaly detection algorithms

---

## 🧮 Technical Details

### Memory Usage
- **RAM**: ~15KB (pattern arrays + anomaly history)
- **NVS Storage**: ~3KB (persistent patterns)
- **CPU**: <1% (exponential moving average calculations)

### Storage Format
Patterns are stored in NVS namespace `"patterns"`:
- 24 hours × (temp + pH + TDS + ambient + stdDevs) = ~200 floats
- Compressed storage using keys like `hr0t`, `hr0ts`, etc.
- Survives ESP32 restarts/power loss

### Update Frequency
- Pattern learning: Every sensor reading (~1 Hz)
- Anomaly checking: Every 60 seconds
- Pattern saving: Every 60 minutes (auto-save)

---

## 📚 Additional Resources

- [Main README](README.md) - Full system overview
- [PID Control Guide](ADVANCED_FEATURES.md) - PID tuning details
- [Water Change Assistant](WATER_CHANGE_ASSISTANT.md) - Maintenance automation
- [pH Calibration](PH_CALIBRATION_GUIDE.md) - Sensor calibration

---

**Built with ❤️ for better aquarium management through machine learning!** 🐠🧠
