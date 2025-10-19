# ESP32 Aquarium Controller - Pin Reference

## Default Pin Configuration

**Total Pins Used:**
- **Without Display:** 8 GPIO pins (30 free)
- **With Ender 3 Display:** 17 GPIO pins (21 free)
- **With SSD1309 OLED:** 10 GPIO pins (28 free)

### Core Sensors & Outputs (8 pins)
| Function | GPIO Pin | Type | Notes |
|----------|----------|------|-------|
| Water Temperature Sensor (DS18B20) | GPIO 4 | Digital (1-Wire) | Requires 4.7kΩ pullup resistor to 3.3V |
| Ambient Temperature Sensor (DS18B20) | GPIO 5 | Digital (1-Wire) | Requires 4.7kΩ pullup resistor to 3.3V |
| pH Sensor | GPIO 34 | Analog Input | Input only, no pullup available |
| TDS Sensor | GPIO 35 | Analog Input | Input only, no pullup available |
| Heater Relay | GPIO 26 | Digital Output | Can drive relay directly or via transistor |
| CO2 Solenoid Relay | GPIO 27 | Digital Output | Can drive relay directly or via transistor |
| Dosing Pump IN1 | GPIO 25 | Digital Output | L298N motor driver input |
| Dosing Pump IN2 | GPIO 33 | Digital Output | L298N motor driver input |

### Display Options

**Option 1: Ender 3 Pro LCD12864 (9 pins) - Interactive Menu**
| Function | GPIO Pin | Type | Notes |
|----------|----------|------|-------|
| LCD CS (Chip Select) | GPIO 15 | Digital Output | ST7920 LCD12864 display |
| LCD A0 (Register Select) | GPIO 2 | Digital Output | ST7920 LCD12864 display |
| LCD Reset | GPIO 0 | Digital Output | ST7920 LCD12864 display |
| LCD E (Enable) | GPIO 16 | Digital Output | ST7920 LCD12864 display |
| LCD R/W | GPIO 17 | Digital Output | ST7920 LCD12864 display |
| LCD PSB | GPIO 18 | Digital Output | Parallel/Serial mode select |
| Encoder DT (Data) | GPIO 13 | Digital Input | Rotary encoder with pullup |
| Encoder CLK (Clock) | GPIO 14 | Digital Input | Rotary encoder with pullup |
| Encoder Switch | GPIO 23 | Digital Input | Button press with pullup |

**Option 2: SSD1309 OLED (2 pins) - Monitoring Only**
| Function | GPIO Pin | Type | Notes |
|----------|----------|------|-------|
| I2C SDA | GPIO 21 | I2C Data | Hardware I2C, internal pullup |
| I2C SCL | GPIO 22 | I2C Clock | Hardware I2C, internal pullup |

*Trade-off: OLED saves 7 GPIO pins and ~400 KB flash but has no user input (web interface only)*

## ESP32 DevKit v1 Pin Layout

```
                        ┌─────────┐
                     EN │1      38│ GND
                  GPIO36│2      37│ GPIO23 ◄── Encoder Switch
                  GPIO39│3      36│ GPIO22
                  GPIO34│4      35│ GPIO1 (TX)
                  GPIO35│5      34│ GPIO3 (RX)
                  GPIO32│6      33│ GPIO21
                  GPIO33│7      32│ GND
                  GPIO25│8      31│ GPIO19
                  GPIO26│9      30│ GPIO18 ◄── LCD PSB
                  GPIO27│10     29│ GPIO5  ◄── Ambient Temp
                  GPIO14│11     28│ GPIO17 ◄── LCD R/W
                  GPIO12│12     27│ GPIO16 ◄── LCD E
                     GND│13     26│ GPIO4  ◄── Water Temp
                  GPIO13│14     25│ GPIO0  ◄── LCD Reset
                   GPIO9│15     24│ GPIO2  ◄── LCD A0
                  GPIO10│16     23│ GPIO15 ◄── LCD CS
                  GPIO11│17     22│ GPIO8
                     VIN│18     21│ GPIO7
                     GND│19     20│ GPIO6
                        └─────────┘

   Core Sensors & Outputs:
   pH Sensor ──────────────────►│4│ GPIO34
   TDS Sensor ─────────────────►│5│ GPIO35
   Water Temp Sensor ──────────►│26│ GPIO4
   Ambient Temp Sensor ─────────►│29│ GPIO5
   Heater Relay ───────────────►│9│ GPIO26
   CO2 Relay ──────────────────►│10│ GPIO27
   Dosing Pump IN1 ────────────►│8│ GPIO25
   Dosing Pump IN2 ────────────►│7│ GPIO33

   Display Interface:
   LCD CS ─────────────────────►│23│ GPIO15
   LCD A0 ─────────────────────►│24│ GPIO2
   LCD Reset ──────────────────►│25│ GPIO0
   LCD E ──────────────────────►│27│ GPIO16
   LCD R/W ────────────────────►│28│ GPIO17
   LCD PSB ────────────────────►│30│ GPIO18
   Encoder DT ─────────────────►│14│ GPIO13
   Encoder CLK ────────────────►│11│ GPIO14
   Encoder Switch ─────────────►│37│ GPIO23
```

## Detailed Wiring

### Water Temperature Sensor (DS18B20)
```
DS18B20          ESP32
┌──────┐
│  1   │ GND ──── GND
│  2   │ DATA ─── GPIO 4 (with 4.7kΩ to 3.3V)
│  3   │ VCC ──── 3.3V
└──────┘

Note: Place this sensor in the aquarium water
```

### Ambient Temperature Sensor (DS18B20)
```
DS18B20          ESP32
┌──────┐
│  1   │ GND ──── GND
│  2   │ DATA ─── GPIO 5 (with 4.7kΩ to 3.3V)
│  3   │ VCC ──── 3.3V
└──────┘

Note: Place this sensor in the air near the aquarium for ambient temperature
      Used during pH calibration to measure calibration solution temperature
```

### pH Sensor Module
```
pH Module        ESP32
┌────────┐
│  VCC   │ ───── 3.3V or 5V (check your module)
│  GND   │ ───── GND
│  PO    │ ───── GPIO 34 (analog output)
└────────┘
```

### TDS Sensor Module
```
TDS Module       ESP32
┌────────┐
│  VCC   │ ───── 3.3V or 5V (check your module)
│  GND   │ ───── GND
│  A     │ ───── GPIO 35 (analog output)
└────────┘
```

### Relay Modules
```
Relay Module     ESP32        Device
┌──────────┐
│  VCC     │ ─── 5V (ESP32)
│  GND     │ ─── GND (ESP32)
│  IN1     │ ─── GPIO 26     [Heater]
│  IN2     │ ─── GPIO 27     [CO2 Solenoid]
│          │
│  COM1    │ ─┐
│  NO1     │ ─┤──── Heater Power Control (240V AC 🇦🇺)
│  NC1     │ ─┘
│          │
│  COM2    │ ─┐
│  NO2     │ ─┤──── CO2 Solenoid Power Control
│  NC2     │ ─┘
└──────────┘

Note: Use COM and NO (Normally Open) for safety
      Device is OFF when ESP32 is off/crashed
      
⚠️ Australian Electrical Safety (AS/NZS 3000:2018):
   - 240V AC circuits MUST have RCD protection (30mA)
   - Use IP-rated enclosures near water
   - Licensed electrician required for AC wiring
```

### Dosing Pump (L298N Motor Driver)
```
L298N Module     ESP32        Pump Motor
┌──────────┐
│  +12V    │ ─── 12V Supply
│  GND     │ ─── GND (common with ESP32)
│  +5V     │ ─── (leave disconnected or use for ESP32 5V)
│  IN1     │ ─── GPIO 25
│  IN2     │ ─── GPIO 33
│  ENA     │ ─── 5V (or PWM from ESP32)
│          │
│  OUT1    │ ─┐
│  OUT2    │ ─┤──── Dosing Pump Motor (12V DC)
└──────────┘

Note: IN1/IN2 control direction (forward/reverse/brake)
      ENA enables motor (HIGH = enabled)
```

### Display Option 1: Ender 3 Pro LCD12864 (Interactive Menu)
```
LCD12864         ESP32
┌────────┐
│  CS    │ ───── GPIO 15 (Chip Select)
│  A0    │ ───── GPIO 2 (Register Select)
│  RST   │ ───── GPIO 0 (Reset)
│  E     │ ───── GPIO 16 (Enable)
│  R/W   │ ───── GPIO 17 (Read/Write)
│  PSB   │ ───── GPIO 18 (Mode: HIGH=parallel)
│  VCC   │ ───── 5V
│  GND   │ ───── GND
│  BLA   │ ───── 5V (backlight anode)
│  BLK   │ ───── GND (backlight cathode)
└────────┘

Rotary Encoder   ESP32
┌────────┐
│  DT    │ ───── GPIO 13 (Data)
│  CLK   │ ───── GPIO 14 (Clock)
│  SW    │ ───── GPIO 23 (Switch/Button)
│  +     │ ───── 3.3V
│  GND   │ ───── GND
└────────┘

Note: ST7920 controller, 128x64 pixels
      Rotary encoder has internal pullups
      Do NOT connect SD card slot (not implemented)
      9 GPIO pins total
```

### Display Option 2: SSD1309 OLED 128x64 (Monitoring Only)
```
SSD1309 OLED     ESP32
┌────────┐
│  VCC   │ ───── 3.3V or 5V (check your module)
│  GND   │ ───── GND
│  SDA   │ ───── GPIO 21 (I2C Data)
│  SCL   │ ───── GPIO 22 (I2C Clock)
└────────┘

Note: SSD1309 controller, 128x64 OLED
      I2C address typically 0x3C or 0x3D
      Only 2 GPIO pins required
      No user input (monitoring only)
      ~400 KB flash savings vs Ender 3
      See OLED_DISPLAY_GUIDE.md for details
```

## Power Considerations

### ESP32 Power
- **Input Voltage (VIN)**: 5V via USB or external
- **Regulated 3.3V**: From onboard regulator
- **Max Current per GPIO**: 12mA (40mA absolute max)

### Sensor Power Requirements
- **DS18B20**: 3.3V, ~1.5mA (idle) to 2mA (active)
- **pH Module**: Check datasheet (usually 3.3V or 5V, <10mA)
- **TDS Module**: Check datasheet (usually 3.3V or 5V, <10mA)

### Relay Power
- **Relay Module VCC**: Usually 5V
- **Current per relay coil**: ~70mA typical
- **Don't power relays from ESP32 3.3V!** Use 5V pin or external supply

## Analog Input Voltage Ranges

ESP32 ADC characteristics:
- **Maximum Input**: 3.3V (DO NOT EXCEED!)
- **Resolution**: 12-bit (0-4095)
- **Reference**: 3.3V (non-adjustable)

For 5V sensors, use voltage divider:
```
5V Signal ────[10kΩ]────┬──── GPIO 34/35
                        │
                     [6.8kΩ]
                        │
                       GND

Output = 5V × (6.8/(10+6.8)) = 2.02V ✓ Safe
```

## Alternative Pins (if defaults conflict)

You can use these alternative GPIOs:

### For Digital Output (Relays)
- GPIO 2, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33

### For 1-Wire (Temperature)
- Any GPIO except input-only pins (34-39)

### For Analog Input (pH, TDS)
- GPIO 32, 33, 34, 35, 36, 39 (ADC1)
- GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26, 27 (ADC2, but conflicts with WiFi)
- **Recommended**: Use ADC1 pins (32-39) to avoid WiFi conflicts

## Pins to AVOID

| Pin | Reason |
|-----|--------|
| GPIO 0 | Boot mode selection, pulls low at boot |
| GPIO 1 (TX) | Serial communication |
| GPIO 2 | Internal LED, boot mode |
| GPIO 3 (RX) | Serial communication |
| GPIO 5 | Boot mode, SPI |
| GPIO 6-11 | Connected to flash chip (DON'T USE!) |
| GPIO 12 | Boot mode selection |
| GPIO 15 | Boot mode, SPI |

## Modifying Pin Configuration

To change pins, edit the appropriate header files:

### Core System Pins (`ConfigManager.h`)
```cpp
SystemConfig() {
    // ... other settings ...
    
    tempSensorPin = 4;      // Change to your pin
    phSensorPin = 34;       // Must be ADC1 pin
    tdsSensorPin = 35;      // Must be ADC1 pin
    heaterRelayPin = 26;    // Any digital output pin
    co2RelayPin = 27;       // Any digital output pin
}
```

### Display Pins

**Option 1: Ender 3 Display (`DisplayManager.h`)**
```cpp
// ST7920 LCD pins
#define LCD_CS 15      // Chip select
#define LCD_A0 2       // Register select (RS)
#define LCD_RESET 0    // Reset
#define LCD_E 16       // Enable
#define LCD_RW 17      // Read/write
#define LCD_PSB 18     // Parallel/serial select

// Rotary encoder pins
#define ENC_DT 13      // Data
#define ENC_CLK 14     // Clock
#define ENC_SW 23      // Switch/button
```

**Option 2: SSD1309 OLED (`DisplayManager_OLED.h`)**
```cpp
// Hardware I2C (default ESP32 pins)
// SDA: GPIO 21
// SCL: GPIO 22
// I2C Address: 0x3C (most common)

// No defines needed - uses hardware I2C
// Switch by changing: #include "DisplayManager_OLED.h"
```

### Dosing Pump Pins (`main.cpp`)
```cpp
dosingPump = new DosingPump(25, 33, 1);  // IN1, IN2, channel
```

Then rebuild and upload firmware.

## Testing Individual Components

### Test Temperature Sensor
```cpp
// In setup()
tempSensor->begin();
Serial.println(tempSensor->readTemperature());
```

### Test pH Sensor
```cpp
// Raw voltage reading
int raw = analogRead(34);
float voltage = (raw / 4095.0) * 3.3;
Serial.printf("pH Voltage: %.3fV\n", voltage);
```

### Test Relays
```cpp
// Turn on/off
digitalWrite(26, HIGH);  // Heater ON
delay(1000);
digitalWrite(26, LOW);   // Heater OFF
```

## Safety Checklist

- [ ] All grounds connected together
- [ ] No voltage exceeds 3.3V on analog inputs
- [ ] Relays powered from 5V, not 3.3V
- [ ] Pullup resistor on DS18B20 data line
- [ ] Relays set to Normally Open (NO)
- [ ] Proper electrical isolation for AC devices
- [ ] Waterproof connections near aquarium
- [ ] GFCI/RCD protection on AC circuits

---

**Note**: Always verify pinout for YOUR specific ESP32 board. Different manufacturers may have different layouts.
