# OLED Display Manager

The `OLEDDisplayManager` is a streamlined display manager specifically designed for the **SSD1309 OLED** display. It provides automatic screen cycling, trend graphs, and animated status indicators for your aquarium controller.

## Features

- **128x64 OLED Display** - High contrast SSD1309 controller via I2C
- **Auto-Cycling Screens** - 3 screens that rotate every 5 seconds
- **Real-time Trends** - Historical graphs for temperature, pH, and TDS
- **Animated Status** - Wave animation and status indicators
- **Network Status** - WiFi connection and IP address display
- **System Information** - Uptime and memory usage

## Hardware Requirements

### SSD1309 OLED Display
- **Resolution:** 128x64 pixels
- **Interface:** I2C (Hardware I2C)
- **Pins:** SDA (GPIO 21), SCL (GPIO 22)
- **Voltage:** 3.3V
- **Current:** ~20mA typical

### Wiring
```
ESP32     SSD1309 OLED
------    ------------
3.3V  →   VCC
GND   →   GND
21    →   SDA
22    →   SCL
```

## Usage

### 1. Include Header
```cpp
#include "OLEDDisplayManager.h"
```

### 2. Create Instance
```cpp
OLEDDisplayManager* displayMgr = new OLEDDisplayManager();
```

### 3. Initialize
```cpp
void setup() {
    if (!displayMgr->begin()) {
        Serial.println("OLED initialization failed!");
        return;
    }
    
    // Optional: Adjust contrast (0-255)
    displayMgr->setContrast(128);
}
```

### 4. Update Loop
```cpp
void loop() {
    // Update display (handles screen cycling and animations)
    displayMgr->update();
    
    // Update sensor data
    displayMgr->updateTemperature(25.5, 26.0);  // current, target
    displayMgr->updatePH(7.2, 7.0);             // current, target
    displayMgr->updateTDS(450);
    displayMgr->updateAmbientTemperature(22.1);
    
    // Update control states
    displayMgr->updateHeaterState(true);
    displayMgr->updateCO2State(false);
    displayMgr->updateDosingState(false);
    
    // Update network info
    bool connected = (WiFi.status() == WL_CONNECTED);
    String ip = connected ? WiFi.localIP().toString() : "0.0.0.0";
    displayMgr->updateNetworkStatus(connected, ip.c_str());
    
    // Update time (if you have NTP)
    displayMgr->updateTime("12:34:56");
    
    // Update water change date
    displayMgr->updateWaterChangeDate("2025-10-15");
}
```

## Screen Layout

### Screen 0: Main Status
```
┌─────────────────────────────────────────┐
│ [WiFi] ●●○              12:34:56        │
├─────────────────────────────────────────┤
│ 🌡️ 25.5C    [🔥]  ████████▒▒▒▒ 26.0    │
│ 📊 7.20     [💨]  ██████▒▒▒▒▒▒ 7.0      │
│ 💧 450 ppm         [💉] DOSING          │
│ 📅 WC: 2025-10-15                       │
│ ～～～～～～～～～～～～～～～～～～～～～～  │
└─────────────────────────────────────────┘
```

### Screen 1: Trend Graphs
```
┌─────────────────────────────────────────┐
│ [WiFi] ○●○              12:34:56        │
├─────────────────────────────────────────┤
│ Temp    pH      TDS                     │
│ ┌─────┐ ┌─────┐ ┌─────┐                 │
│ │  /\ │ │ \_/ │ │ /‾\ │                 │
│ │ /  \│ │/   \│ │/   \│                 │
│ │/    │ │     │ │     │                 │
│ └─────┘ └─────┘ └─────┘                 │
│ 25.5    7.20    450                     │
│ H       C       D                       │
└─────────────────────────────────────────┘
```

### Screen 2: Network & System
```
┌─────────────────────────────────────────┐
│ [WiFi] ○○●              12:34:56        │
├─────────────────────────────────────────┤
│ Network:                                │
│ Connected                               │
│ 192.168.1.100                          │
│                                         │
│ System:                                 │
│ Uptime: 1440m          Heap: 156K      │
│                                         │
│                                         │
└─────────────────────────────────────────┘
```

## API Reference

### Core Methods
```cpp
bool begin();                    // Initialize display
void update();                   // Update display (call in loop)
void clear();                    // Clear display buffer
void setBrightness(uint8_t);     // Set brightness (0-255)
void setContrast(uint8_t);       // Set contrast (0-255)
void test();                     // Display test screen
```

### Data Update Methods
```cpp
void updateTemperature(float current, float target);
void updatePH(float current, float target);
void updateTDS(float tds);
void updateAmbientTemperature(float temp);
void updateHeaterState(bool active);
void updateCO2State(bool active);
void updateDosingState(bool active);
void updateWaterChangeDate(const char* date);
void updateNetworkStatus(bool connected, const char* ip);
void updateTime(const char* time);
```

### Screen Control
```cpp
void nextScreen();               // Manually advance to next screen
void setScreen(uint8_t screen);  // Jump to specific screen (0-2)
```

## Configuration

### Screen Timing
```cpp
// In OLEDDisplayManager.h, modify these constants:
static const unsigned long UPDATE_INTERVAL = 1000;       // Display refresh: 1Hz
static const unsigned long SCREEN_SWITCH_INTERVAL = 5000; // Auto-cycle: 5 seconds
static const unsigned long ANIMATION_INTERVAL = 200;     // Animation: 5 FPS
```

### Trend History
```cpp
// Adjust trend buffer size:
static const uint8_t TREND_SIZE = 32;  // Number of historical data points
```

### Display Settings
```cpp
// In begin() method:
display->setContrast(128);  // Adjust contrast (0-255)
```

## Migration from Existing Display Managers

### From DisplayManager_OLED
Replace:
```cpp
#include "DisplayManager_OLED.h"
DisplayManager* display = new DisplayManager();
```

With:
```cpp
#include "OLEDDisplayManager.h"
OLEDDisplayManager* display = new OLEDDisplayManager();
```

All method calls remain the same!

### From UnifiedDisplayManager
Replace:
```cpp
#include "UnifiedDisplayManager.h"
UnifiedDisplayManager* display = new UnifiedDisplayManager(DISPLAY_SSD1309_OLED);
```

With:
```cpp
#include "OLEDDisplayManager.h"
OLEDDisplayManager* display = new OLEDDisplayManager();
```

## Troubleshooting

### Display Not Working
1. Check I2C connections (SDA/SCL)
2. Verify power supply (3.3V)
3. Test I2C address: `i2cdetect -y 1` (should show 0x3C or 0x3D)
4. Check U8g2 library is installed

### Poor Display Quality
1. Adjust contrast: `displayMgr->setContrast(200);`
2. Check power supply stability
3. Verify I2C pullup resistors (usually internal)

### Memory Issues
1. Reduce `TREND_SIZE` if needed
2. Monitor heap usage in Screen 2
3. Consider reducing update frequency

## Performance

- **Update Rate:** 1 Hz (configurable)
- **Screen Cycle:** 5 seconds (configurable)
- **Animation Rate:** 5 FPS
- **Memory Usage:** ~2KB RAM for trends + display buffer
- **I2C Speed:** 100kHz (default) or 400kHz (fast mode)

## Integration Example

```cpp
// main.cpp integration
#include "OLEDDisplayManager.h"

OLEDDisplayManager* displayMgr = nullptr;

void setup() {
    // Initialize display
    displayMgr = new OLEDDisplayManager();
    if (!displayMgr->begin()) {
        Serial.println("Display initialization failed!");
        return;
    }
}

void updateDisplayData() {
    if (!displayMgr) return;
    
    // Get sensor readings
    float temp = tempSensor ? tempSensor->getTemperature() : 0.0;
    float targetTemp = tempController ? tempController->getSetpoint() : 26.0;
    float ph = phSensor ? phSensor->getPH() : 7.0;
    float targetPH = phController ? phController->getSetpoint() : 7.0;
    float tds = tdsSensor ? tdsSensor->getTDS() : 0.0;
    
    // Update display
    displayMgr->updateTemperature(temp, targetTemp);
    displayMgr->updatePH(ph, targetPH);
    displayMgr->updateTDS(tds);
    
    // Update states
    displayMgr->updateHeaterState(relayController->getHeaterState());
    displayMgr->updateCO2State(relayController->getCO2State());
    displayMgr->updateDosingState(dosingPump->isActive());
    
    // Update network
    displayMgr->updateNetworkStatus(WiFi.isConnected(), WiFi.localIP().toString().c_str());
    displayMgr->updateTime(getCurrentTimeString().c_str());
}

void loop() {
    updateDisplayData();
    displayMgr->update();  // Must be called regularly
    
    // ... rest of main loop
}
```

This OLED display manager provides a clean, efficient interface for your aquarium controller's SSD1309 OLED display with all the features you need and none of the complexity of supporting multiple display types.