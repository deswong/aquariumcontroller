# ESP32 Aquarium Controller - Pin Reference

## Default Pin Configuration

**Total Pins Used:**
- **Without Display:** 8 GPIO pins (30 free)
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

### Display: SSD1309 OLED (2 pins) - Monitoring Only
| Function | GPIO Pin | Type | Notes |
|----------|----------|------|-------|
| I2C SDA | GPIO 21 | I2C Data | Hardware I2C, internal pullup |
| I2C SCL | GPIO 22 | I2C Clock | Hardware I2C, internal pullup |

## ESP32 DevKit v1 Pin Layout

```
                        ┌─────────┐
                     EN │1      38│ GND
                  GPIO36│2      37│ GPIO23
                  GPIO39│3      36│ GPIO22 ◄── I2C SCL
                  GPIO34│4      35│ GPIO21 ◄── I2C SDA
                  GPIO35│5      34│ GPIO19
                  GPIO32│6      33│ GPIO18
                  GPIO33│7      32│ GND
                  GPIO25│8      31│ GPIO5  ◄── Ambient Temp
                  GPIO26│9      30│ GPIO17
                  GPIO27│10     29│ GPIO16
                  GPIO14│11     28│ GPIO4  ◄── Water Temp
                  GPIO12│12     27│ GPIO0
                     GND│13     26│ GPIO2
                  GPIO13│14     25│ GPIO15
                   GPIO9│15     24│ GPIO8
                  GPIO10│16     23│ GPIO7
                  GPIO11│17     22│ GPIO6
                     VIN│18     21│ 3.3V
                     GND│19     20│ 5V
                        └─────────┘
```

## Communication Interfaces

### I2C Configuration
```
Default pins:
SDA (GPIO21) ◄── I2C Data
SCL (GPIO22) ◄── I2C Clock

Note: Use 4.7kΩ pullup resistors if not included on modules
```

### ADC Considerations
- ADC1 (GPIO32-39): Available with WiFi
- ADC2: Not usable when WiFi is active
- Input voltage range: 0-3.3V
- Resolution: 12-bit (0-4095)

## Detailed Wiring

### Temperature Sensors (DS18B20)
```
DS18B20          ESP32
┌────────┐
│  VCC   │ ───── 3.3V
│  DATA  │ ───── GPIO 4 (Water) / GPIO 5 (Ambient)
│  GND   │ ───── GND
└────────┘
         │
        4.7kΩ (pullup to 3.3V)
```

### pH Sensor
```
pH Sensor        ESP32
┌────────┐
│  VCC   │ ───── 3.3V or 5V (check sensor spec)
│  OUT   │ ───── GPIO 34 (ADC1_CH6)
│  GND   │ ───── GND
└────────┘

Note: Add 0.1µF capacitor between OUT and GND for noise filtering
```

### TDS Sensor
```
TDS Sensor       ESP32
┌────────┐
│  VCC   │ ───── 3.3V or 5V (check sensor spec)
│  OUT   │ ───── GPIO 35 (ADC1_CH7)
│  GND   │ ───── GND
└────────┘

Note: Add 0.1µF capacitor between OUT and GND for noise filtering
```

### Relay Modules
```
Relay Module     ESP32
┌────────┐
│  VCC   │ ───── 5V
│  IN1   │ ───── GPIO 26 (Heater)
│  IN2   │ ───── GPIO 27 (CO2)
│  GND   │ ───── GND
└────────┘

Note: Use optocoupler-based relay modules for isolation
```

### L298N Motor Driver (Dosing Pump)
```
L298N            ESP32
┌────────┐
│  VCC   │ ───── 5V or 12V (motor voltage)
│  GND   │ ───── GND
│  IN1   │ ───── GPIO 25
│  IN2   │ ───── GPIO 33
│  ENA   │ ───── 5V (enable always on)
│  OUT1  │ ───── Pump Motor +
│  OUT2  │ ───── Pump Motor -
└────────┘
```

### SSD1309 OLED Display
```
SSD1309 OLED     ESP32
┌────────┐
│  VCC   │ ───── 3.3V
│  GND   │ ───── GND
│  SDA   │ ───── GPIO 21
│  SCL   │ ───── GPIO 22
└────────┘

Note: 4.7kΩ pullup resistors on SDA/SCL if not included on module
```

## Sensor Specifications

### Analog Sensors

| Sensor | Operating Range | Typical Values |
|--------|----------------|----------------|
| pH Sensor | 0-3.3V | pH 7 ≈ 1.5V |
| TDS Sensor | 0-2.5V | 0 TDS ≈ 0V |

### Digital Sensors

| Sensor | Protocol | Update Rate |
|--------|----------|------------|
| DS18B20 | 1-Wire | 750ms per reading |
| SSD1309 | I2C | ~60Hz refresh |

## Power Management

### Deep Sleep Considerations
- RTC GPIOs (0,2,4,12-15,25-27,32-39) remain active
- Use RTC GPIOs for wake-up sensors
- Other GPIOs reset on wake
- Typical deep sleep current: <10µA

### Power Budget
```
Component         Current Draw
─────────────────────────────
ESP32 (active)    ~80mA
ESP32 (WiFi TX)   ~160mA
ESP32 (sleep)     ~10µA
DS18B20           ~1.5mA
pH Module         ~10mA
TDS Module        ~10mA
Relay (per coil)  ~70mA
SSD1309 OLED      ~20mA
L298N (idle)      ~20mA
L298N (active)    ~100mA+
```

## I2C Device Configuration

Default I2C pins for ESP32:
- **SDA**: GPIO 21
- **SCL**: GPIO 22

Compatible with common aquarium sensors:
- Atlas Scientific I2C circuits
- BME280 environmental sensors
- SSD1309/SSD1306 OLED displays

### I2C Wiring Example
```
I2C Device       ESP32
┌────────┐
│  VCC   │ ───── 3.3V
│  GND   │ ───── GND
│  SDA   │ ───── GPIO 21
│  SCL   │ ───── GPIO 22
└────────┘

Note: Use 4.7kΩ pullup resistors on both SDA and SCL if not included on modules
```

## Troubleshooting Guide

### Common Issues

1. **Erratic readings**
   - Check ground connections
   - Verify power supply stability
   - Add decoupling capacitors (0.1µF near each sensor)

2. **WiFi disconnections**
   - Move ADC sensors to ADC1 pins (GPIO32-39)
   - Check power supply capacity
   - Reduce WiFi TX power if needed

3. **Sensor failures**
   - Verify voltage levels with multimeter
   - Check pullup resistors on I2C and 1-Wire
   - Test cables for continuity

4. **OLED display not working**
   - Verify I2C address (usually 0x3C or 0x3D)
   - Check SDA/SCL connections
   - Ensure 4.7kΩ pullup resistors present

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.2 | 2025-10-20 | Removed Ender 3 LCD, kept only SSD1309 OLED |
| 1.1 | 2025-10-17 | Added I2C, deep sleep, and troubleshooting sections |
| 1.0 | 2025-10-01 | Initial documentation |
