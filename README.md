# 🐠 ESP32 Aquarium Controller

A comprehensive aquarium automation system built for ESP32 that monitors and controls temperature, pH (CO2), and TDS with self-learning PID control, web interface, and MQTT integration.

**🇦🇺 Configured for Australian conditions:** 240V AC, Celsius, Australian time zones, and local water parameters. See [AUSTRALIAN_CONFIGURATION.md](AUSTRALIAN_CONFIGURATION.md) for details.

## Features

### 📊 Monitoring
- **Temperature Sensing** - DS18B20 digital temperature sensor with moving average filtering
- **pH Monitoring** - Analog pH sensor with 3-point calibration system
- **TDS Measurement** - Total Dissolved Solids sensor with temperature compensation
- **Real-time Web Dashboard** - Live sensor readings and system status

### 🎛️ Control Systems
- **Adaptive PID Controllers** - Self-learning PID with auto-tuning capabilities
- **Temperature Control** - Automated heater control with safety limits
- **CO2 Control** - pH-based CO2 injection control via solenoid valve
- **Safety Features** - Emergency stop mechanisms and overshoot prevention
- **Persistent Storage** - All PID parameters and calibration data stored in NVS (survives reboots)

### 🌐 Connectivity
- **WiFi Manager** - Easy WiFi setup with Access Point fallback
- **Web Interface** - Responsive HTML5 dashboard with WebSocket real-time updates
- **MQTT Integration** - Publish sensor data for external monitoring (Home Assistant, Node-RED, etc.)
- **OTA Updates** - Remote firmware updates via web interface or Arduino OTA

### ⚙️ Advanced Features
- **FreeRTOS Tasks** - Multi-core processing for optimal performance
- **pH Calibration Interface** - Step-by-step calibration with multiple buffer solutions
- **Remote Configuration** - Change WiFi, MQTT, and control settings via web interface
- **Safety Interlocks** - Automatic shutdown if parameters exceed safe limits

## Hardware Requirements

### Components
- **ESP32 Development Board** (DevKit v1 or similar)
- **DS18B20 Temperature Sensor** with 4.7kΩ pull-up resistor
- **pH Sensor Module** (analog output)
- **TDS Sensor Module** (analog output)
- **2x Relay Modules** (for heater and CO2 solenoid)
- **Heater** (appropriate for your aquarium size)
- **CO2 Solenoid Valve**
- **Power Supply** (appropriate for your relays and ESP32)

### Pin Configuration

Default pin assignments (configurable in code):

```
Temperature Sensor (DS18B20): GPIO 4
pH Sensor (Analog):          GPIO 34
TDS Sensor (Analog):         GPIO 35
Heater Relay:                GPIO 26
CO2 Solenoid Relay:          GPIO 27
```

### Wiring Diagram

```
ESP32 Connections:
┌─────────────────────────────────────────────┐
│                                             │
│  GPIO 4  ──── Data ──── DS18B20            │
│                         (+ 4.7kΩ pullup)   │
│                                             │
│  GPIO 34 ──── Analog ─── pH Sensor         │
│  GPIO 35 ──── Analog ─── TDS Sensor        │
│                                             │
│  GPIO 26 ──── IN ──────── Heater Relay     │
│  GPIO 27 ──── IN ──────── CO2 Relay        │
│                                             │
│  GND ─────────────────── Common Ground     │
│  3.3V ────────────────── Sensors VCC       │
│  5V ──────────────────── Relay Modules     │
└─────────────────────────────────────────────┘

⚠️ AUSTRALIAN ELECTRICAL SAFETY WARNING:
   - 240V AC mains - USE RCD (30mA) PROTECTION MANDATORY
   - Ensure proper isolation for AC-powered equipment
   - Use optocoupled relays for 240V switching
   - Keep low-voltage components >30cm from water
   - IP65+ rated enclosure required near water
   - Comply with AS/NZS 3000:2018 wiring standards
   
   See AUSTRALIAN_CONFIGURATION.md for complete safety guide.
```

## Software Setup

### Prerequisites
- [PlatformIO](https://platformio.org/) installed (or Arduino IDE)
- USB driver for ESP32 (CP2102 or CH340 depending on your board)

### Installation

1. **Clone or download this repository**
   ```bash
   git clone https://github.com/yourusername/aquariumcontroller.git
   cd aquariumcontroller
   ```

2. **Open in PlatformIO**
   ```bash
   pio run
   ```

3. **Upload to ESP32**
   ```bash
   pio run --target upload
   ```

4. **Upload filesystem (for web interface)**
   ```bash
   pio run --target uploadfs
   ```

5. **Monitor serial output**
   ```bash
   pio device monitor -b 115200
   ```

6. **Run tests (optional but recommended)**
   ```bash
   pio test -e native
   ```
   See [TESTING.md](TESTING.md) for detailed testing information.

## First Time Setup

### 1. WiFi Configuration

On first boot, the device will create an Access Point:
- **SSID:** `AquariumController`
- **Password:** `12345678`

Connect to this network and navigate to `http://192.168.4.1` to configure your WiFi credentials.

### 2. pH Sensor Calibration

**Important:** Calibrate the pH sensor before first use!

1. Navigate to the **pH Calibration** tab in the web interface
2. Prepare pH buffer solutions (pH 4.0, 7.0, and 10.0)
3. Click "Start Calibration"
4. For each buffer solution:
   - Immerse the pH probe
   - Wait 30-60 seconds for reading to stabilize
   - Click the corresponding calibration button
   - Rinse probe with distilled water between solutions
5. Click "Save Calibration"

### 3. Set Target Parameters

In the **Control** tab:
- Set your desired temperature (typically 24-26°C for tropical fish)
- Set your desired pH (typically 6.5-7.0 for planted tanks with CO2)

### 4. Configure MQTT (Optional)

In the **Settings** tab:
- Enter your MQTT broker address
- Configure port (default: 1883)
- Enter credentials if required
- Save and restart

## Web Interface

Access the web interface at `http://<device-ip>` where `<device-ip>` is shown in the serial monitor.

### Dashboard Sections

#### 📊 Live Sensor Readings
- Real-time display of temperature, pH, and TDS
- Updates every second via WebSocket

#### 🔌 Device Status
- Shows current state of heater and CO2 solenoid
- Visual indicators (green = ON, red = OFF)

#### ⚙️ Control Tab
- Adjust temperature and pH targets
- Emergency stop button (immediately disables all outputs)
- Clear emergency stop button

#### 🔬 pH Calibration Tab
- Step-by-step calibration wizard
- Three-point calibration for accuracy
- Save/reset calibration data

#### 🔧 Settings Tab
- WiFi configuration
- MQTT broker settings
- Device restart option

## MQTT Topics

The controller publishes to the following topics (prefix configurable):

```
aquarium/temperature     - Current temperature in °C
aquarium/ph             - Current pH level
aquarium/tds            - TDS in ppm
aquarium/heater         - Heater state (ON/OFF)
aquarium/co2            - CO2 solenoid state (ON/OFF)
aquarium/alert          - Emergency alerts
aquarium/status         - Device online status
```

## PID Control

### How It Works

The system uses two independent adaptive PID controllers:

1. **Temperature Controller**
   - Maintains water temperature at target
   - Controls heater relay
   - Safety shutdown if temp exceeds safety limit

2. **CO2 Controller**
   - Maintains pH at target by controlling CO2 injection
   - Controls CO2 solenoid valve
   - Safety shutdown if pH drops too low

### Self-Learning Features

- **Auto-tuning:** Can automatically calculate optimal PID parameters
- **Adaptive Adjustment:** Gradually refines parameters based on performance
- **Persistent Storage:** Learned parameters saved to NVS flash memory
- **Minimal Overshoot:** Designed to prevent dangerous parameter spikes

### Safety Mechanisms

1. **Absolute Limits:** Hard-coded maximum values
2. **Safety Thresholds:** Configurable warning margins
3. **Emergency Stop:** Automatic shutdown on dangerous conditions
4. **Manual Override:** Web interface emergency stop button
5. **Minimum Toggle Interval:** Prevents rapid relay switching

## OTA Updates

### Web-Based Update
1. Navigate to `http://<device-ip>/update`
2. Click "Choose File" and select `.bin` firmware file
3. Click "Update" and wait for completion
4. Device will restart automatically

### Arduino OTA
The device is discoverable as `aquarium-controller` on your network. You can upload directly from PlatformIO/Arduino IDE.

**Default OTA Password:** `aquarium123` (change in `OTAManager.cpp`)

## Troubleshooting

### No WiFi Connection
- Check credentials in Settings tab
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Try power cycling the device
- Connect to AP mode to reconfigure

### Sensors Not Reading
- Check wiring connections
- Verify power supply (3.3V for sensors)
- Check serial monitor for error messages
- Test sensors individually

### pH Readings Inaccurate
- Perform calibration procedure
- Ensure probe is clean and hydrated
- Check probe BNC connection
- Replace probe if old (typical lifespan: 1-2 years)

### Heater/CO2 Not Activating
- Check relay wiring and power
- Verify relay module type (active high/low)
- Check emergency stop status in web interface
- Review serial monitor for safety stops

### MQTT Not Connecting
- Verify broker address and port
- Check network connectivity
- Confirm credentials if authentication enabled
- Check broker logs

## Testing

This project includes a comprehensive test suite with 50+ tests covering:
- Unit tests for individual components
- Integration tests for complete workflows
- Mock tests for hardware-dependent features
- Safety and edge case validation

### Running Tests

```bash
# Run all tests on your computer (fast)
pio test -e native

# Run tests on ESP32 hardware
pio test -e esp32dev

# Run specific test file
pio test -e native -f test_integration
```

For detailed testing information, see [TESTING.md](TESTING.md).

### Test Coverage
- ✅ PID controller logic (30+ tests)
- ✅ Sensor validation and filtering
- ✅ Safety systems and emergency stop
- ✅ Configuration storage and retrieval
- ✅ pH calibration workflow
- ✅ Data flow and integration

## Safety Warnings

⚠️ **IMPORTANT SAFETY INFORMATION** ⚠️

1. **Electrical Safety**
   - NEVER work on wiring with power connected
   - Use proper electrical isolation for AC-powered devices
   - Consider using GFCI/RCD protection
   - Keep all electronics away from water

2. **Aquarium Safety**
   - Always test new equipment before adding livestock
   - Monitor closely for first 48 hours
   - Set conservative safety limits
   - Have backup heating/aeration
   - Never rely solely on automation - check manually

3. **CO2 Safety**
   - High CO2 levels can suffocate fish
   - Always set pH safety minimum above 6.0
   - Monitor fish behavior closely
   - Ensure adequate surface agitation
   - Consider solenoid timer for nighttime shutoff

4. **Temperature Safety**
   - Set temperature safety max appropriately for your species
   - Typical maximum: 28-30°C for tropical fish
   - Use appropriately sized heater
   - Consider backup temperature sensor

## Advanced Configuration

### Custom Pin Assignments

Edit `ConfigManager.h` default constructor to change pin assignments:

```cpp
tempSensorPin = 4;
phSensorPin = 34;
tdsSensorPin = 35;
heaterRelayPin = 26;
co2RelayPin = 27;
```

### PID Tuning

Initial PID values can be adjusted in `main.cpp`:

```cpp
tempPID = new AdaptivePID("temp-pid", Kp, Ki, Kd);
co2PID = new AdaptivePID("co2-pid", Kp, Ki, Kd);
```

For manual tuning:
- Increase **Kp** for faster response (but may overshoot)
- Increase **Ki** to eliminate steady-state error
- Increase **Kd** to reduce overshoot and oscillation

### Task Priorities

FreeRTOS task priorities can be adjusted in `SystemTasks.cpp`:
- Higher priority = more CPU time
- Sensor and Control tasks run on separate cores

## Development

### Project Structure

```
aquariumcontroller/
├── include/               # Header files
│   ├── AdaptivePID.h
│   ├── ConfigManager.h
│   ├── OTAManager.h
│   ├── PHSensor.h
│   ├── RelayController.h
│   ├── SystemTasks.h
│   ├── TDSSensor.h
│   ├── TemperatureSensor.h
│   ├── WebServer.h
│   └── WiFiManager.h
├── src/                   # Source files
│   ├── AdaptivePID.cpp
│   ├── ConfigManager.cpp
│   ├── OTAManager.cpp
│   ├── PHSensor.cpp
│   ├── RelayController.cpp
│   ├── SystemTasks.cpp
│   ├── TDSSensor.cpp
│   ├── TemperatureSensor.cpp
│   ├── WebServer.cpp
│   ├── WiFiManager.cpp
│   └── main.cpp
├── data/                  # Web interface files
│   └── index.html
├── platformio.ini         # PlatformIO configuration
├── partitions.csv         # ESP32 partition table
└── README.md             # This file
```

### Adding Features

To add new sensors or features:
1. Create header/source files in `include/` and `src/`
2. Add to task loop or create new task in `SystemTasks.cpp`
3. Update web interface in `data/index.html`
4. Add API endpoints in `WebServer.cpp`

## License

This project is provided as-is for educational and personal use. See LICENSE file for details.

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## Credits

Built with:
- ESP32 Arduino Core
- PlatformIO
- AsyncWebServer
- ArduinoJson
- PubSubClient (MQTT)
- DallasTemperature (DS18B20)

## Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check existing issues for solutions
- Consult the serial monitor for debug output

## Disclaimer

This project involves controlling aquarium life support systems. While designed with safety features, the author assumes no responsibility for equipment failure, data loss, or harm to aquatic life. Always monitor your aquarium and have backup systems in place.

---

**Happy Fishkeeping! 🐠🌱**
