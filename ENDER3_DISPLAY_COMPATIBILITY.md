# ESP32 Pin Usage Analysis & Ender 3 Pro Display Compatibility

## Current Pin Usage

### Occupied Pins

#### Sensors (Inputs)
- **GPIO 4**: Water Temperature Sensor (DS18B20) - OneWire
- **GPIO 5**: Ambient Temperature Sensor (DS18B20) - OneWire  
- **GPIO 34**: pH Sensor (Analog ADC) - Input only
- **GPIO 35**: TDS Sensor (Analog ADC) - Input only

#### Control Outputs
- **GPIO 26**: Heater Relay (was used for dosing pump IN2, conflict!)
- **GPIO 27**: CO2 Solenoid Relay

#### Dosing Pump (DRV8871 Motor Driver)
- **GPIO 25**: DRV8871 IN1 (PWM capable)
- **GPIO 26**: DRV8871 IN2 (PWM capable) - **CONFLICT with heater relay!**

⚠️ **CRITICAL ISSUE FOUND**: GPIO 26 is assigned to both heater relay AND dosing pump IN2!

---

## ESP32 DevKit Pin Availability

### Total GPIO Pins: 38
### Strapping/Boot Pins (avoid if possible):
- **GPIO 0**: Must be HIGH during boot (has pullup)
- **GPIO 2**: Boot mode selection (has pulldown)  
- **GPIO 5**: Strapping pin (already used for ambient temp - OK)
- **GPIO 12**: Boot voltage selection (low during boot)
- **GPIO 15**: Boot mode (has pullup)

### Input-Only Pins (ADC2, no PWM):
- **GPIO 34-39**: ADC only, no pullup/pulldown (34, 35 already used)

### Reserved/Unavailable:
- **GPIO 6-11**: Connected to flash (DO NOT USE)
- **GPIO 1 (TX0)**: Serial TX (used for debugging)
- **GPIO 3 (RX0)**: Serial RX (used for debugging)

### Available GPIO Pins

#### Recommended for General Use:
- **GPIO 13, 14**: Good digital I/O, no restrictions
- **GPIO 16, 17**: Good digital I/O (UART2)
- **GPIO 18, 19**: Good digital I/O (SPI pins but can be repurposed)
- **GPIO 21, 22**: Good digital I/O (I2C pins but can be repurposed)
- **GPIO 23**: Good digital I/O (SPI MOSI but can be repurposed)
- **GPIO 32, 33**: Good digital I/O, ADC1 capable, touch capable

#### Usable with Caution:
- **GPIO 0**: Button input (needs pullup, pulled LOW for boot mode)
- **GPIO 2**: LED output (has pulldown, must be LOW during flash)
- **GPIO 12**: Must be LOW during boot
- **GPIO 15**: Must be HIGH during boot

---

## Ender 3 Pro Display Requirements

### Standard Ender 3 Pro Display (RepRapDiscount Full Graphic Smart Controller)

This is a **128x64 LCD with rotary encoder and SD card slot**.

#### Interface: SPI + Digital I/O
**Required Pins: 10 total**

1. **LCD_CS** (Chip Select) - Digital Output
2. **LCD_A0** (Data/Command) - Digital Output
3. **LCD_RESET** - Digital Output
4. **LCD_SCK** (SPI Clock) - SPI Clock
5. **LCD_MOSI** (SPI Data) - SPI Data
6. **BTN_ENC** (Encoder Button) - Digital Input (pullup)
7. **BTN_EN1** (Encoder A) - Digital Input (pullup)
8. **BTN_EN2** (Encoder B) - Digital Input (pullup)
9. **BEEPER** - Digital Output (PWM for tone)
10. **SD_DETECT** (optional) - Digital Input (pullup)

#### Additional Features:
- **SD Card**: Uses same SPI bus as LCD (shares SCK, MOSI, MISO)
  - **SD_CS** - Digital Output (separate chip select)
  - **SD_MISO** - SPI MISO

---

## Pin Assignment Analysis

### Current Usage Summary
| Function | GPIO | Type | Notes |
|----------|------|------|-------|
| Water Temp | 4 | OneWire | DS18B20 |
| Ambient Temp | 5 | OneWire | DS18B20 |
| Heater Relay | 26 | Output | **CONFLICT!** |
| CO2 Relay | 27 | Output | |
| Dosing IN1 | 25 | PWM Out | DRV8871 |
| Dosing IN2 | 26 | PWM Out | **CONFLICT!** |
| pH Sensor | 34 | ADC Input | Analog |
| TDS Sensor | 35 | ADC Input | Analog |

**Total Used: 7 pins (but 1 conflict = 8 physical pins needed)**

### Pins Available After Fixing Conflict
**Fix dosing pump conflict first:**
- Move Dosing IN2 from GPIO 26 to GPIO 33 (PWM capable)
- Keep Heater Relay on GPIO 26

**After fix, occupied pins: 8**
**Remaining GPIO: 30 pins total - 8 used = 22 available**

### Display Pin Requirements: 10-12 pins

**Plenty of pins available!** ✅

---

## Recommended Pin Mapping for Ender 3 Display

### Option 1: Hardware SPI (Faster, recommended)

Using ESP32 hardware SPI (VSPI):
- **VSPI_MOSI** = GPIO 23
- **VSPI_MISO** = GPIO 19
- **VSPI_SCK** = GPIO 18
- **VSPI_CS** = GPIO 5 (but already used!)

**Alternative**: Use HSPI or software SPI

### Option 2: Software SPI (Flexible, any pins)

Recommended pin assignment avoiding conflicts:

| Display Pin | ESP32 GPIO | Function | Notes |
|------------|-----------|----------|-------|
| **LCD_CS** | GPIO 15 | Chip Select | Pull HIGH at boot |
| **LCD_A0** | GPIO 2 | Data/Command | OK for output |
| **LCD_RESET** | GPIO 0 | Reset | Has pullup |
| **LCD_SCK** | GPIO 18 | SPI Clock | Can be SW SPI |
| **LCD_MOSI** | GPIO 23 | SPI Data Out | |
| **BTN_ENC** | GPIO 13 | Encoder Button | Needs pullup |
| **BTN_EN1** | GPIO 14 | Encoder A | Needs pullup |
| **BTN_EN2** | GPIO 16 | Encoder B | Needs pullup |
| **BEEPER** | GPIO 17 | Buzzer | PWM for tones |
| **SD_CS** | GPIO 21 | SD Chip Select | Optional |
| **SD_MISO** | GPIO 19 | SD Data In | Optional |
| **SD_DETECT** | GPIO 22 | SD Card Detect | Optional |

**Total: 12 pins for full functionality (including SD card)**

---

## Final Pin Assignment (Complete System)

### Sensors & Control (8 pins)
| Function | GPIO | Type |
|----------|------|------|
| Water Temp | 4 | OneWire |
| Ambient Temp | 5 | OneWire |
| pH Sensor | 34 | ADC Input |
| TDS Sensor | 35 | ADC Input |
| Heater Relay | 26 | Digital Out |
| CO2 Relay | 27 | Digital Out |
| Dosing IN1 | 25 | PWM Out |
| Dosing IN2 | **33** | PWM Out (FIXED!) |

### Display (12 pins)
| Function | GPIO | Type |
|----------|------|------|
| LCD_CS | 15 | Digital Out |
| LCD_A0 | 2 | Digital Out |
| LCD_RESET | 0 | Digital Out |
| LCD_SCK | 18 | SPI Clock |
| LCD_MOSI | 23 | SPI Data |
| BTN_ENC | 13 | Digital In |
| BTN_EN1 | 14 | Digital In |
| BTN_EN2 | 16 | Digital In |
| BEEPER | 17 | PWM Out |
| SD_CS | 21 | Digital Out |
| SD_MISO | 19 | SPI Data In |
| SD_DETECT | 22 | Digital In |

**Total Pins Used: 20 of 38**  
**Remaining: 18 GPIO pins available for expansion**

---

## Answer: YES, Plenty of Pins! ✅

### Summary
- **Current system uses**: 8 pins (after fixing conflict)
- **Ender 3 display needs**: 10-12 pins (with SD card)
- **Total required**: 20 pins
- **ESP32 DevKit has**: 38 GPIO pins
- **Remaining for expansion**: 18 pins

### Critical Fix Needed First

**Update ConfigManager.h default:**
```cpp
heaterRelayPin = 26;   // Keep
co2RelayPin = 27;       // Keep
```

**Update main.cpp:**
```cpp
// Change from:
dosingPump = new DosingPump(25, 26, 1);

// To:
dosingPump = new DosingPump(25, 33, 1);  // IN1=GPIO25, IN2=GPIO33
```

---

## Implementation Steps

### 1. Fix Pin Conflict
- Update `src/main.cpp` line 261: Change GPIO 26 to GPIO 33 for dosing pump IN2
- Test dosing pump operation

### 2. Add Display Support

#### Install U8g2 Library
```ini
# platformio.ini
lib_deps =
    ...existing libraries...
    olikraus/U8g2 @ ^2.35.9
```

#### Create Display Manager Class
```cpp
// include/DisplayManager.h
#include <U8g2lib.h>

#define LCD_CS   15
#define LCD_A0   2
#define LCD_RESET 0
#define LCD_SCK  18
#define LCD_MOSI 23
#define BTN_ENC  13
#define BTN_EN1  14
#define BTN_EN2  16
#define BEEPER   17

class DisplayManager {
private:
    U8G2_ST7920_128X64_F_SW_SPI* display;
    int encoderPos;
    bool buttonPressed;
    
public:
    void begin();
    void update();
    void showSensorData(float temp, float ph, float tds);
    void showMenu();
    int getEncoderPosition();
    bool isButtonPressed();
};
```

### 3. Wire Connections

**From Ender 3 Display EXP1/EXP2 Connectors:**

```
EXP1:
1  - BEEPER    → GPIO 17
2  - BTN_ENC   → GPIO 13
3  - LCD_EN    → LCD_CS (GPIO 15)
4  - LCD_RS    → LCD_A0 (GPIO 2)
5  - LCD_D4    → LCD_MOSI (GPIO 23)
6  - RESET     → LCD_RESET (GPIO 0)
7  - BTN_EN1   → GPIO 14
8  - BTN_EN2   → GPIO 16
9  - SCK       → GPIO 18
10 - MOSI      → (shared with LCD_D4)

EXP2:
1  - MISO      → GPIO 19 (SD card)
2  - SCK       → GPIO 18 (SD card)
3  - BTN_ENC   → GPIO 13 (duplicate)
4  - SD_CS     → GPIO 21
5  - MOSI      → GPIO 23 (SD card)
6  - BTN_EN1   → GPIO 14 (duplicate)
7  - SD_DETECT → GPIO 22
8  - RESET     → GPIO 0 (duplicate)
9  - LCD_D5    → (not used for ST7920)
10 - LCD_EN    → GPIO 15 (duplicate)
```

---

## Advantages of Using Ender 3 Display

### 1. **Local Control**
- No WiFi needed for basic operations
- Adjust targets, view sensors
- Manual water changes
- Dosing pump control

### 2. **Visual Feedback**
- Real-time sensor readings
- System status at a glance
- Graphs/trends on screen
- Alert notifications

### 3. **Menu System**
- Settings adjustment
- Calibration workflows
- History viewing
- Emergency controls

### 4. **Existing Hardware**
- Repurpose unused Ender 3 display
- Cost-effective
- Well-documented pinout
- Proven reliable design

---

## Display UI Concept

### Main Screen
```
┌────────────────────────┐
│ Aquarium Controller    │
│ ──────────────────────│
│ Temp:  25.2°C  [25.0°C]│
│ pH:     6.82    [6.80] │
│ TDS:   345 ppm         │
│ ──────────────────────│
│ Heater: ON   CO2: OFF  │
│ Next WC: 6.5 days      │
└────────────────────────┘
```

### Menu Structure
```
Main Menu
├── Sensor Data
│   ├── Temperature
│   ├── pH
│   ├── TDS
│   └── History Graph
├── Control
│   ├── Set Temp Target
│   ├── Set pH Target
│   ├── Manual Override
│   └── Emergency Stop
├── Water Change
│   ├── Start Change
│   ├── View Prediction
│   └── History
├── Dosing Pump
│   ├── Manual Dose
│   ├── Calibrate
│   └── Schedule
├── Settings
│   ├── WiFi Setup
│   ├── MQTT Setup
│   ├── Time/Date
│   └── Pin Config
└── Info
    ├── System Status
    ├── Uptime
    └── About
```

---

## Conclusion

✅ **YES - Completely Feasible!**

The ESP32 has **more than enough pins** to support:
- All current sensors and controls (8 pins)
- Full Ender 3 Pro display with SD card (12 pins)
- **18 additional GPIO pins** still available for future expansion

### Next Steps:
1. Fix GPIO 26 conflict (move dosing pump to GPIO 33)
2. Install U8g2 library
3. Create DisplayManager class
4. Wire up Ender 3 display
5. Implement UI screens
6. Test and enjoy local control!

This would make an excellent upgrade - combining web/MQTT control with local display feedback!
