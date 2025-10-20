# Quick Start Guide - ESP32-S3 Aquarium Controller

⚡ **Hardware Requirement**: This project requires **ESP32-S3 with PSRAM** (ESP32-S3-DevKitC-1 recommended)

🇦🇺 **Australian Users**: See [AUSTRALIAN_CONFIGURATION.md](AUSTRALIAN_CONFIGURATION.md) for Australian-specific setup (240V AC, AEST timezone, au.pool.ntp.org)

## 🚀 Getting Started in 6 Steps

### Step 0: Verify Your Hardware

**Required:**
- ESP32-S3-DevKitC-1 (or compatible with PSRAM)
- 8MB Flash + 8MB PSRAM
- USB-C cable for programming

**To check if you have the right board:**
```bash
# After connecting via USB
pio device list
# Should show: ESP32-S3
```

### Step 1: Hardware Assembly

**Core Sensors (Required - 8 pins):**
1. Connect water temp DS18B20 to GPIO 4 (with 4.7kΩ pullup to 3.3V)
2. Connect ambient temp DS18B20 to GPIO 5 (with 4.7kΩ pullup to 3.3V)
3. Connect pH sensor analog output to GPIO 34
4. Connect TDS sensor analog output to GPIO 35
5. Connect heater relay to GPIO 26 (⚠️ 240V AC in Australia - see electrical safety)
6. Connect CO2 solenoid relay to GPIO 27
7. Connect dosing pump IN1 to GPIO 25, IN2 to GPIO 33
8. Connect all grounds together
9. Power sensors from 3.3V, relays from 5V

**Display (Optional - 2 pins with ESP32-S3):**
10. Connect SSD1309 OLED Display (I2C):
    - SDA → GPIO 21
    - SCL → GPIO 22
    - VCC → 3.3V
    - GND → GND

⚠️ **Australian Electrical Safety (AS/NZS 3000:2018)**:
- 240V AC circuits MUST use RCD protection (30mA)
- Licensed electrician required for mains wiring
- Use IP-rated enclosures near water

See [PINOUT.md](PINOUT.md) for complete wiring diagrams.

### Step 2: Flash Firmware
```bash
# Install PlatformIO if you haven't
pip install platformio

# Clone and navigate to project
cd aquariumcontroller

# Build and upload firmware for ESP32-S3
pio run -e esp32s3dev --target upload

# Upload web interface files
pio run -e esp32s3dev --target uploadfs
```

**Note:** The default environment is now `esp32s3dev`. Legacy ESP32 support is available but deprecated.

### Step 3: Initial WiFi Setup
1. Power on the ESP32-S3 (via USB-C or 5V power)
2. Look for WiFi network: `AquariumController` (password: `12345678`)
3. Connect and open browser to `http://192.168.4.1`
4. Go to Settings tab and enter your WiFi credentials
5. Save and restart

**ESP32-S3 USB Note:** The board has native USB support, so no external USB-to-Serial chip is needed!

### Step 4: Configure Australian Settings 🇦🇺 (if applicable)
1. Go to Settings tab
2. Set timezone to AEST (UTC+10)
3. Set NTP server to `au.pool.ntp.org`
4. Enable daylight saving time
5. Verify electrical standards (240V AC, 50Hz, RCD 30mA)
6. Save settings

See [AUSTRALIAN_CONFIGURATION.md](AUSTRALIAN_CONFIGURATION.md) for details.

### Step 5: Calibrate pH Sensor
1. Navigate to pH Calibration tab (or use LCD display if installed)
2. Click "Start Calibration"
3. Place probe in pH 4.0 buffer → Click "Calibrate pH 4.0"
4. Rinse probe with distilled water
5. Place probe in pH 7.0 buffer → Click "Calibrate pH 7.0"
6. Rinse probe with distilled water
7. Place probe in pH 10.0 buffer → Click "Calibrate pH 10.0"
8. Click "Save Calibration"

**Note**: System uses ambient temperature sensor (GPIO 5) during calibration, water temperature (GPIO 4) for readings. See [PH_CALIBRATION_GUIDE.md](PH_CALIBRATION_GUIDE.md).

### Step 6: Set Your Targets
1. Go to Control tab (or use LCD display)
2. Set Temperature Target (e.g., 25.0°C)
3. Set pH Target (e.g., 6.8 for planted tank)
4. Click "Update Targets"

## ✅ You're Done!

The system will now:
- ✓ Read sensors every second
- ✓ Control heater to maintain temperature (240V AC 🇦🇺)
- ✓ Control CO2 to maintain pH
- ✓ Learn and optimize PID parameters
- ✓ Display live data on web interface
- ✓ Show status on LCD display (if installed)
- ✓ Sync time with NTP (au.pool.ntp.org 🇦🇺)
- ✓ Learn patterns and predict water changes
- ✓ Control dosing pump (if installed)
- ✓ Publish data to MQTT (if configured)
- ✓ Store all settings in flash memory

## 🔧 Optional: MQTT Setup

If you want to integrate with Home Assistant or other systems:

1. Go to Settings tab
2. Enter your MQTT broker IP address
3. Enter port (usually 1883)
4. Enter username/password if required
5. Save and restart

### MQTT Topics
Your data will be published to:
- `aquarium/temperature` - Water temperature (°C)
- `aquarium/ambient_temperature` - Air/room temperature (°C)
- `aquarium/ph` - pH (temperature compensated)
- `aquarium/tds` - TDS (ppm)
- `aquarium/heater` - Heater state (ON/OFF, 240V AC 🇦🇺)
- `aquarium/co2` - CO2 solenoid state
- `aquarium/dosing_pump` - Dosing pump status (if installed)
- `aquarium/display` - Display data (JSON, if installed)
- `aquarium/water_change` - Water change predictions
- `aquarium/patterns` - Pattern learning analytics

## ⚠️ Important Safety Notes

1. **Test First**: Run system for 24 hours WITHOUT livestock
2. **Set Safety Limits**: 
   - Temperature Max: 30°C (adjust in code)
   - pH Min: 6.0 (adjust in code)
3. **Monitor Closely**: Check system multiple times daily for first week
4. **Have Backup**: Keep battery-powered air pump as backup
5. **CO2 Warning**: Start with conservative pH target (6.8), then adjust based on fish behavior

## 📱 Accessing Your Controller

After WiFi is configured, access at:
- **Dashboard**: `http://<device-ip>/`
- **OTA Updates**: `http://<device-ip>/update`

Find device IP in:
- Serial monitor (connect via USB)
- Router's DHCP client list
- Use network scanner app

## 🔄 Updating Firmware

### Via Web Interface:
1. Build new firmware: `pio run`
2. Find `.pio/build/esp32dev/firmware.bin`
3. Go to `http://<device-ip>/update`
4. Upload firmware file

### Via USB:
```bash
pio run --target upload
```

## 🐛 Common Issues

**"No temperature sensor found"**
- Check DS18B20 wiring
- Verify 4.7kΩ pullup resistor installed
- Try different GPIO pin

**"pH readings unstable"**
- Calibrate the sensor
- Ensure probe is properly hydrated
- Check BNC connection

**"Heater won't turn on"**
- Check relay wiring
- Verify relay module type (active high/low)
- Check emergency stop status

**"Can't connect to WiFi"**
- ESP32 only supports 2.4GHz WiFi
- Check password carefully
- Try power cycling
- Connect to AP mode to reconfigure

## 📚 Next Steps

- **Display**: Install LCD display for at-a-glance monitoring (see [ENDER3_DISPLAY_WIRING.md](ENDER3_DISPLAY_WIRING.md))
- **Dosing**: Set up automated nutrient dosing (see [DOSING_PUMP_GUIDE.md](DOSING_PUMP_GUIDE.md))
- **Patterns**: Enable pattern learning for anomaly detection (see [PATTERN_LEARNING.md](PATTERN_LEARNING.md))
- **Water Changes**: Use water change predictor (see [WATER_CHANGE_ASSISTANT.md](WATER_CHANGE_ASSISTANT.md))
- **Fine-tune**: Adjust PID parameters if needed
- **MQTT**: Set up for external monitoring and Home Assistant
- **Alerts**: Add temperature/pH alerts to your phone
- **Monitor**: Watch fish behavior and adjust targets accordingly

## 🆘 Need Help?

1. Check the full [README.md](README.md) for detailed documentation
2. Monitor serial output: `pio device monitor -b 115200`
3. Review 30+ documentation files for specific features
4. Check GitHub issues
5. 🇦🇺 Australian users: See [AUSTRALIAN_CONFIGURATION.md](AUSTRALIAN_CONFIGURATION.md)
6. Display issues: See [DISPLAY_TESTS.md](DISPLAY_TESTS.md)

**Happy Fishkeeping! 🐠🎯🇦🇺**
