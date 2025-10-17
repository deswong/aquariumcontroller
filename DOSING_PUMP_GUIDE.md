# Fertilizer Dosing Pump System Guide

## Table of Contents
1. [Overview](#overview)
2. [Hardware Setup](#hardware-setup)
3. [Initial Configuration](#initial-configuration)
4. [Calibration Procedure](#calibration-procedure)
5. [Dosing Operations](#dosing-operations)
6. [Automated Scheduling](#automated-scheduling)
7. [Maintenance](#maintenance)
8. [Safety Features](#safety-features)
9. [Troubleshooting](#troubleshooting)
10. [API Reference](#api-reference)
11. [Web Interface Guide](#web-interface-guide)

---

## Overview

The Fertilizer Dosing Pump System provides automated, precise liquid fertilizer dosing for your aquarium. It features:

- **Accurate Volume Control**: Calibrated flow rate ensures precise dosing
- **Automated Scheduling**: Daily, weekly, or custom interval dosing
- **Safety Limits**: Maximum dose and daily volume protection
- **Maintenance Functions**: Prime, backflush, and cleaning cycles
- **Web Interface**: Complete control and monitoring through browser
- **History Tracking**: Records all dosing events with timestamps
- **Real-time Monitoring**: Progress bars and status updates

### Hardware Components

- **ESP32 Microcontroller**: Main control unit
- **DRV8871 Motor Driver**: Controls the peristaltic pump motor
- **Peristaltic Pump**: Delivers fertilizer with precise volume control
- **Power Supply**: 12V DC (recommended) for pump motor

---

## Hardware Setup

### DRV8871 Motor Driver

The DRV8871 is a brushed DC motor driver that provides bidirectional control for the peristaltic pump.

#### Pin Connections

```
ESP32          DRV8871         Notes
─────────────────────────────────────────────────────
GPIO 25    →   IN1            Control input 1 (PWM)
GPIO 26    →   IN2            Control input 2 (PWM)
GND        →   GND            Ground connection
─────────────────────────────────────────────────────

DRV8871        Pump Motor     Notes
─────────────────────────────────────────────────────
OUT1       →   Motor +        Positive motor terminal
OUT2       →   Motor -        Negative motor terminal
VM         →   12V DC         Motor power supply
GND        →   Power GND      Power ground
```

#### Wiring Diagram

```
                     ┌─────────────────┐
                     │   ESP32 Board   │
                     │                 │
                     │   GPIO 25  ────────┐
                     │   GPIO 26  ────────┼─┐
                     │   GND      ────────┼─┼─┐
                     └─────────────────┘  │ │ │
                                          │ │ │
                     ┌─────────────────┐  │ │ │
                     │   DRV8871       │  │ │ │
                     │   Motor Driver  │  │ │ │
                     │                 │  │ │ │
                     │   IN1 ────────────┘ │ │
                     │   IN2 ──────────────┘ │
                     │   GND ────────────────┘
                     │                 │
    12V DC ────────→ │   VM            │
    Power GND ─────→ │   GND           │
                     │                 │
                     │   OUT1 ────────────┐
                     │   OUT2 ────────────┼─┐
                     └─────────────────┘  │ │
                                          │ │
                     ┌─────────────────┐  │ │
                     │ Peristaltic     │  │ │
                     │ Pump Motor      │  │ │
                     │                 │  │ │
                     │   + ───────────────┘ │
                     │   - ─────────────────┘
                     └─────────────────┘
```

### Peristaltic Pump Setup

1. **Tubing Installation**:
   - Use food-grade silicone tubing (4mm ID recommended)
   - Cut tubing to required length (inlet to outlet)
   - Install tubing in pump head following manufacturer's guide
   - Ensure tubing is seated properly in roller tracks

2. **Fertilizer Container**:
   - Use opaque container to prevent light degradation
   - Place inlet tube at bottom of container
   - Add strainer/filter to inlet to prevent debris
   - Secure container to prevent tipping

3. **Outlet Placement**:
   - Route outlet tube to aquarium
   - Position outlet near water flow for mixing
   - Avoid placing directly on plants or substrate
   - Secure tubing with clips to prevent movement

### Power Supply Requirements

- **Voltage**: 12V DC (typical), check pump specifications
- **Current**: 0.5-2A depending on pump size
- **Type**: Regulated DC power supply
- **Safety**: Use power supply with overcurrent protection

---

## Initial Configuration

### GPIO Pin Configuration

The default GPIO pins are defined in `main.cpp`:

```cpp
// GPIO 25: DRV8871 IN1
// GPIO 26: DRV8871 IN2
// PWM Channel 1
dosingPump = new DosingPump(25, 26, 1);
```

To change pins, modify these values in `main.cpp` and recompile.

### Default Settings

The system initializes with these defaults:

```cpp
// Safety Limits
Max Dose Volume: 50.0 mL per operation
Max Daily Volume: 200.0 mL per day
Safety Limits: Enabled

// Schedule
Enabled: False (disabled until calibrated)
Frequency: Weekly
Time: 09:00 (9:00 AM)
Dose Volume: 5.0 mL
```

### First Boot

On first boot, the system will:
1. Initialize the dosing pump hardware
2. Load saved calibration from NVS (if exists)
3. Load saved schedule configuration
4. Log status to event logger
5. Display calibration status in web interface

---

## Calibration Procedure

**⚠️ IMPORTANT**: The pump MUST be calibrated before use to ensure accurate dosing volumes.

### When to Calibrate

- **Initial Setup**: Always calibrate before first use
- **Tubing Change**: Recalibrate after replacing tubing
- **Monthly**: Recalibrate every 30 days for accuracy
- **After Maintenance**: Recalibrate after major maintenance
- **Inaccurate Dosing**: If volumes seem incorrect

### Calibration Steps

#### Method 1: Web Interface (Recommended)

1. **Navigate to Dosing Pump Tab**:
   - Open web interface
   - Click "💊 Dosing Pump" tab
   - Scroll to "Calibration Wizard" section

2. **Prepare**:
   - Get a graduated cylinder or measuring container
   - Have paper and pen ready to record measurements
   - Ensure fertilizer container has liquid
   - Position outlet tube over measuring container

3. **Start Calibration**:
   - Select calibration speed (100% recommended)
   - Click "Start Calibration" button
   - Pump will start running immediately

4. **Collect Output**:
   - Watch the timer counting elapsed seconds
   - Collect output in measuring container
   - Run for at least 30 seconds for accuracy
   - Click "Stop & Measure" when done

5. **Enter Measurements**:
   - Read the exact volume from container (e.g., 25.5 mL)
   - Duration is automatically recorded
   - Enter volume in "Measured Volume" field
   - Click "Finish Calibration"

6. **Verify**:
   - System calculates flow rate (mL/sec)
   - Flow rate is displayed on screen
   - Status changes to "✅ Calibrated"
   - Calibration is saved to NVS memory

#### Method 2: REST API

```bash
# Start calibration at 100% speed
curl -X POST http://[IP]/api/dosing/calibrate/start \
  -H "Content-Type: application/json" \
  -d '{"speed": 100}'

# Wait and collect output...
# Measure volume and duration

# Finish calibration with measurements
curl -X POST http://[IP]/api/dosing/calibrate/finish \
  -H "Content-Type: application/json" \
  -d '{"measuredML": 25.5, "seconds": 30}'
```

### Calibration Tips

- **Use Accurate Measurements**: Use graduated cylinder with 0.5 mL precision
- **Run Long Enough**: 30-60 seconds provides best accuracy
- **Multiple Attempts**: Calibrate 2-3 times and average if unsure
- **Temperature Matters**: Calibrate at normal operating temperature
- **Clean Tubing**: Ensure tubing is clean and free of air bubbles
- **Record Values**: Write down flow rate for future reference

### Calibration Data Storage

Calibration data is stored in NVS memory:
- **Namespace**: `dosepump-cal`
- **Flow Rate**: Calculated mL/second
- **Timestamp**: Unix timestamp of calibration
- **Persistence**: Survives power loss and reboots

---

## Dosing Operations

### Manual Dosing

#### Web Interface

1. Navigate to "Manual Controls" section
2. Enter desired volume (0.1 - 50.0 mL)
3. Adjust speed slider (10 - 100%)
4. Click "Dose Now" button
5. Watch progress bar for completion
6. Click "Stop" if needed to abort

#### REST API

```bash
# Start manual dose: 5 mL at 100% speed
curl -X POST http://[IP]/api/dosing/start \
  -H "Content-Type: application/json" \
  -d '{"volume": 5.0, "speed": 100}'

# Stop dosing
curl -X POST http://[IP]/api/dosing/stop
```

### Dosing Speed

Speed percentage controls motor PWM:
- **10%**: Very slow, for testing
- **50%**: Half speed, gentle dosing
- **100%**: Full speed, fastest dosing

Lower speeds provide more accurate volumes for small doses.

### Progress Monitoring

During dosing, the web interface shows:
- **State**: "DOSING" or "PAUSED"
- **Progress Bar**: Visual percentage complete
- **Volume**: Current vs. target volume (e.g., "2.5 / 5.0 mL")
- **Speed**: Current motor speed percentage

### Pausing and Resuming

You can pause active dosing:

```bash
# Pause
curl -X POST http://[IP]/api/dosing/pause

# Resume
curl -X POST http://[IP]/api/dosing/resume
```

Pausing is useful if you need to:
- Check aquarium conditions
- Add more fertilizer to container
- Temporarily stop during water change

---

## Automated Scheduling

### Schedule Types

#### Daily Dosing
Doses once per day at specified time.

```json
{
  "enabled": true,
  "schedule": "daily",
  "hour": 9,
  "minute": 0,
  "doseVolume": 5.0
}
```

#### Weekly Dosing
Doses once per week at specified time (every 7 days).

```json
{
  "enabled": true,
  "schedule": "weekly",
  "hour": 9,
  "minute": 0,
  "doseVolume": 10.0
}
```

#### Custom Interval
Doses every N days at specified time.

```json
{
  "enabled": true,
  "schedule": "custom",
  "customDays": 3,
  "hour": 9,
  "minute": 0,
  "doseVolume": 7.5
}
```

### Configuring Schedule

#### Web Interface

1. Navigate to "Automated Schedule" section
2. Check "Enable Automated Dosing"
3. Select frequency (Daily/Weekly/Custom)
4. Set time (hour:minute)
5. Enter dose volume (mL)
6. Click "Save Schedule"
7. View "Next dose in X hours Y minutes"

#### REST API

```bash
# Set daily schedule at 9:00 AM for 5 mL
curl -X POST http://[IP]/api/dosing/schedule \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "schedule": "daily",
    "hour": 9,
    "minute": 0,
    "doseVolume": 5.0
  }'

# Get current schedule
curl http://[IP]/api/dosing/schedule
```

### Schedule Behavior

- **Automatic Execution**: Doses trigger automatically at scheduled time
- **Overdue Handling**: If ESP32 was offline, dose triggers on next update
- **Safety Checks**: Schedule respects daily volume limits
- **Calibration Required**: Schedule only runs if pump is calibrated
- **Event Logging**: All scheduled doses logged to history

### Next Dose Calculation

The system calculates next dose time based on:
1. Last successful dose timestamp
2. Schedule interval (1 day, 7 days, or custom)
3. Scheduled time of day (hour:minute)

Example for daily 9:00 AM schedule:
- Last dose: Today at 9:00 AM
- Next dose: Tomorrow at 9:00 AM

---

## Maintenance

Regular maintenance ensures reliable operation and accurate dosing.

### Daily Maintenance

- **Visual Inspection**: Check for leaks or loose connections
- **Fertilizer Level**: Ensure adequate fertilizer in container
- **Tubing Check**: Look for kinks or blockages
- **Operation Verification**: Check web interface shows normal operation

### Weekly Maintenance

#### Prime Pump

Removes air bubbles and verifies flow:

**Web Interface**:
1. Navigate to "Manual Controls"
2. Set duration (10 seconds)
3. Click "Prime Pump"
4. Observe steady flow from outlet

**REST API**:
```bash
curl -X POST http://[IP]/api/dosing/prime \
  -H "Content-Type: application/json" \
  -d '{"duration": 10, "speed": 50}'
```

#### Check for Blockages

1. Run prime operation
2. Verify consistent flow rate
3. Check for air bubbles in tubing
4. Inspect inlet filter for debris

### Monthly Maintenance

#### Full Cleaning Cycle

Removes deposits and maintains performance:

**Web Interface**:
1. Remove outlet from aquarium
2. Place in waste container
3. Click "Run Cleaning" button
4. Wait for 3 forward/reverse cycles to complete

**REST API**:
```bash
curl -X POST http://[IP]/api/dosing/clean \
  -H "Content-Type: application/json" \
  -d '{"cycles": 3}'
```

#### Backflush Operation

Clears partial blockages:

**Web Interface**:
1. Set duration (10-15 seconds)
2. Click "Backflush" button
3. Pump runs in reverse to clear blockages

**REST API**:
```bash
curl -X POST http://[IP]/api/dosing/backflush \
  -H "Content-Type: application/json" \
  -d '{"duration": 10, "speed": 30}'
```

#### Recalibration

- Perform full calibration procedure
- Compare new flow rate to previous
- If flow rate changed >10%, investigate tubing wear
- Update calibration in system

### Quarterly Maintenance

#### Tubing Replacement

1. Stop all dosing operations
2. Remove old tubing from pump head
3. Inspect pump rollers for wear
4. Install new food-grade silicone tubing
5. Prime pump to remove air
6. **Recalibrate pump** (required after tubing change)

#### Motor Inspection

1. Listen for unusual noises (grinding, squealing)
2. Check motor shaft for smooth rotation
3. Inspect electrical connections
4. Verify motor runs at all speed settings

#### Deep Cleaning

1. Disconnect fertilizer container
2. Run cleaning solution (vinegar/water)
3. Multiple cleaning cycles
4. Flush with clean water
5. Reconnect fertilizer
6. Prime and recalibrate

### Maintenance Schedule Checklist

| Task | Frequency | Estimated Time |
|------|-----------|----------------|
| Visual inspection | Daily | 1 minute |
| Check fertilizer level | Daily | 1 minute |
| Prime pump | Weekly | 2 minutes |
| Clean inlet filter | Weekly | 5 minutes |
| Full cleaning cycle | Monthly | 10 minutes |
| Recalibration | Monthly | 5 minutes |
| Tubing replacement | Quarterly | 15 minutes |
| Deep cleaning | Quarterly | 30 minutes |

---

## Safety Features

### Safety Limits

The system includes multiple safety limits to prevent overdosing:

#### Maximum Dose Volume

Limits single operation volume:
- **Default**: 50 mL
- **Purpose**: Prevents large accidental doses
- **Override**: Can be adjusted in web interface

#### Maximum Daily Volume

Limits total daily dosing:
- **Default**: 200 mL per 24 hours
- **Purpose**: Prevents overdosing from multiple operations
- **Reset**: Automatically resets at midnight
- **Tracking**: Accumulates manual + scheduled doses

#### Safety Configuration

**Web Interface**:
1. Navigate to "Safety Limits" section
2. Adjust "Max Dose per Operation"
3. Adjust "Max Daily Volume"
4. Check/uncheck "Enable Safety Limits"
5. Click "Save Safety Settings"

**REST API**:
```bash
curl -X POST http://[IP]/api/dosing/safety \
  -H "Content-Type: application/json" \
  -d '{
    "maxDose": 50.0,
    "maxDaily": 200.0,
    "enabled": true
  }'
```

### Error Handling

The system handles various error conditions:

#### Volume Limit Exceeded

```json
{
  "error": "Dose volume exceeds maximum limit"
}
```

Action: Reduce dose volume or increase limit

#### Daily Limit Reached

```json
{
  "error": "Daily volume limit exceeded"
}
```

Action: Wait until midnight or increase daily limit

#### Not Calibrated

```json
{
  "error": "Pump not calibrated"
}
```

Action: Complete calibration procedure

#### Invalid State

```json
{
  "error": "Cannot start in current state"
}
```

Action: Stop current operation first

### Emergency Stop

**Web Interface**:
- Large red "🛑 Emergency Stop" button
- Immediately stops all pump operations
- Safe to use anytime

**REST API**:
```bash
curl -X POST http://[IP]/api/dosing/stop
```

### Best Safety Practices

1. **Start Small**: Begin with small doses and observe effects
2. **Test with Water**: Test system with water before fertilizer
3. **Monitor Aquarium**: Watch plants/fish for signs of stress
4. **Keep Limits Enabled**: Only disable for testing
5. **Regular Calibration**: Ensures accurate volumes
6. **Backup Plan**: Have manual dosing method available
7. **Document Settings**: Record your dose volumes and schedule
8. **Gradual Changes**: Increase doses slowly over weeks

---

## Troubleshooting

### Pump Not Running

**Symptoms**: No motor sound, no liquid flow

**Checks**:
1. Verify power supply connected and on
2. Check web interface shows "calibrated"
3. Verify GPIO connections to DRV8871
4. Test with manual dose operation
5. Check motor driver power LED

**Solutions**:
- Reconnect loose wires
- Replace power supply if faulty
- Check ESP32 GPIO functionality
- Verify DRV8871 not in fault mode

### Inaccurate Volumes

**Symptoms**: Actual volume differs from expected

**Checks**:
1. Check calibration age (>30 days?)
2. Verify no air bubbles in tubing
3. Check for kinked or blocked tubing
4. Test flow rate with manual measurement
5. Verify consistent power supply voltage

**Solutions**:
- Recalibrate pump
- Prime to remove air
- Replace worn tubing
- Use regulated power supply
- Check motor speed at 100%

### Air in Tubing

**Symptoms**: Intermittent flow, bubbles visible

**Checks**:
1. Check fertilizer container level
2. Verify inlet submerged
3. Look for loose connections
4. Check tubing for cracks

**Solutions**:
- Prime pump for 20-30 seconds
- Refill fertilizer container
- Tighten all connections
- Replace damaged tubing
- Ensure no inlet leaks

### Pump Won't Prime

**Symptoms**: Motor runs but no liquid movement

**Checks**:
1. Verify tubing installed correctly
2. Check for blockage at inlet
3. Inspect pump rollers
4. Verify motor direction

**Solutions**:
- Reinstall tubing in pump head
- Clean inlet filter
- Run backflush operation
- Manually rotate motor shaft

### Schedule Not Triggering

**Symptoms**: Scheduled dose doesn't run

**Checks**:
1. Verify schedule enabled in web interface
2. Check ESP32 clock set correctly
3. Confirm pump calibrated
4. Check daily limit not exceeded
5. Verify WiFi/NTP time sync

**Solutions**:
- Enable schedule in web interface
- Set ESP32 time via NTP
- Complete calibration
- Reset daily volume counter
- Check system logs

### Flow Rate Changed

**Symptoms**: Calibration shows different flow rate

**Checks**:
1. Check tubing condition (wear/stretching)
2. Verify power supply voltage
3. Inspect pump rollers
4. Check motor speed consistency

**Solutions**:
- Replace tubing if worn
- Use regulated power supply
- Recalibrate with new tubing
- Service pump motor if needed

### Error Messages

#### "Pump not calibrated"
- **Cause**: No calibration data in NVS
- **Solution**: Complete calibration procedure

#### "Dose volume exceeds maximum limit"
- **Cause**: Requested volume > maxDoseVolume
- **Solution**: Reduce dose or increase limit

#### "Daily volume limit exceeded"
- **Cause**: Total daily volume > maxDailyVolume
- **Solution**: Wait until midnight or increase limit

#### "Cannot start in current state"
- **Cause**: Pump already running
- **Solution**: Stop current operation first

---

## API Reference

### GET /api/dosing/status

Get current dosing pump status.

**Response**:
```json
{
  "state": "idle",
  "calibrated": true,
  "flowRate": 0.850,
  "currentSpeed": 100,
  "targetVolume": 5.0,
  "volumePumped": 0.0,
  "progress": 0.0,
  "scheduleEnabled": true,
  "hoursUntilNext": 12.5,
  "dailyVolume": 15.0,
  "maxDailyVolume": 200.0,
  "safetyEnabled": true
}
```

### POST /api/dosing/start

Start manual dosing operation.

**Request**:
```json
{
  "volume": 5.0,
  "speed": 100
}
```

**Response**:
```json
{
  "status": "ok"
}
```

**Error Response**:
```json
{
  "error": "Pump not calibrated"
}
```

### POST /api/dosing/stop

Stop current dosing operation.

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/pause

Pause active dosing.

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/resume

Resume paused dosing.

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/prime

Prime pump (forward flow).

**Request**:
```json
{
  "duration": 10,
  "speed": 50
}
```

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/backflush

Backflush pump (reverse flow).

**Request**:
```json
{
  "duration": 10,
  "speed": 30
}
```

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/clean

Run cleaning cycles.

**Request**:
```json
{
  "cycles": 3
}
```

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/calibrate/start

Start calibration process.

**Request**:
```json
{
  "speed": 100
}
```

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/calibrate/finish

Finish calibration with measurements.

**Request**:
```json
{
  "measuredML": 25.5,
  "seconds": 30
}
```

**Response**:
```json
{
  "status": "ok",
  "flowRate": 0.850
}
```

### POST /api/dosing/calibrate/cancel

Cancel calibration process.

**Response**:
```json
{
  "status": "ok"
}
```

### GET /api/dosing/history

Get dosing history records.

**Parameters**:
- `count`: Number of records (1-100, default 20)

**Response**:
```json
{
  "history": [
    {
      "timestamp": 1234567890,
      "volumeDosed": 5.0,
      "duration": 6,
      "success": true,
      "type": "manual"
    }
  ]
}
```

### GET /api/dosing/schedule

Get current schedule configuration.

**Response**:
```json
{
  "enabled": true,
  "schedule": "daily",
  "customDays": 0,
  "hour": 9,
  "minute": 0,
  "doseVolume": 5.0,
  "lastDoseTime": 1234567890,
  "nextDoseTime": 1234654290
}
```

### POST /api/dosing/schedule

Set schedule configuration.

**Request**:
```json
{
  "enabled": true,
  "schedule": "daily",
  "customDays": 0,
  "hour": 9,
  "minute": 0,
  "doseVolume": 5.0
}
```

**Response**:
```json
{
  "status": "ok"
}
```

### POST /api/dosing/safety

Set safety limits.

**Request**:
```json
{
  "maxDose": 50.0,
  "maxDaily": 200.0,
  "enabled": true
}
```

**Response**:
```json
{
  "status": "ok"
}
```

### GET /api/dosing/stats

Get dosing statistics.

**Response**:
```json
{
  "totalRuntime": 3600,
  "totalDoses": 42,
  "averageVolume": 5.2,
  "totalVolume": 218.4,
  "daysSinceCalibration": 15,
  "lastCalibrationTime": 1234567890
}
```

---

## Web Interface Guide

### Dashboard Overview

The Dosing Pump tab provides complete control and monitoring.

#### Status Section

- **Pump State**: Current operation (IDLE/DOSING/PRIMING/etc.)
- **Calibration**: Shows if pump is calibrated
- **Flow Rate**: Displays calibrated mL/sec
- **Daily Volume**: Used vs. limit with percentage
- **Progress Bar**: Real-time dosing progress (when active)

#### Calibration Wizard

Step-by-step wizard guides you through calibration:

**Step 1**: Prepare container and select speed
**Step 2**: Collect output, timer shows elapsed time
**Step 3**: Enter measured volume
**Step 4**: Results displayed, calibration complete

#### Manual Controls

**Dose Now**:
- Enter volume (0.1 - 50 mL)
- Adjust speed (10 - 100%)
- Click "Dose Now" to start
- Click "Stop" to abort

**Maintenance**:
- Set duration (seconds)
- Prime: Forward flow at 50%
- Backflush: Reverse flow at 30%
- Run Cleaning: 3 alternating cycles

**Emergency Stop**:
- Large red button for immediate stop
- Stops any running operation
- Safe to use anytime

#### Schedule Configuration

- **Enable**: Checkbox to enable/disable automation
- **Frequency**: Daily, Weekly, or Custom interval
- **Custom Interval**: Specify days between doses
- **Time**: Hour and minute for scheduled dose
- **Volume**: Amount to dose (mL)
- **Next Dose**: Shows countdown to next scheduled dose

#### Safety Limits

- **Max Dose**: Maximum volume per operation
- **Max Daily**: Maximum total volume per day
- **Enable Toggle**: Turn safety limits on/off

#### History Table

Shows recent dosing records:
- Date/Time of dose
- Volume dosed (mL)
- Duration (seconds)
- Type (manual/scheduled/calibration)
- Success status (✓/✗)

#### Statistics

- **Total Doses**: Lifetime dose count
- **Total Volume**: Cumulative volume dosed
- **Average Dose**: Mean dose volume
- **Total Runtime**: Cumulative motor runtime
- **Days Since Calibration**: Warning if >30 days

### Using the Web Interface

#### Performing Manual Dose

1. Open web interface
2. Navigate to "💊 Dosing Pump" tab
3. Scroll to "Manual Controls"
4. Enter desired volume (e.g., 5.0 mL)
5. Adjust speed if needed (100% default)
6. Click "Dose Now"
7. Watch progress bar
8. System stops automatically at target volume

#### Setting Up Automated Schedule

1. Navigate to "Automated Schedule" section
2. Check "Enable Automated Dosing"
3. Select "Daily" frequency
4. Set time to 09:00
5. Enter dose volume: 5.0 mL
6. Click "Save Schedule"
7. Verify "Next dose in X hours" appears
8. System will dose automatically

#### Monitoring Operation

The web interface updates every 2 seconds:
- Status changes (idle → dosing → idle)
- Progress bar advances
- Volume pumped increases
- Daily volume accumulates
- History table updates

#### Reviewing History

1. Scroll to "Recent Dosing History"
2. Table shows last 20 doses
3. Click date/time to see details
4. Check success column for failures
5. History updates every 30 seconds

---

## Advanced Topics

### Integration with Home Automation

The REST API enables integration with home automation systems:

#### Home Assistant Example

```yaml
# configuration.yaml
rest_command:
  aquarium_dose_fertilizer:
    url: "http://192.168.1.100/api/dosing/start"
    method: POST
    content_type: "application/json"
    payload: '{"volume": 5.0, "speed": 100}'

sensor:
  - platform: rest
    name: "Aquarium Dosing Status"
    resource: "http://192.168.1.100/api/dosing/status"
    json_attributes:
      - state
      - calibrated
      - dailyVolume
    value_template: '{{ value_json.state }}'
    scan_interval: 10

automation:
  - alias: "Morning Fertilizer Dose"
    trigger:
      platform: time
      at: "09:00:00"
    action:
      service: rest_command.aquarium_dose_fertilizer
```

### MQTT Integration

While not implemented by default, MQTT topics could be added:

```
aquarium/dosing/status          # JSON status updates
aquarium/dosing/command         # Command topic
aquarium/dosing/history         # Dose completion events
```

### Custom Dose Amounts

For different tank sizes:

- **Nano tanks (<10L)**: 1-2 mL per dose
- **Small tanks (10-40L)**: 3-5 mL per dose
- **Medium tanks (40-100L)**: 5-10 mL per dose
- **Large tanks (>100L)**: 10-20 mL per dose

Adjust based on:
- Plant density
- Growth rate
- Fertilizer concentration
- Water change frequency

### Troubleshooting with Serial Monitor

Connect to ESP32 serial port (115200 baud):

```
Initializing dosing pump...
Flow rate: 0.850 mL/sec
Dosing pump initialized and calibrated

Starting manual dose: 5.0 mL
Dosing in progress... 2.5 mL pumped
Dosing complete: 5.0 mL in 6 seconds
```

### Backup and Restore

Calibration and configuration stored in NVS:

**Backup** (via API):
```bash
# Get current calibration
curl http://[IP]/api/dosing/status > dosing_backup.json

# Get schedule
curl http://[IP]/api/dosing/schedule >> dosing_backup.json
```

**Restore**:
- Recalibrate manually
- Set schedule via web interface
- Or use API to restore settings

---

## Frequently Asked Questions

### Q: How often should I dose fertilizer?

**A**: Depends on your setup:
- **High-tech planted**: Daily or every 2 days
- **Low-tech planted**: Weekly
- **Moderate planted**: Every 3-4 days

### Q: Can I use this for liquid CO2?

**A**: Yes, but consider:
- CO2 liquids may be corrosive
- Check tubing compatibility
- Adjust volumes carefully
- Monitor fish behavior closely

### Q: What if calibration seems wrong?

**A**: Try:
1. Recalibrate with longer duration (60 seconds)
2. Use more accurate measuring container
3. Check for air bubbles
4. Verify consistent power supply
5. Replace tubing if old/worn

### Q: Is it safe to leave running 24/7?

**A**: Yes, with precautions:
- Safety limits enabled
- Regular maintenance
- Monitor fertilizer container level
- Have backup plan for power failure
- Check regularly for leaks

### Q: Can I dose multiple fertilizers?

**A**: Not with single pump, but you could:
- Use multiple dosing pumps
- Alternate fertilizers by schedule
- Pre-mix fertilizers (if compatible)

### Q: What if power goes out?

**A**: System handles gracefully:
- Calibration saved in NVS
- Schedule persists
- Resumes normal operation on power restore
- May trigger overdue dose if scheduled

### Q: How do I know what volume to dose?

**A**: Follow fertilizer instructions:
1. Read manufacturer's dosing guide
2. Calculate for your tank volume
3. Start with 50% recommended dose
4. Increase gradually if needed
5. Monitor plant growth and algae

---

## Support and Resources

### Serial Console Logging

All dosing operations logged to serial:
```
[dosing] Manual dose started: 5.0 mL
[dosing] Calibration started at 100% speed
[dosing] Calibration complete: 0.850 mL/sec
[dosing] Priming pump for 10 seconds
[dosing] Schedule updated: daily
[dosing] Safety limits updated
```

### Event Logger

Events stored in `/events/` directory on SPIFFS:
- Manual dose events
- Scheduled dose events
- Calibration events
- Error events

### Further Reading

- DRV8871 Datasheet: [TI Product Page](https://www.ti.com/product/DRV8871)
- Peristaltic Pump Guide: Wikipedia
- Aquarium Fertilization: Various aquarium forums
- ESP32 Documentation: Espressif Systems

---

## Version History

### v1.0.0 - Initial Release
- Basic dosing functionality
- DRV8871 motor control
- Calibration system
- Manual and scheduled dosing
- Safety limits
- Web interface
- REST API
- History tracking
- NVS persistence

---

## License

This documentation is part of the ESP32 Aquarium Controller project.
See LICENSE file for details.

---

**Document Version**: 1.0.0  
**Last Updated**: 2024  
**Author**: ESP32 Aquarium Controller Project
