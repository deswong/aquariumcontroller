# Display Configuration

The aquarium controller uses an **SSD1309 OLED display** for real-time monitoring and status visualization.

## SSD1309 OLED 128x64 📊 Monitoring Display

### Overview
High-contrast OLED display with automatic screen cycling and trend visualization.

### Specifications
- **Display:** 128x64 OLED with SSD1309 controller
- **Interface:** I2C (Hardware I2C)
- **GPIO Pins:** 2 (21-SDA, 22-SCL)
- **Code Size:** ~400 lines
- **Flash Usage:** ~200 KB
- **Update Rate:** 1 Hz (1000ms)
- **Cost:** $8-15
- **Power:** 3.3V, ~20mA typical

### Features
- ✅ 3 auto-cycling screens (5-second intervals)
- ✅ High contrast OLED display (160° viewing angle)
- ✅ Real-time trend graphs for all sensors
- ✅ Animated status indicators
- ✅ Network connectivity status
- ✅ System information display
- ✅ Low power consumption
- ✅ Compact 2-pin interface

### Screen Layout
1. **Main Status** - Real-time values with progress bars and icons
2. **Trend Graphs** - Historical data visualization for temperature, pH, TDS
3. **System Info** - Network status, uptime, memory usage

### Benefits
- ✅ Minimal GPIO usage (only 2 pins)
- ✅ High contrast and readability
- ✅ No user input complexity
- ✅ Automatic operation
- ✅ Small flash footprint
- ✅ Low power consumption

### Implementation
**Files:** `DisplayManager.h`, `DisplayManager.cpp`

**Include:**
```cpp
#include "DisplayManager.h"
```

### Implementation

**Files:** `OLEDDisplayManager.h`, `OLEDDisplayManager.cpp`

**Include:**
```cpp
#include "OLEDDisplayManager.h"
```

**Usage:**
```cpp
OLEDDisplayManager* displayMgr = new OLEDDisplayManager();
if (displayMgr->begin()) {
    displayMgr->updateTemperature(25.5, 26.0);
    displayMgr->updatePH(7.2, 7.0);
    displayMgr->update();  // Call in main loop
}
```

**Wiring:**
```
ESP32     SSD1309
------    -------
3.3V  →   VCC
GND   →   GND
21    →   SDA
22    →   SCL
```

### Screen Layouts

**Screen 0: Main Status** (Auto-cycles every 5 seconds)
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

**Screen 1: Trend Graphs**
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

**Screen 2: System Information**
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

**Documentation:**
- See archived documentation (Ender 3 display support removed)

---

## Option 2: SSD1309 OLED 128x64 📊 Monitoring

### Overview
Minimal monitoring display with automatic information updates. All control via web interface.

### Specifications
- **Display:** 128x64 OLED with SSD1309 controller
- **Interface:** I2C
- **User Input:** None (monitoring only)
- **GPIO Pins:** 2 (21 SDA, 22 SCL)
- **Code Size:** 287 lines
- **Flash Usage:** ~80 KB
- **Update Rate:** 1 Hz (1000ms)
- **Cost:** $5-12
- **Power:** 3.3V or 5V, ~20mA

### Features
- ✅ Single information screen
- ✅ Real-time sensor display (temp, pH, TDS)
- ✅ System status (heater, CO2, WiFi)
- ✅ Room temperature
- ✅ WiFi status and IP address
- ✅ Last water change date
- ✅ Current time from NTP
- ✅ High contrast OLED (160° viewing angle)
- ✅ Auto-updates every second

### Display Layout
```
┌──────────────────────────────────────────┐
│ Last WC: 2025-10-15          12:34:56    │
│ Temp: 25.5C [HEAT]                       │
│ pH: 7.20 [CO2]                           │
│ TDS: 245 ppm                             │
│ Room: 23.0C                              │
│ WiFi: 192.168.1.100                      │
└──────────────────────────────────────────┘
```

### When to Choose This
- ✅ Primarily use web interface for control
- ✅ Want simple monitoring display
- ✅ Need to save GPIO pins (7 pins freed)
- ✅ Want to save flash space (~400 KB freed)
- ✅ Prefer lower cost option ($10-15 cheaper)
- ✅ Want simpler wiring (just 2 wires)
- ✅ No need for on-device input

### Implementation
**Files:** `DisplayManager_OLED.h`, `DisplayManager_OLED.cpp`

**Include:**
```cpp
#include "DisplayManager_OLED.h"
```

**Wiring:**
```
SDA: GPIO 21
SCL: GPIO 22
Power: 3.3V or 5V, GND
```

**Documentation:**
- [OLED_DISPLAY_MANAGER.md](OLED_DISPLAY_MANAGER.md) - Complete implementation guide
- [SSD1309_IMPLEMENTATION_SUMMARY.md](SSD1309_IMPLEMENTATION_SUMMARY.md) - Technical details

## Alternative: No Display Setup

For maximum GPIO availability and minimal components, you can run without any display:

### Web Interface Only
- Comment out display initialization in `main.cpp`
- Saves all GPIO pins for other features  
- Saves flash space
- Use web interface exclusively for monitoring and control
- Access via: `http://[ESP32_IP_ADDRESS]`

### Benefits of No Display
- **Maximum GPIO availability** - 2 additional pins freed
- **Minimal components** - Reduces cost and complexity
- **Web-first approach** - Modern interface with full features
- **Remote monitoring** - Access from anywhere on network
- **Mobile friendly** - Responsive design works on phones/tablets

### Setup
```cpp
// In main.cpp, comment out display initialization:
// displayMgr = new OLEDDisplayManager();
// displayMgr->begin();
```

## Migration from Previous Display Configurations

If migrating from older code that used different display managers:

```cpp
// Replace any of these:
// #include "DisplayManager_OLED.h"      // Old OLED implementation  
// #include "UnifiedDisplayManager.h"    // Unified version

// With this:
#include "OLEDDisplayManager.h"

// Replace initialization:
// DisplayManager* displayMgr = new DisplayManager();
// UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_SSD1309_OLED);

// With:
OLEDDisplayManager* displayMgr = new OLEDDisplayManager();
```

All data update method calls remain the same!

## Testing

### Test OLED Display
```cpp
displayMgr->begin();
displayMgr->test();  // Shows "Aquarium Controller"
displayMgr->updateTemperature(25.5, 26.0);
displayMgr->updatePH(7.2, 7.0);
displayMgr->updateTDS(245);
displayMgr->update(); // Call in main loop
```

## Complete Setup Guide

See [OLED_DISPLAY_MANAGER.md](OLED_DISPLAY_MANAGER.md) for detailed setup instructions, API reference, and troubleshooting guide.
displayMgr->updateTemperature(25.5, 26.0);
displayMgr->updatePH(7.2, 7.0);
displayMgr->updateTDS(245);
displayMgr->updateNetworkStatus(true, "192.168.1.100");
```

## Conclusion

The SSD1309 OLED display is the recommended display option for the aquarium controller:

- **SSD1309 OLED**: Minimal monitoring display for web-centric control
- Simple 2-wire I2C interface
- Low GPIO pin count (only 2 pins)
- Small flash footprint (~80 KB)
- Reliable and inexpensive

---

**Implementation Guide:**
- OLED: [OLED_DISPLAY_GUIDE.md](OLED_DISPLAY_GUIDE.md)
