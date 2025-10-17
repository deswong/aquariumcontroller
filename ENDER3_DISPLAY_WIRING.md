# Ender 3 Pro Display Wiring Guide

## Hardware Requirements

- **ESP32 DevKit** (30-pin or 36-pin version)
- **Ender 3 Pro Full Graphic Smart Controller** (RepRapDiscount compatible)
- **Jumper wires** (female-to-female recommended)
- **5V power supply** (display requires 5V, ESP32 provides 5V out)

---

## Pin Mapping

### ESP32 to Ender 3 Display Connections

| ESP32 GPIO | Function | Display Pin | EXP Connector | Description |
|------------|----------|-------------|---------------|-------------|
| **GPIO 15** | LCD_CS | LCD_EN | EXP1-3 | Chip Select (Enable) |
| **GPIO 2** | LCD_A0 | LCD_RS | EXP1-4 | Data/Command Select |
| **GPIO 0** | LCD_RESET | RESET | EXP1-6 | Display Reset |
| **GPIO 18** | LCD_SCK | E (SCK) | EXP1-9 | SPI Clock |
| **GPIO 23** | LCD_MOSI | R/W (MOSI) | EXP1-5 | SPI Data Out |
| **GPIO 13** | BTN_ENC | BTN_ENC | EXP1-2 | Encoder Button |
| **GPIO 14** | BTN_EN1 | BTN_EN1 | EXP1-7 | Encoder Pin A |
| **GPIO 16** | BTN_EN2 | BTN_EN2 | EXP1-8 | Encoder Pin B |
| **GPIO 17** | BEEPER | BEEPER | EXP1-1 | Buzzer |
| **5V** | Power | +5V | EXP1/2-10 | Display Power |
| **GND** | Ground | GND | EXP1/2-9 | Common Ground |

---

## Ender 3 Display Connectors

The Ender 3 Pro display has two 10-pin IDC ribbon cable connectors (EXP1 and EXP2).

### EXP1 Pinout (Main LCD Control)
```
 ┌─────────────────────┐
 │ 1  BEEPER      5V  2 │  (some boards)
 │ 3  LCD_EN    BTN_ENC 4 │
 │ 5  LCD_D4    LCD_RS  6 │
 │ 7  BTN_EN1   RESET   8 │
 │ 9  BTN_EN2   GND    10 │
 └─────────────────────┘
```

**Pin Functions:**
1. **BEEPER** - Buzzer signal (active high)
2. **NC / +5V** - Not connected or 5V (varies by board)
3. **LCD_EN** - LCD Enable (Chip Select for ST7920)
4. **BTN_ENC** - Encoder button (switch)
5. **LCD_D4** - LCD Data 4 (used as MOSI in serial mode)
6. **LCD_RS** - Register Select (A0/DC)
7. **BTN_EN1** - Encoder phase A
8. **RESET** - Display reset (active low)
9. **BTN_EN2** - Encoder phase B (or SCK in some configs)
10. **GND** - Ground

### EXP2 Pinout (SD Card & Duplicates)
```
 ┌─────────────────────┐
 │ 1  MISO      +5V    2 │
 │ 3  SCK     BTN_ENC  4 │
 │ 5  BTN_EN2 MOSI     6 │
 │ 7  SD_DET  BTN_EN1  8 │
 │ 9  GND     RESET   10 │
 └─────────────────────┘
```

**Note:** EXP2 provides SD card SPI pins and duplicates some encoder pins. We're **NOT using SD card** in this implementation, so EXP2 connections are optional.

---

## Detailed Wiring Instructions

### Step 1: Power Connections
1. Connect **ESP32 5V** pin to **Display EXP1-10** (or EXP2-2) - 5V power
2. Connect **ESP32 GND** pin to **Display EXP1-9** (or EXP2-9) - Ground

⚠️ **Important:** The display requires 5V. Most ESP32 DevKit boards provide a 5V output pin when powered via USB.

### Step 2: LCD Control Pins
3. Connect **ESP32 GPIO 15** to **EXP1-3** (LCD_EN / Chip Select)
4. Connect **ESP32 GPIO 2** to **EXP1-6** (LCD_RS / Data-Command)
5. Connect **ESP32 GPIO 0** to **EXP1-8** (RESET)
6. Connect **ESP32 GPIO 18** to **EXP1-9** (SCK) or **EXP2-3** (SCK)
7. Connect **ESP32 GPIO 23** to **EXP1-5** (MOSI / LCD_D4)

### Step 3: Encoder Connections
8. Connect **ESP32 GPIO 13** to **EXP1-4** (BTN_ENC - Encoder button)
9. Connect **ESP32 GPIO 14** to **EXP1-7** (BTN_EN1 - Encoder A)
10. Connect **ESP32 GPIO 16** to **EXP1-9** (BTN_EN2 - Encoder B)

### Step 4: Buzzer (Optional)
11. Connect **ESP32 GPIO 17** to **EXP1-1** (BEEPER)

---

## Connection Diagram (ASCII)

```
ESP32 DevKit                    Ender 3 Display (EXP1)
┌─────────────┐                 ┌──────────────────┐
│             │                 │                  │
│         5V  ├─────────────────┤ 10  +5V         │
│        GND  ├─────────────────┤ 9   GND         │
│             │                 │                  │
│    GPIO 15  ├─────────────────┤ 3   LCD_EN      │
│    GPIO 2   ├─────────────────┤ 6   LCD_RS      │
│    GPIO 0   ├─────────────────┤ 8   RESET       │
│    GPIO 18  ├─────────────────┤ 9   SCK*        │
│    GPIO 23  ├─────────────────┤ 5   LCD_D4      │
│             │                 │                  │
│    GPIO 13  ├─────────────────┤ 4   BTN_ENC     │
│    GPIO 14  ├─────────────────┤ 7   BTN_EN1     │
│    GPIO 16  ├─────────────────┤ 9   BTN_EN2*    │
│             │                 │                  │
│    GPIO 17  ├─────────────────┤ 1   BEEPER      │
│             │                 │                  │
└─────────────┘                 └──────────────────┘

* Note: Some displays use EXP1-9 for SCK, others use EXP2-3
        Consult your specific display pinout
```

---

## Wiring Notes & Tips

### 1. **IDC Ribbon Cable Considerations**
- Most Ender 3 displays come with 10-pin ribbon cables (EXP1/EXP2)
- You have two options:
  - **Option A:** Cut the ribbon cable and solder wires
  - **Option B:** Use a breakout board/adapter (easier, cleaner)

### 2. **Recommended: Use a Breakout Board**
- Available on Amazon/AliExpress as "RAMPS EXP1/EXP2 breakout"
- Provides labeled screw terminals or pin headers
- Makes rewiring much easier

### 3. **Encoder Wiring**
- Encoders need internal pull-ups (enabled in code via `INPUT_PULLUP`)
- If encoder is unstable, add external 10kΩ pull-up resistors
- Test encoder direction; if backwards, swap EN1 and EN2

### 4. **Boot Pin Precautions**
- **GPIO 0** (RESET): Must be HIGH during boot
- **GPIO 2** (LCD_A0): Must be LOW during flash programming
- If ESP32 won't boot, temporarily disconnect GPIO 0 and GPIO 2

### 5. **Display Contrast**
- ST7920 displays often have a contrast potentiometer on the back
- Adjust if screen is too dark/light
- Software contrast can also be adjusted in code (currently set to 255)

---

## Testing Procedure

### 1. Visual Inspection
- Check all connections are secure
- Verify 5V and GND are correct (reverse polarity can damage display!)
- Look for any shorts or loose wires

### 2. Power On Test
1. Power the ESP32 via USB
2. Display backlight should turn on (if equipped)
3. You should see initialization splash screen
4. Check serial monitor for: `"Display initialized successfully"`

### 3. Encoder Test
1. Rotate encoder - menu selection should change
2. Press encoder button - should hear beep and navigate menus
3. If encoder doesn't work, check EN1/EN2 connections

### 4. Display Test
- Main screen should show:
  - Temperature readings
  - pH value
  - TDS reading
  - Heater/CO2 status
  - Water change prediction

---

## Troubleshooting

### Problem: Display shows nothing (blank screen)
**Solutions:**
- Check 5V power is connected
- Verify GND is connected
- Adjust contrast potentiometer
- Check LCD_EN (CS) pin connection
- Try software contrast: `display->setContrast(128);` in DisplayManager.cpp

### Problem: Display shows garbage/random pixels
**Solutions:**
- Check SCK (clock) connection - loose or wrong pin
- Check MOSI (data) connection
- Verify A0 (RS) pin connection
- Ensure GND is solid connection

### Problem: Encoder doesn't respond
**Solutions:**
- Swap BTN_EN1 and BTN_EN2 pins (reversed direction)
- Add 10kΩ pull-up resistors to encoder pins
- Check button connection (BTN_ENC)
- Verify `INPUT_PULLUP` is enabled in code (it is)

### Problem: ESP32 won't boot with display connected
**Solutions:**
- GPIO 0 must be HIGH at boot (it's used for RESET)
- Temporarily disconnect GPIO 0 and GPIO 2 during upload
- Add 10kΩ pull-up resistor to GPIO 0
- Use a different reset pin (change in code)

### Problem: Beeper doesn't work or stays on
**Solutions:**
- Verify BEEPER pin (GPIO 17) connection
- Check beeper polarity (some are active-low)
- Disable beeper in code if annoying: comment out `beep()` calls

---

## Alternative Pin Assignments

If you have conflicts with other hardware, you can change pins in `DisplayManager.h`:

```cpp
// Current assignments (recommended)
#define LCD_CS      15
#define LCD_A0      2
#define LCD_RESET   0
#define LCD_SCK     18
#define LCD_MOSI    23
#define BTN_ENC     13
#define BTN_EN1     14
#define BTN_EN2     16
#define BEEPER      17

// Alternative assignments (if needed)
// Avoid: GPIO 1, 3 (Serial), GPIO 6-11 (Flash), GPIO 34-39 (Input only)
// Good alternatives: GPIO 12, 19, 21, 22, 32, 33
```

**To change pins:**
1. Edit `include/DisplayManager.h`
2. Update pin definitions
3. Recompile and upload
4. Update your wiring accordingly

---

## Final Complete Pin Assignment Table

### After Ender 3 Display Integration

| GPIO | Function | Component | Type | Notes |
|------|----------|-----------|------|-------|
| **GPIO 0** | LCD_RESET | Display | Output | Pull HIGH at boot |
| **GPIO 2** | LCD_A0 | Display | Output | Pull LOW during flash |
| **GPIO 4** | Water Temp | DS18B20 | OneWire | Temperature sensor |
| **GPIO 5** | Ambient Temp | DS18B20 | OneWire | Ambient sensor |
| **GPIO 13** | BTN_ENC | Display | Input | Encoder button |
| **GPIO 14** | BTN_EN1 | Display | Input | Encoder A |
| **GPIO 15** | LCD_CS | Display | Output | Chip select |
| **GPIO 16** | BTN_EN2 | Display | Input | Encoder B |
| **GPIO 17** | BEEPER | Display | PWM Out | Buzzer |
| **GPIO 18** | LCD_SCK | Display | Output | SPI clock |
| **GPIO 23** | LCD_MOSI | Display | Output | SPI data |
| **GPIO 25** | Dosing IN1 | DRV8871 | PWM Out | Motor driver |
| **GPIO 26** | Heater Relay | Relay | Output | Heater control |
| **GPIO 27** | CO2 Relay | Relay | Output | CO2 solenoid |
| **GPIO 33** | Dosing IN2 | DRV8871 | PWM Out | Motor driver (FIXED!) |
| **GPIO 34** | pH Sensor | ADC | Input | Analog only |
| **GPIO 35** | TDS Sensor | ADC | Input | Analog only |

**Total Pins Used:** 17 of 38 available  
**Remaining GPIO:** 21 pins for future expansion

---

## Physical Mounting Suggestions

### Option 1: 3D Print Enclosure
- Design or find an ESP32 + Display enclosure on Thingiverse
- Mount both ESP32 and display in one case
- Include cutouts for sensors, relays, and power

### Option 2: Panel Mount
- Mount display on front panel of aquarium cabinet
- ESP32 and sensors inside cabinet
- Use ribbon cable extension if needed (up to 30cm recommended)

### Option 3: Desktop Stand
- Use Ender 3 display's existing stand/case
- Position near aquarium
- Connect sensors with longer cables

---

## Software Configuration

After wiring is complete, the display should work automatically. You can customize:

### 1. Update Refresh Rate
Edit `include/DisplayManager.h`:
```cpp
static const unsigned long UPDATE_INTERVAL = 200; // 200ms = 5 Hz
```

### 2. Adjust Screen Timeout
Edit `include/DisplayManager.h`:
```cpp
static const unsigned long SCREEN_TIMEOUT = 300000; // 5 minutes
```

### 3. Change Display Contrast
Edit `src/DisplayManager.cpp` in `begin()` method:
```cpp
display->setContrast(255); // 0-255, adjust as needed
```

### 4. Customize Screens
Edit screen drawing methods in `src/DisplayManager.cpp`:
- `drawMainScreen()` - Main status display
- `drawMenuScreen()` - Menu navigation
- `drawSensorsScreen()` - Detailed sensor view
- `drawControlScreen()` - Control status
- etc.

---

## Safety Considerations

⚠️ **IMPORTANT SAFETY NOTES:**

1. **Water-Proof Enclosure Required**
   - ESP32 and display MUST be in water-proof enclosure
   - Keep away from aquarium water
   - Use IP65+ rated case

2. **Power Safety**
   - Use isolated 5V power supply
   - Don't use aquarium heater outlet for electronics
   - Consider UPS/battery backup

3. **Sensor Isolation**
   - Use optoisolators for relay control (already implemented)
   - Keep low-voltage sensors away from 240V AC mains (Australian standard)
   - Use RCD (Residual Current Device) safety switch for aquarium equipment
   - Ensure all mains-powered devices are double-insulated or earthed

4. **First Power-On**
   - Test outside of final enclosure first
   - Verify all functions work before sealing
   - Check for shorts with multimeter

---

## Resources

### U8g2 Library Documentation
- **GitHub:** https://github.com/olikraus/u8g2
- **Reference:** https://github.com/olikraus/u8g2/wiki
- **Fonts:** https://github.com/olikraus/u8g2/wiki/fntlistall

### Ender 3 Display Info
- **RepRapDiscount Full Graphic Smart Controller**
- **Controller:** ST7920 128x64
- **Operating Mode:** Serial SPI (not parallel)

### ESP32 Pinout
- **Espressif Docs:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
- **DevKit Pinout:** Search "ESP32 DevKit pinout" for your specific board

---

## Summary

✅ **What's Implemented:**
- Full LCD12864 support (ST7920 controller)
- Rotary encoder with button navigation
- Multi-screen menu system
- Real-time sensor display
- Control status indicators
- Water change prediction display
- Buzzer feedback
- Auto sleep after 5 minutes

❌ **What's NOT Implemented:**
- SD card slot (not needed for aquarium controller)
- Touch screen (not on Ender 3 display)
- Color display (monochrome 128x64)

🔧 **Next Steps:**
1. Wire up the display per this guide
2. Power on and verify display initializes
3. Test encoder navigation
4. Customize UI screens as desired
5. Mount in water-proof enclosure
6. Enjoy local control of your aquarium!

---

**Questions or Issues?**
Check serial monitor output for debugging info. All display operations log to serial console.

Happy building! 🐠💧📺
