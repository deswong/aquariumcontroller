# Water Change Assistant Documentation

## Overview
The Water Change Assistant is an intelligent system for managing routine aquarium water changes safely and effectively. It automates safety procedures, tracks history, and helps maintain optimal water quality through scheduled maintenance.

---

## Features

### 1. **Automated Safety Management**
- **System Pause**: Automatically disables heating and CO2 systems during water changes
- **Parameter Monitoring**: Tracks temperature and pH before/after changes
- **Safety Limits**: Validates new water parameters before resuming systems
- **Emergency Stop**: Can cancel water change at any phase

### 2. **Schedule Management**
- **Multiple Schedules**: Daily, Weekly, Bi-weekly, Monthly
- **Volume Control**: Configurable percentage (10-50%) of tank volume
- **Overdue Alerts**: Notifications when maintenance is overdue
- **MQTT Integration**: Remote monitoring and alerts

### 3. **Multi-Phase Process**
Guided step-by-step process:
1. **Prepare** - Pause all systems for safety
2. **Draining** - Remove old water
3. **Drained** - Wait for new water preparation
4. **Filling** - Add conditioned new water
5. **Stabilizing** - Allow parameters to normalize
6. **Complete** - Resume normal operation

### 4. **History Tracking**
- Stores last 50 water changes in SPIFFS
- Tracks volume, temperature, pH, and duration
- Statistical analysis (monthly totals, averages)
- Export history via API

---

## Configuration

### Initial Setup

Set tank volume and schedule in `main.cpp`:

```cpp
waterChangeAssistant->setTankVolume(20.0);           // 20 gallon tank
waterChangeAssistant->setSchedule(SCHEDULE_WEEKLY, 25.0); // 25% weekly
waterChangeAssistant->setSafetyLimits(2.0, 0.5);     // ±2°C, ±0.5 pH
```

### Tank Volume
Set your aquarium's total volume in gallons:

```cpp
waterChangeAssistant->setTankVolume(55.0); // 55 gallon tank
```

**Supported Range**: 5-1000 gallons

### Schedule Options

```cpp
SCHEDULE_NONE     // No automatic scheduling
SCHEDULE_DAILY    // Every 1 day (advanced users)
SCHEDULE_WEEKLY   // Every 7 days (recommended)
SCHEDULE_BIWEEKLY // Every 14 days
SCHEDULE_MONTHLY  // Every 30 days
```

**Example Configurations:**

```cpp
// Planted tank with CO2 injection - frequent small changes
waterChangeAssistant->setSchedule(SCHEDULE_WEEKLY, 20.0);

// Community tank - standard maintenance
waterChangeAssistant->setSchedule(SCHEDULE_BIWEEKLY, 30.0);

// Established tank - minimal maintenance
waterChangeAssistant->setSchedule(SCHEDULE_MONTHLY, 25.0);
```

### Safety Limits

Configure maximum safe parameter changes:

```cpp
waterChangeAssistant->setSafetyLimits(
    2.0,  // Max temperature difference (°C)
    0.5   // Max pH difference
);
```

**Recommended Values:**
- **Sensitive fish** (discus, tetras): ±1.5°C, ±0.3 pH
- **Standard community**: ±2.0°C, ±0.5 pH
- **Robust species** (goldfish, cichlids): ±3.0°C, ±0.8 pH

---

## Usage

### Starting a Water Change

#### Via API:
```bash
# Start with scheduled volume
curl -X POST http://aquarium.local/api/waterchange/start

# Start with custom volume (10 gallons)
curl -X POST http://aquarium.local/api/waterchange/start \
  -H "Content-Type: application/json" \
  -d '{"volume": 10.0}'
```

#### Via Code:
```cpp
// Use scheduled volume
waterChangeAssistant->startWaterChange();

// Custom volume (15 gallons)
waterChangeAssistant->startWaterChange(15.0);
```

### Step-by-Step Process

**Phase 1: PREPARE**
- Systems automatically pause (heater OFF, CO2 OFF)
- Initial temperature and pH recorded
- Ready to drain

```bash
curl -X POST http://aquarium.local/api/waterchange/advance
```

**Phase 2: DRAINING**
- Remove old water (siphon, pump, bucket)
- Timeout warning after 10 minutes

```bash
curl -X POST http://aquarium.local/api/waterchange/advance
```

**Phase 3: DRAINED**
- Prepare new water (dechlorinate, temperature match)
- System waits for your confirmation

```bash
curl -X POST http://aquarium.local/api/waterchange/advance
```

**Phase 4: FILLING**
- Add conditioned new water slowly
- Monitor water level
- Timeout warning after 10 minutes

```bash
curl -X POST http://aquarium.local/api/waterchange/advance
```

**Phase 5: STABILIZING**
- Safety checks run automatically
- Waits 5 minutes for parameters to stabilize
- Verifies temperature and pH are within safe ranges
- **Auto-advances** to complete

**Phase 6: COMPLETE**
- Systems automatically resume
- History recorded
- MQTT alert sent

### Checking Status

```bash
curl http://aquarium.local/api/waterchange/status
```

Response:
```json
{
  "inProgress": true,
  "phase": 2,
  "phaseDescription": "Draining old water",
  "phaseElapsed": 120,
  "systemsPaused": true,
  "currentVolume": 5.0,
  "tankVolume": 20.0,
  "schedule": 7,
  "daysSinceLastChange": 3,
  "daysUntilNextChange": 4,
  "isOverdue": false
}
```

### Canceling a Water Change

If you need to abort:

```bash
curl -X POST http://aquarium.local/api/waterchange/cancel
```

⚠️ **Warning**: Systems will resume immediately. Only cancel if tank is filled to safe level.

---

## API Reference

### GET `/api/waterchange/status`
Returns current status and schedule information.

**Response Fields:**
- `inProgress` - Boolean, water change active
- `phase` - Integer (0-6), current phase
- `phaseDescription` - String, human-readable phase
- `phaseElapsed` - Integer, seconds in current phase
- `systemsPaused` - Boolean, heating/CO2 paused
- `currentVolume` - Float, gallons being changed
- `tankVolume` - Float, total tank gallons
- `schedule` - Integer, days between changes
- `daysSinceLastChange` - Integer
- `daysUntilNextChange` - Integer
- `isOverdue` - Boolean, maintenance overdue

### POST `/api/waterchange/start`
Start a new water change.

**Request Body (optional):**
```json
{
  "volume": 10.0
}
```

Omit `volume` to use scheduled amount.

**Response:**
```json
{"status": "ok"}
```

### POST `/api/waterchange/advance`
Move to next phase.

**Response:**
```json
{"status": "ok"}
```

### POST `/api/waterchange/cancel`
Cancel current water change and resume systems.

**Response:**
```json
{"status": "ok"}
```

### GET `/api/waterchange/history?count=10`
Retrieve water change history.

**Query Parameters:**
- `count` - Number of recent records (default: 10)

**Response:**
```json
[
  {
    "timestamp": 123456,
    "volume": 5.0,
    "tempBefore": 25.5,
    "tempAfter": 25.8,
    "phBefore": 6.8,
    "phAfter": 6.9,
    "duration": 15,
    "successful": true
  }
]
```

### POST `/api/waterchange/schedule`
Update schedule settings.

**Request Body:**
```json
{
  "schedule": 7,
  "volumePercent": 25.0
}
```

**Schedule Values:**
- 0: None
- 1: Daily
- 7: Weekly
- 14: Bi-weekly
- 30: Monthly

**Response:**
```json
{"status": "ok"}
```

### GET `/api/waterchange/stats`
Get water change statistics.

**Response:**
```json
{
  "totalChanges": 24,
  "changesThisMonth": 4,
  "volumeThisMonth": 20.0,
  "averageVolume": 5.0
}
```

---

## Best Practices

### Water Preparation

**Before Starting:**
1. Prepare new water in clean container
2. Add dechlorinator/conditioner
3. Match temperature (±2°C of tank)
4. Match pH if possible (±0.5)
5. Have equipment ready (siphon, buckets, towels)

**Temperature Matching:**
```
Tank Temp: 25°C
Safe Range: 23-27°C
Target: 25°C ± 1°C for best results
```

**pH Matching:**
```
Tank pH: 6.8
Safe Range: 6.3-7.3
Target: 6.8 ± 0.2 for best results
```

### Volume Guidelines

**Small Tanks (5-20 gallons):**
- Weekly: 20-25%
- Bi-weekly: 30%
- Monthly: Not recommended (too infrequent)

**Medium Tanks (20-55 gallons):**
- Weekly: 25-30%
- Bi-weekly: 30-40%
- Monthly: 40-50%

**Large Tanks (55+ gallons):**
- Weekly: 20%
- Bi-weekly: 25-30%
- Monthly: 30-40%

**Planted Tanks with CO2:**
- More frequent, smaller changes preferred
- Weekly 20-25% recommended
- Maintains stable parameters

**Heavily Stocked Tanks:**
- Increase frequency and/or volume
- Weekly 30% or bi-weekly 40%
- Monitor nitrate levels

### Timing

**Best Times:**
- Morning (before feeding)
- When you can monitor for 30 minutes after
- Not during spawning or stress periods

**Avoid:**
- Late evening (disrupts fish sleep)
- Immediately after feeding
- During disease treatment
- Extreme weather (power outages)

### Safety Procedures

**Before Starting:**
- [ ] Unplug heater if removing >50% water
- [ ] Stop feeding 2-3 hours before
- [ ] Have new water prepared and conditioned
- [ ] Check water parameters of new water

**During Water Change:**
- [ ] Remove old water slowly (reduce stress)
- [ ] Add new water slowly (5-10 minutes minimum)
- [ ] Direct flow away from fish
- [ ] Monitor fish behavior

**After Completion:**
- [ ] Verify systems resumed (heater, CO2)
- [ ] Check temperature within 15 minutes
- [ ] Observe fish for stress signs
- [ ] Clean filter if needed

---

## Troubleshooting

### Issue: "Failed to start water change"

**Possible Causes:**
1. Water change already in progress
2. Volume exceeds 50% of tank capacity

**Solution:**
- Check status: `GET /api/waterchange/status`
- Cancel if stuck: `POST /api/waterchange/cancel`
- Reduce volume if too large

### Issue: "Unsafe water parameters"

**Symptoms:**
- Cannot advance from FILLING to STABILIZING
- Error message about parameter difference

**Solution:**
1. Check current temperature and pH
2. Compare to pre-change values
3. Adjust new water to match closer
4. Wait for parameters to normalize
5. Override only if certain (cancel and restart)

**Example:**
```
Before: 25.0°C
After: 28.0°C (3°C difference)
Limit: 2.0°C
Action: Let water cool or add cooler water
```

### Issue: Phase timeout

**DRAINING timeout (10 minutes):**
- Check siphon/drain system
- Ensure water is actually draining
- Advance manually when ready

**FILLING timeout (10 minutes):**
- Check water source
- Ensure tank is filling
- Advance manually when ready

### Issue: Systems not resuming

**Check:**
1. Water change completed successfully
2. Emergency stops not active
3. Manual relay override not set

**Solution:**
```bash
# Check status
curl http://aquarium.local/api/data

# Check water change phase
curl http://aquarium.local/api/waterchange/status

# If stuck, cancel and restart
curl -X POST http://aquarium.local/api/waterchange/cancel
```

### Issue: History not saving

**Possible Causes:**
- SPIFFS not mounted
- Storage full
- File corruption

**Solution:**
1. Check event logs for SPIFFS errors
2. Clear old history if needed
3. Verify SPIFFS initialized in setup

---

## MQTT Integration

### Alert Messages

Water change events publish to `{prefix}/alert`:

**Water Change Started:**
```json
{
  "category": "waterchange",
  "message": "Water change started - systems paused",
  "critical": false,
  "timestamp": 123456
}
```

**Water Change Completed:**
```json
{
  "category": "waterchange",
  "message": "Water change completed successfully - systems resumed",
  "critical": false,
  "timestamp": 123460
}
```

**Overdue Alert:**
```json
{
  "category": "waterchange",
  "message": "Water change overdue - maintenance required",
  "critical": false,
  "timestamp": 123400
}
```

### OpenHAB Integration

**Items:**
```
String AquariumWaterChangePhase "Phase" { channel="mqtt:topic:aquarium:waterchange_phase" }
Switch AquariumWaterChangeInProgress "In Progress" { channel="mqtt:topic:aquarium:waterchange_active" }
Number AquariumDaysUntilChange "Days Until Change" { channel="mqtt:topic:aquarium:days_until_change" }
```

**Rules Example:**
```
rule "Water Change Reminder"
when
    Item AquariumDaysUntilChange changed to 1
then
    sendNotification("your@email.com", "Aquarium water change tomorrow")
end

rule "Water Change Started"
when
    Item AquariumWaterChangeInProgress changed to ON
then
    logInfo("Aquarium", "Water change started")
    // Turn off smart plugs for safety
    sendCommand(AquariumHeaterPlug, OFF)
end
```

---

## Advanced Usage

### Custom Drain/Fill Automation

For automated systems (solenoids, pumps):

```cpp
// Start water change
waterChangeAssistant->startWaterChange(10.0);

// In your control loop:
if (waterChangeAssistant->getCurrentPhase() == PHASE_DRAINING) {
    // Open drain valve
    digitalWrite(DRAIN_VALVE_PIN, HIGH);
    
    // Monitor water level sensor
    if (waterLevelLow()) {
        // Close valve
        digitalWrite(DRAIN_VALVE_PIN, LOW);
        // Advance to next phase
        waterChangeAssistant->advancePhase();
    }
}

if (waterChangeAssistant->getCurrentPhase() == PHASE_FILLING) {
    // Open fill valve
    digitalWrite(FILL_VALVE_PIN, HIGH);
    
    // Monitor water level sensor
    if (waterLevelHigh()) {
        // Close valve
        digitalWrite(FILL_VALVE_PIN, LOW);
        // Advance to next phase
        waterChangeAssistant->advancePhase();
    }
}
```

### Integration with Auto-Feeder

Pause feeding during water changes:

```cpp
void checkFeeding() {
    // Don't feed during water changes
    if (waterChangeAssistant && waterChangeAssistant->isInProgress()) {
        return; // Skip feeding
    }
    
    // Normal feeding logic
    if (timeToFeed()) {
        activateFeeder();
    }
}
```

### Scheduled Automation

Auto-start water changes on schedule:

```cpp
void checkWaterChangeSchedule() {
    if (!waterChangeAssistant) return;
    
    // Check if change is overdue and it's the right time
    if (waterChangeAssistant->isChangeOverdue() && 
        hour() == 10 && minute() == 0) { // 10:00 AM
        
        Serial.println("Auto-starting scheduled water change");
        waterChangeAssistant->startWaterChange();
        
        // Send notification
        sendMQTTAlert("waterchange", "Automatic water change started", false);
    }
}
```

---

## Performance Impact

### Memory Usage
- **RAM**: ~1-2KB for active water change
- **SPIFFS**: ~10KB for 50 record history (200 bytes each)
- **NVS**: ~100 bytes for settings

### Processing Overhead
- `update()`: < 1ms (only during active water change)
- History save: ~50ms (only when completing change)
- No impact during idle state

### System Behavior
- PID controllers paused (no calculations)
- Relay outputs forced to 0%
- Sensor readings continue normally
- Web server remains responsive

---

## Safety Features

### Automatic Protections

1. **Volume Limits**
   - Maximum 50% of tank capacity
   - Prevents excessive water removal

2. **Parameter Validation**
   - Checks temperature difference
   - Checks pH difference
   - Blocks resumption if unsafe

3. **System Pause**
   - Heater disabled (prevents running dry)
   - CO2 disabled (prevents overdose in low water)
   - Automatic on resume

4. **Timeout Warnings**
   - Drain phase: 10 minutes
   - Fill phase: 10 minutes
   - Logged to event system

5. **Emergency Cancel**
   - Available at any phase
   - Immediate system resume
   - Logged for review

### Manual Overrides

If you need to bypass safety checks:

```cpp
// Cancel current water change
waterChangeAssistant->cancelWaterChange();

// Manually re-enable systems if needed
heaterRelay->begin();  // Re-enables heater
co2Relay->begin();     // Re-enables CO2
```

⚠️ **Use with extreme caution** - bypassing safety features can harm livestock.

---

## Maintenance

### Weekly
- Check if water change is due
- Review history via API
- Verify systems resume properly

### Monthly
- Review statistics
- Check average change volume
- Adjust schedule if needed

### Quarterly
- Export history for backup
- Review event logs for issues
- Verify safety limits appropriate

---

## Future Enhancements

### Planned Features
1. **Water Level Sensors** - Automated drain/fill detection
2. **TDS Monitoring** - Verify water quality during fill
3. **Auto-Schedule** - Adjust based on nitrate levels
4. **Multi-Tank Support** - Manage multiple aquariums
5. **Mobile App** - Guided water change process
6. **Voice Control** - Alexa/Google Home integration

---

## References

### Related Documentation
- `PRODUCTION_FEATURES.md` - System reliability features
- `ADVANCED_FEATURES.md` - PID and WiFi improvements
- `README.md` - System overview
- `TESTING.md` - Testing procedures

### Best Practices Resources
- Regular water changes reduce nitrates
- Frequent small changes better than large infrequent
- Match parameters for fish health
- Never change more than 50% at once

---

## Support

### Logs and Diagnostics

Check event logs:
```bash
curl http://aquarium.local/api/logs?count=50 | grep waterchange
```

Check current status:
```bash
curl http://aquarium.local/api/waterchange/status
```

View history:
```bash
curl http://aquarium.local/api/waterchange/history?count=20
```

### Common Questions

**Q: Can I skip a scheduled change?**
A: Yes, the system tracks "days until next change" but doesn't force changes. You control when to start.

**Q: What if I lose power during a water change?**
A: System resets to IDLE on reboot. Manually assess tank condition and restart if needed.

**Q: Can I change the water while fish are feeding?**
A: Not recommended. Wait 2-3 hours after feeding to avoid stress.

**Q: Does the system add chemicals automatically?**
A: No, you must add dechlorinator and conditioner manually to new water.

**Q: What if my tank is overstocked?**
A: Increase frequency (weekly → every 5 days) or volume (25% → 30%). Monitor nitrates.

