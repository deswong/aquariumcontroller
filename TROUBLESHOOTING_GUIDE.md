# Aquarium Controller Troubleshooting Guide

## Table of Contents
1. [Quick Diagnostics](#quick-diagnostics)
2. [WiFi Connection Issues](#wifi-connection-issues)
3. [Sensor Problems](#sensor-problems)
4. [Control System Issues](#control-system-issues)
5. [Web Interface Problems](#web-interface-problems)
6. [MQTT Connection Issues](#mqtt-connection-issues)
7. [Display Problems](#display-problems)
8. [Memory and Performance Issues](#memory-and-performance-issues)
9. [ML/PID Controller Issues](#mlpid-controller-issues)
10. [Recovery Procedures](#recovery-procedures)

---

## Quick Diagnostics

### System Health Check

1. **Check Serial Monitor** (115200 baud):
   - Look for initialization messages
   - Check for error messages
   - Verify sensor readings

2. **Check Status LED** (if configured):
   - **Solid On**: Normal operation
   - **Slow Blink**: Warning condition
   - **Fast Blink**: Error condition
   - **Breathing**: Access Point mode

3. **Check Web Interface**:
   - Access `http://<device-ip>/`
   - Check all sensors show valid readings
   - Verify no red error indicators

4. **Check MQTT** (if configured):
   - Subscribe to `aquarium/#`
   - Verify data is being published

### Getting Diagnostic Information

Access the following endpoints for diagnostics:
```bash
# System status
curl http://<device-ip>/api/data

# Event logs
curl http://<device-ip>/api/logs?limit=100

# Heap memory status
curl http://<device-ip>/api/monitor/heap

# Task status
curl http://<device-ip>/api/monitor/tasks
```

---

## WiFi Connection Issues

### Problem: Device Won't Connect to WiFi

**Symptoms:**
- Device enters Access Point mode instead of connecting
- Serial monitor shows "WiFi connection failed"
- Cannot access web interface on home network

**Solutions:**

1. **Verify Credentials**:
   ```cpp
   // Check WiFi settings in web interface or NVS
   // SSID and password are case-sensitive
   ```

2. **Check WiFi Network**:
   - Ensure 2.4 GHz network (ESP32-S3 doesn't support 5 GHz)
   - Verify network is not hidden
   - Check router is not blocking MAC address
   - Ensure DHCP is enabled on router

3. **Signal Strength**:
   - Move device closer to router
   - Check antenna connection (if external)
   - Monitor RSSI value (should be > -80 dBm)

4. **Reset WiFi Configuration**:
   ```bash
   # Access AP mode
   # Connect to "AquariumController" WiFi (password: 12345678)
   # Navigate to http://192.168.4.1
   # Re-enter WiFi credentials
   ```

5. **Factory Reset**:
   - Upload firmware with erase_flash option
   - Or use PlatformIO: `pio run -t erase -t upload`

### Problem: Frequent Disconnections

**Symptoms:**
- Device connects but frequently drops connection
- MQTT disconnects repeatedly
- Web interface becomes unresponsive

**Solutions:**

1. **Check Power Supply**:
   - Ensure stable 5V supply with adequate current (>500mA)
   - Try different USB cable/power adapter
   - Check for brownouts in serial monitor

2. **Monitor Signal Strength**:
   ```bash
   curl http://<device-ip>/api/data
   # Check "wifiRSSI" value
   # Should be > -70 dBm for stable connection
   ```

3. **Check Router Settings**:
   - Disable "AP Isolation"
   - Increase DHCP lease time
   - Assign static IP if possible
   - Update router firmware

4. **Reduce Interference**:
   - Move away from microwave ovens
   - Avoid 2.4 GHz cordless phones
   - Change WiFi channel on router

---

## Sensor Problems

### Temperature Sensor Issues

**Problem: Temperature shows -127°C or invalid readings**

**Causes & Solutions:**

1. **Sensor Not Detected**:
   - Check wiring (Data, VCC, GND)
   - Verify 4.7kΩ pull-up resistor on data line
   - Test sensor with known-good setup
   - Replace sensor if faulty

2. **Intermittent Readings**:
   - Check for loose connections
   - Verify waterproof seal integrity
   - Monitor serial output for connection errors
   - Consider cable length (max 100m for DS18B20)

3. **Slow Response**:
   - Normal: DS18B20 has ~750ms conversion time
   - Check resolution setting (9-12 bit)
   - Verify sensor is properly immersed

**Problem: Sensor Anomaly Detected**

The system now detects sensor anomalies:

1. **Stuck Sensor**:
   - Reading hasn't changed for >5 minutes
   - Check if sensor is frozen or disconnected
   - Verify water circulation around sensor

2. **Spike Detection**:
   - Sudden change >5°C detected
   - May indicate sensor failure or actual rapid temp change
   - Check system logs for context

3. **Out of Range**:
   - Reading outside 10-40°C range
   - Check sensor placement (not in heater stream)
   - Verify calibration

### pH Sensor Issues

**Problem: pH readings unstable or incorrect**

**Solutions:**

1. **Calibration Required**:
   ```bash
   # Perform 2-point or 3-point calibration
   # Use fresh calibration solutions
   # Follow calibration procedure in docs
   ```

2. **Probe Maintenance**:
   - Clean probe with soft brush
   - Check reference electrolyte level
   - Store in storage solution (not water!)
   - Replace probe if >12 months old

3. **Electrical Noise**:
   - Keep probe cable away from power lines
   - Use shielded cable if possible
   - Check grounding

4. **Temperature Compensation**:
   - System automatically compensates using tank temp
   - Ensure temperature sensor is working

### TDS Sensor Issues

**Problem: TDS readings incorrect or fluctuating**

**Solutions:**

1. **Calibration**:
   - Use TDS calibration solution (typically 1413 µS/cm)
   - Ensure probe is clean
   - Temperature affects readings

2. **Probe Contamination**:
   - Clean with soft brush
   - Rinse with distilled water
   - Check for mineral buildup

3. **Installation**:
   - Ensure probe is fully submerged
   - Avoid air bubbles on electrodes
   - Keep away from heater/powerhead flow

---

## Control System Issues

### Problem: Heater/CO2 Not Turning On

**Symptoms:**
- Temperature below target but heater relay not activating
- pH above target but CO2 relay not activating

**Diagnostics:**

1. **Check Emergency Stop**:
   ```bash
   curl http://<device-ip>/api/data
   # Look for "emergencyStop": true
   ```
   If active:
   ```bash
   curl -X POST http://<device-ip>/api/emergency/clear
   ```

2. **Check Relay Status**:
   - Listen for relay click when state changes
   - Test relay with multimeter
   - Verify relay board power supply

3. **Check Wiring**:
   - Verify GPIO pin assignments match configuration
   - Check for loose connections
   - Ensure relay polarity correct (NO vs NC)

4. **Check PID Controller**:
   ```bash
   # Get PID health status
   curl http://<device-ip>/api/pid/temp/health
   ```
   Look for:
   - `outputStuck`: true → PID may have issue
   - `outputSaturation`: true → Output at limits

### Problem: Temperature/pH Oscillating

**Symptoms:**
- Temperature cycles above and below target
- pH swings widely

**Solutions:**

1. **PID Tuning**:
   - Current PID values may be too aggressive
   - Use ML adaptation to auto-tune (if enabled)
   - Or manually adjust via web interface

2. **Check Control Period**:
   - Default: heater cycles every 10 seconds
   - For large tanks, may need longer cycle time
   - Adjust in configuration

3. **Hardware Issues**:
   - Heater too powerful for tank size
   - CO2 solenoid cycling too fast
   - Check for proper water circulation

4. **Sensor Placement**:
   - Temperature sensor too close to heater
   - pH probe in dead zone
   - Relocate sensors to representative locations

---

## Web Interface Problems

### Problem: Cannot Access Web Interface

**Solutions:**

1. **Find Device IP**:
   ```bash
   # Check serial monitor for IP address
   # Or check router DHCP leases
   # Or use network scanner: sudo nmap -sn 192.168.1.0/24
   ```

2. **Check Browser**:
   - Try different browser
   - Clear browser cache
   - Disable browser extensions
   - Try incognito/private mode

3. **Check Firewall**:
   - Ensure port 80 not blocked
   - Try from different device on same network

### Problem: Web Interface Slow or Unresponsive

**Solutions:**

1. **Check Memory**:
   ```bash
   curl http://<device-ip>/api/monitor/heap
   # If usagePercent > 90%, memory issue
   ```

2. **Reduce Polling Rate**:
   - Dashboard polls every 1 second by default
   - Increase interval in browser console:
   ```javascript
   localStorage.setItem('pollInterval', '2000'); // 2 seconds
   ```

3. **Clear Event Logs**:
   ```bash
   curl -X DELETE http://<device-ip>/api/logs
   ```

---

## MQTT Connection Issues

### Problem: MQTT Not Connecting

**Solutions:**

1. **Verify MQTT Settings**:
   - Check server address and port
   - Verify username/password if required
   - Check topic prefix

2. **Test MQTT Broker**:
   ```bash
   # Test with mosquitto_sub
   mosquitto_sub -h <broker> -p 1883 -t "aquarium/#" -v
   ```

3. **Check Network**:
   - Ensure broker accessible from device's network
   - Check firewall rules
   - Verify DNS resolution

4. **Monitor Connection**:
   - Check event logs for MQTT errors
   - Watch serial monitor during connection attempts

---

## Display Problems

### Problem: OLED Display Blank

**Solutions:**

1. **Check Connections**:
   - Verify I2C wiring (SDA, SCL, VCC, GND)
   - Default I2C address: 0x3C
   - Try I2C scanner sketch to detect device

2. **Check Power**:
   - OLED needs 3.3V or 5V (check your model)
   - Ensure adequate current supply

3. **Check I2C Address**:
   ```cpp
   // In code, verify:
   // SSD1306: usually 0x3C
   // SSD1309: usually 0x3C or 0x3D
   ```

### Problem: Display Corrupted or Flickering

**Solutions:**

1. **Reduce Update Rate**:
   - Default: 1 second updates
   - Increase interval if flickering

2. **Check I2C Speed**:
   - Default: 400 kHz
   - Reduce to 100 kHz if issues persist

3. **Cable Length**:
   - Keep I2C cables <30cm
   - Use pull-up resistors if longer cables needed

---

## Memory and Performance Issues

### Problem: System Crashes or Reboots

**Diagnostics:**

1. **Check Serial Output**:
   - Look for stack traces
   - Note exception type
   - Check for "Guru Meditation Error"

2. **Monitor Memory**:
   ```bash
   curl http://<device-ip>/api/monitor/heap
   ```
   - If `minFreeHeap` < 10KB → memory leak likely
   - Check for fragmentation

3. **Check Task Stacks**:
   ```bash
   curl http://<device-ip>/api/monitor/tasks
   ```
   - Look for `stackUsagePercent` > 80%
   - Increase stack size if needed

**Solutions:**

1. **Reduce Feature Load**:
   - Disable ML features if not needed
   - Reduce event log size
   - Decrease sensor read frequency

2. **Memory Leak**:
   - System automatically detects leaks
   - Check logs for leak warnings
   - Update to latest firmware

3. **Stack Overflow**:
   - Increase task stack size in SystemTasks.cpp
   - Reduce recursive function calls

### Problem: Slow Performance

**Solutions:**

1. **Check CPU Usage**:
   ```bash
   curl http://<device-ip>/api/pid/temp/profiler
   # Check cpuUsagePercent
   ```

2. **Disable Debug Logging**:
   - Compile with production build flags
   - Use `esp32s3dev` not `esp32s3dev-debug`

3. **Optimize Sensor Reads**:
   - Increase sensor read intervals
   - Use averaged readings

---

## ML/PID Controller Issues

### Problem: ML Features Not Working

**Solutions:**

1. **Check ML Model**:
   ```bash
   # Get ML model info
   curl http://<device-ip>/api/pid/temp/model
   ```
   - Verify model version and validity

2. **Check Dual-Core Status**:
   ```bash
   curl http://<device-ip>/api/pid/temp/dualcore
   ```
   - Ensure ML task is running

3. **Check PSRAM**:
   - ML features require PSRAM
   - Verify `BOARD_HAS_PSRAM` defined
   - Check `api/monitor/heap` for PSRAM info

### Problem: Poor Control Performance

**Solutions:**

1. **Check Health Metrics**:
   ```bash
   curl http://<device-ip>/api/pid/temp/health
   ```
   - `outputStuck`: Output not responding to error
   - `persistentHighError`: Error remains high
   - `outputSaturation`: Output at limits often

2. **Review Kalman Filter**:
   ```bash
   curl http://<device-ip>/api/pid/temp/kalman
   ```
   - Adjust process/measurement noise if needed

3. **Feed-Forward Check**:
   ```bash
   curl http://<device-ip>/api/pid/temp/feedforward
   ```
   - Verify contributions are reasonable

---

## Recovery Procedures

### Factory Reset

**Method 1: Via PlatformIO**
```bash
cd /path/to/aquariumcontroller
pio run -t erase       # Erase all flash
pio run -t upload      # Upload fresh firmware
pio run -t uploadfs    # Upload web files
```

**Method 2: Via Web Interface**
- Access `/api/config/reset` (if implemented)
- Or manually delete NVS partitions

### Backup and Restore

**Backup Configuration:**
```bash
curl http://<device-ip>/api/config/export -o backup.json
```

**Restore Configuration:**
```bash
curl -X POST http://<device-ip>/api/config/import \
  -F "config=@backup.json"
```

### Debug Build

For detailed debugging:
```bash
# Use debug environment
pio run -e esp32s3dev-debug -t upload
pio device monitor

# Enable all logging
# Set LOG_LEVEL_COMPILE_TIME=0 in platformio.ini
```

---

## Common Error Messages

### "WiFi connection lost!"
- **Cause**: WiFi signal too weak or router issue
- **Solution**: Improve signal strength or check router

### "NTP time synchronization failed"
- **Cause**: NTP server unreachable
- **Solution**: Check internet connection, try different NTP server

### "Temperature sensor disconnected"
- **Cause**: DS18B20 not detected on OneWire bus
- **Solution**: Check wiring and sensor

### "Heap fragmentation detected"
- **Cause**: Memory fragmentation from allocations
- **Solution**: Usually benign, but monitor for leaks

### "Watchdog timeout for task: SensorTask"
- **Cause**: Task taking too long
- **Solution**: Check for blocking operations

### "ML model invalid or not loaded"
- **Cause**: ML model file missing or corrupted
- **Solution**: Upload ML model to SPIFFS partition

---

## Getting Help

### Information to Provide

When seeking help, please provide:

1. **System Information**:
   ```bash
   curl http://<device-ip>/api/data
   ```

2. **Event Logs**:
   ```bash
   curl http://<device-ip>/api/logs?limit=100
   ```

3. **Configuration**:
   ```bash
   curl http://<device-ip>/api/settings
   ```

4. **Serial Output**:
   - Copy last 100-200 lines from serial monitor
   - Include any error messages or stack traces

5. **Hardware Setup**:
   - ESP32-S3 model and flash size
   - Sensor types and models
   - Wiring diagram if custom

### Support Channels

- GitHub Issues: [Repository URL]
- Documentation: See README.md files
- Community Forum: [If applicable]

---

## Prevention and Best Practices

### Regular Maintenance

1. **Weekly**:
   - Check sensor readings for anomalies
   - Review event logs for warnings
   - Verify WiFi signal strength

2. **Monthly**:
   - Clean pH probe
   - Check TDS sensor
   - Backup configuration
   - Review PID performance

3. **Quarterly**:
   - Calibrate pH sensor
   - Update firmware if new version
   - Check heater/relay operation

### Monitoring

Set up monitoring to catch issues early:
- Enable MQTT alerts
- Use notification system
- Monitor heap usage trends
- Track PID performance metrics

---

**Version:** 2.0.0  
**Last Updated:** October 2025  
**Compatible with:** ESP32-S3 Aquarium Controller v2.0+
