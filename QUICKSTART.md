# Quick Start Guide - ESP32 Aquarium Controller

## 🚀 Getting Started in 5 Steps

### Step 1: Hardware Assembly
1. Connect DS18B20 temperature sensor to GPIO 4 (with 4.7kΩ pullup to 3.3V)
2. Connect pH sensor analog output to GPIO 34
3. Connect TDS sensor analog output to GPIO 35
4. Connect heater relay to GPIO 26
5. Connect CO2 solenoid relay to GPIO 27
6. Connect all grounds together
7. Power sensors from 3.3V, relays from 5V

### Step 2: Flash Firmware
```bash
# Install PlatformIO if you haven't
pip install platformio

# Clone and navigate to project
cd aquariumcontroller

# Build and upload firmware
pio run --target upload

# Upload web interface files
pio run --target uploadfs
```

### Step 3: Initial WiFi Setup
1. Power on the ESP32
2. Look for WiFi network: `AquariumController` (password: `12345678`)
3. Connect and open browser to `http://192.168.4.1`
4. Go to Settings tab and enter your WiFi credentials
5. Save and restart

### Step 4: Calibrate pH Sensor
1. Navigate to pH Calibration tab
2. Click "Start Calibration"
3. Place probe in pH 4.0 buffer → Click "Calibrate pH 4.0"
4. Rinse probe with distilled water
5. Place probe in pH 7.0 buffer → Click "Calibrate pH 7.0"
6. Rinse probe with distilled water
7. Place probe in pH 10.0 buffer → Click "Calibrate pH 10.0"
8. Click "Save Calibration"

### Step 5: Set Your Targets
1. Go to Control tab
2. Set Temperature Target (e.g., 25.0°C)
3. Set pH Target (e.g., 6.8 for planted tank)
4. Click "Update Targets"

## ✅ You're Done!

The system will now:
- ✓ Read sensors every second
- ✓ Control heater to maintain temperature
- ✓ Control CO2 to maintain pH
- ✓ Learn and optimize PID parameters
- ✓ Display live data on web interface
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
- `aquarium/temperature`
- `aquarium/ph`
- `aquarium/tds`
- `aquarium/heater`
- `aquarium/co2`

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

- Fine-tune PID parameters if needed
- Set up MQTT for external monitoring
- Add temperature alerts to your phone
- Connect to Home Assistant
- Monitor and adjust based on fish behavior

## 🆘 Need Help?

1. Check the full README.md for detailed documentation
2. Monitor serial output: `pio device monitor -b 115200`
3. Check GitHub issues
4. Review safety warnings in README.md

**Happy Fishkeeping! 🐠**
