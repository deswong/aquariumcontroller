# Dosing Pump Feature - Quick Start

## Overview

The fertilizer dosing pump system has been successfully integrated into your ESP32 Aquarium Controller! This feature provides automated, precise liquid fertilizer dosing with web-based control.

## What's Been Added

### 1. Core Implementation (1,070 lines)
- **DosingPump.h**: Complete class definition with 40+ methods
- **DosingPump.cpp**: Full implementation with motor control, calibration, scheduling, safety

### 2. System Integration
- **main.cpp**: Initialization code, GPIO configuration, update loop integration
- **SystemTasks.h/cpp**: Global dosing pump object declarations
- **WebServer.cpp**: 16 REST API endpoints for complete control

### 3. Web Interface (600+ lines)
- **data/index.html**: Complete "Dosing Pump" tab with:
  - Real-time status dashboard
  - Step-by-step calibration wizard
  - Manual dosing controls
  - Maintenance functions (prime, backflush, clean)
  - Automated schedule configuration
  - Safety limit settings
  - History table
  - Statistics display

### 4. Documentation
- **DOSING_PUMP_GUIDE.md**: Comprehensive 900+ line guide covering:
  - Hardware setup and wiring
  - Calibration procedures
  - Operation instructions
  - Maintenance schedules
  - Troubleshooting
  - Complete API reference

## Hardware Requirements

- **DRV8871 Motor Driver**: DC motor driver board
- **Peristaltic Pump**: Food-grade pump with 12V DC motor
- **Tubing**: Food-grade silicone tubing (4mm ID recommended)
- **Power Supply**: 12V DC regulated power supply
- **Connections**:
  - GPIO 25 → DRV8871 IN1
  - GPIO 26 → DRV8871 IN2
  - GND → GND
  - 12V power to DRV8871 VM and motor

## Quick Start Guide

### 1. Upload Code
```bash
pio run --target upload
```

### 2. Connect Hardware
- Wire DRV8871 to ESP32 (GPIO 25, 26, GND)
- Connect 12V power to DRV8871
- Connect pump motor to DRV8871 outputs
- Install tubing in pump

### 3. Access Web Interface
- Navigate to `http://[ESP32-IP]/`
- Click "💊 Dosing Pump" tab

### 4. Calibrate Pump
Follow the on-screen calibration wizard:
1. Prepare graduated container
2. Click "Start Calibration"
3. Collect output for 30+ seconds
4. Click "Stop & Measure"
5. Enter measured volume
6. Click "Finish Calibration"

### 5. Configure Schedule (Optional)
1. Enable "Automated Dosing"
2. Select frequency (Daily/Weekly/Custom)
3. Set time and volume
4. Click "Save Schedule"

### 6. Test Manual Dose
1. Enter small volume (e.g., 2 mL)
2. Click "Dose Now"
3. Verify correct operation

## Key Features

### Calibration System
- Wizard-guided calibration process
- Calculates precise flow rate (mL/sec)
- Stored in NVS (survives reboots)
- Warning after 30 days

### Automated Scheduling
- Daily, weekly, or custom intervals
- Specific time-of-day scheduling
- Automatic execution
- Overdue dose handling

### Safety Limits
- Max dose per operation (default 50 mL)
- Max daily volume (default 200 mL)
- Prevents overdosing
- Daily reset at midnight

### Maintenance Functions
- **Prime**: Forward flow to fill tubing
- **Backflush**: Reverse flow to clear blockages
- **Clean**: Multi-cycle cleaning sequence
- **Emergency Stop**: Immediate shutdown

### History & Statistics
- Records all doses with timestamps
- Success/failure tracking
- Total doses and volume
- Runtime tracking
- Days since calibration

## REST API Endpoints

### Status & Control
- `GET /api/dosing/status` - Current status
- `POST /api/dosing/start` - Start manual dose
- `POST /api/dosing/stop` - Stop operation
- `POST /api/dosing/pause` - Pause dosing
- `POST /api/dosing/resume` - Resume dosing

### Calibration
- `POST /api/dosing/calibrate/start` - Begin calibration
- `POST /api/dosing/calibrate/finish` - Complete calibration
- `POST /api/dosing/calibrate/cancel` - Cancel calibration

### Maintenance
- `POST /api/dosing/prime` - Prime pump
- `POST /api/dosing/backflush` - Backflush
- `POST /api/dosing/clean` - Run cleaning

### Configuration
- `GET /api/dosing/schedule` - Get schedule
- `POST /api/dosing/schedule` - Set schedule
- `POST /api/dosing/safety` - Set safety limits

### Data
- `GET /api/dosing/history` - Dose history
- `GET /api/dosing/stats` - Statistics

## Configuration

### Default Settings (main.cpp)
```cpp
// GPIO Pins
GPIO 25: DRV8871 IN1
GPIO 26: DRV8871 IN2
PWM Channel: 1

// Safety Limits
Max Dose: 50.0 mL
Max Daily: 200.0 mL
Safety Enabled: true

// Default Schedule
Enabled: false (until calibrated)
Frequency: Weekly
Time: 09:00
Volume: 5.0 mL
```

### Changing GPIO Pins
Edit `src/main.cpp` around line 236:
```cpp
dosingPump = new DosingPump(25, 26, 1);  // Change pins here
```

### Adjusting Safety Limits
Via web interface or API:
```bash
curl -X POST http://[IP]/api/dosing/safety \
  -H "Content-Type: application/json" \
  -d '{"maxDose": 100.0, "maxDaily": 500.0, "enabled": true}'
```

## Web Interface Screenshot Guide

### Status Dashboard
Shows current state, calibration status, flow rate, and daily volume usage with progress bar during dosing.

### Calibration Wizard
4-step process:
1. Prepare and set speed
2. Run pump and collect output
3. Enter measurements
4. View results

### Manual Controls
- Dose now with volume and speed sliders
- Maintenance buttons (prime, backflush, clean)
- Emergency stop button

### Schedule Configuration
- Enable/disable toggle
- Frequency selector
- Time picker
- Volume input
- Next dose countdown

### History Table
Recent doses with date, volume, duration, type, and success status.

## Troubleshooting

### Pump Not Running
1. Check power supply connected
2. Verify GPIO wiring
3. Check web interface shows "calibrated"
4. Test with manual dose

### Inaccurate Volumes
1. Recalibrate pump
2. Check for air bubbles
3. Prime pump
4. Verify tubing condition

### Schedule Not Triggering
1. Verify schedule enabled
2. Check ESP32 time (NTP sync)
3. Confirm pump calibrated
4. Check daily limit not exceeded

### Web Interface Issues
1. Hard refresh browser (Ctrl+F5)
2. Clear browser cache
3. Check browser console for errors
4. Verify SPIFFS uploaded

## Maintenance Schedule

| Task | Frequency | Time |
|------|-----------|------|
| Visual inspection | Daily | 1 min |
| Check fertilizer level | Daily | 1 min |
| Prime pump | Weekly | 2 min |
| Clean inlet filter | Weekly | 5 min |
| Run cleaning cycle | Monthly | 10 min |
| Recalibrate | Monthly | 5 min |
| Replace tubing | Quarterly | 15 min |

## Safety Recommendations

1. **Always calibrate** before first use
2. **Start small** - test with water first
3. **Monitor aquarium** for plant/fish response
4. **Keep safety limits enabled**
5. **Regular maintenance** prevents issues
6. **Document settings** for reference
7. **Have backup plan** (manual dosing)

## Integration Examples

### Home Assistant
```yaml
sensor:
  - platform: rest
    name: "Aquarium Dosing Status"
    resource: "http://192.168.1.100/api/dosing/status"
    value_template: '{{ value_json.state }}'
```

### Node-RED
```javascript
// Trigger daily dose
msg.url = "http://192.168.1.100/api/dosing/start";
msg.payload = {volume: 5.0, speed: 100};
return msg;
```

### Python Script
```python
import requests

# Manual dose
response = requests.post(
    'http://192.168.1.100/api/dosing/start',
    json={'volume': 5.0, 'speed': 100}
)
print(response.json())
```

## Files Modified/Created

### Created Files
- `include/DosingPump.h` (170 lines)
- `src/DosingPump.cpp` (900 lines)
- `DOSING_PUMP_GUIDE.md` (900 lines)
- `DOSING_PUMP_README.md` (this file)

### Modified Files
- `include/SystemTasks.h` (added DosingPump include and extern)
- `src/SystemTasks.cpp` (added global variable)
- `src/main.cpp` (added initialization and update)
- `src/WebServer.cpp` (added 16 API endpoints, 470 lines)
- `data/index.html` (added dosing tab, 650 lines)

## Next Steps

1. **Build and upload** firmware to ESP32
2. **Upload SPIFFS** data (includes web interface)
3. **Connect hardware** per wiring guide
4. **Run calibration** using web wizard
5. **Test manual dose** with small volume
6. **Configure schedule** for automation
7. **Monitor operation** and adjust as needed

## Support

For detailed information, see:
- **DOSING_PUMP_GUIDE.md** - Complete documentation (900+ lines)
- **Serial Monitor** - Runtime logs and debugging
- **Event Logger** - Stored events in `/events/` on SPIFFS

## Version Information

- **Feature Version**: 1.0.0
- **Lines of Code**: 2,500+
- **API Endpoints**: 16
- **Documentation**: 1,800+ lines
- **Status**: Complete and ready to use

---

**Feature Complete!** The dosing pump system is fully integrated and ready for use. Follow the quick start guide above to begin automated fertilizer dosing.
