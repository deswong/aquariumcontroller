# ⚠️ DEPRECATED: Unified Display Manager 

> **Notice:** This unified display manager has been **replaced** by the streamlined `OLEDDisplayManager` which supports only the SSD1309 OLED display. The Ender 3 LCD support has been removed from the project.
>
> **Please use:** [OLED_DISPLAY_MANAGER.md](OLED_DISPLAY_MANAGER.md) for current display implementation.

---

## Migration Path

If you're using the unified display manager, please migrate to the new OLED-only implementation:

### Old Code (Deprecated)
```cpp
#include "UnifiedDisplayManager.h"
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_SSD1309_OLED);
```

### New Code (Current)  
```cpp
#include "OLEDDisplayManager.h"
OLEDDisplayManager* displayMgr = new OLEDDisplayManager();
```

All method calls remain the same - only the class name and constructor change.

---

## Historical Documentation (For Reference Only)

The `UnifiedDisplayManager` previously consolidated both display types into a single, configurable class that could handle either:
- **ST7920 LCD** (Ender 3 Pro display with rotary encoder) - **REMOVED**
- **SSD1309 OLED** (I2C OLED with automatic screen cycling) - **Now OLEDDisplayManager**

## Usage

### 1. Include the Header
```cpp
#include "UnifiedDisplayManager.h"
```

### 2. Create Display Manager Instance

**For ST7920 LCD (default):**
```cpp
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_ST7920_LCD);
// or simply:
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager();
```

**For SSD1309 OLED:**
```cpp
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_SSD1309_OLED);
```

### 3. Initialize and Use
```cpp
void setup() {
    // Initialize display
    if (!displayMgr->begin()) {
        Serial.println("Display initialization failed!");
        return;
    }
    
    // Configure display
    displayMgr->setContrast(128);
}

void loop() {
    // Update display (handles input and rendering automatically)
    displayMgr->update();
    
    // Update sensor data
    displayMgr->updateTemperature(25.5, 26.0);
    displayMgr->updatePH(7.2, 7.0);
    displayMgr->updateTDS(450);
    displayMgr->updateHeaterState(true);
    displayMgr->updateCO2State(false);
    
    // Update network info
    displayMgr->updateNetworkStatus(WiFi.status() == WL_CONNECTED, WiFi.localIP().toString().c_str());
    displayMgr->updateTime("12:34:56");
}
```

## Build Configuration

### Option 1: Compile-time Selection
Add to your `platformio.ini`:

```ini
# For ST7920 LCD
build_flags = -DUSE_ST7920_LCD

# For SSD1309 OLED  
build_flags = -DUSE_SSD1309_OLED
```

Then in your main code:
```cpp
#ifdef USE_ST7920_LCD
    UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_ST7920_LCD);
#elif defined(USE_SSD1309_OLED)
    UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_SSD1309_OLED);
#else
    // Default to LCD
    UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_ST7920_LCD);
#endif
```

### Option 2: Runtime Configuration
Store display type in configuration:
```cpp
// In ConfigManager or similar
enum DisplayType {
    DISPLAY_TYPE_LCD = 0,
    DISPLAY_TYPE_OLED = 1
};

// Load from EEPROM/NVS and create appropriate display
DisplayType savedType = (DisplayType)configMgr->getDisplayType();
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(
    savedType == DISPLAY_TYPE_LCD ? DISPLAY_ST7920_LCD : DISPLAY_SSD1309_OLED
);
```

## Features Comparison

| Feature | ST7920 LCD | SSD1309 OLED |
|---------|------------|---------------|
| **Input** | Rotary encoder + button | Auto-cycling screens |
| **Interaction** | Full menu system | View-only |
| **Screens** | 8 menu screens | 3 auto-cycling screens |
| **Audio** | Buzzer feedback | Silent |
| **Power** | Sleep on timeout | Always on |
| **Trends** | Static values | Historical graphs |

## API Methods

### Common Methods
```cpp
bool begin();                              // Initialize display
void update();                             // Main update loop (call frequently)
void clear();                              // Clear display
void setBrightness(uint8_t brightness);    // Set display brightness
void setContrast(uint8_t contrast);        // Set display contrast
void test();                               // Display test screen

// Data updates
void updateTemperature(float current, float target);
void updatePH(float current, float target);
void updateTDS(float tds);
void updateAmbientTemperature(float temp);
void updateHeaterState(bool active);
void updateCO2State(bool active);
void updateDosingState(bool active);
void updateWaterChangeData(float days, int confidence);
void updateWaterChangeDate(const char* date);
void updateNetworkStatus(bool connected, const char* ip);
void updateTime(const char* time);

// Display type detection
DisplayType getDisplayType() const;
bool isLCD() const;
bool isOLED() const;
```

### LCD-Specific Methods
```cpp
void enterMenu();                          // Enter menu system
void exitMenu();                           // Exit to main screen
MenuScreen getCurrentScreen() const;       // Get current menu screen
```

### OLED-Specific Methods  
```cpp
void nextScreen();                         // Manually advance to next screen
void setScreen(uint8_t screen);            // Set specific screen (0-2)
```

## Pin Assignments

### ST7920 LCD Pins
```cpp
#define LCD_CS      15  // Chip Select
#define LCD_A0      2   // Data/Command (DC)
#define LCD_RESET   0   // Reset
#define LCD_SCK     18  // SPI Clock
#define LCD_MOSI    23  // SPI Data

#define BTN_ENC     13  // Encoder button (click)
#define BTN_EN1     14  // Encoder pin A
#define BTN_EN2     16  // Encoder pin B
#define BEEPER      17  // Buzzer
```

### SSD1309 OLED Pins
```cpp
// Hardware I2C (pins defined by ESP32)
// SDA: GPIO 21
// SCL: GPIO 22
// No additional pins required
```

## Migration from Separate Classes

### From DisplayManager (LCD)
Replace:
```cpp
#include "DisplayManager.h"
DisplayManager* displayMgr = new DisplayManager();
```

With:
```cpp
#include "UnifiedDisplayManager.h"
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_ST7920_LCD);
```

### From DisplayManager_OLED 
Replace:
```cpp
#include "DisplayManager_OLED.h"
DisplayManager* displayMgr = new DisplayManager();
```

With:
```cpp
#include "UnifiedDisplayManager.h"
UnifiedDisplayManager* displayMgr = new UnifiedDisplayManager(DISPLAY_SSD1309_OLED);
```

All existing method calls remain compatible!

## Benefits of Unified Approach

1. **Single codebase** - No duplicate code maintenance
2. **Runtime switching** - Change display types via configuration  
3. **Consistent API** - Same methods work for both displays
4. **Future extensibility** - Easy to add new display types
5. **Reduced complexity** - One class to manage instead of two
6. **Automatic optimization** - Display-specific features only active when needed

## Next Steps

1. Update main.cpp to use UnifiedDisplayManager
2. Add display type to configuration system
3. Test both display types with unified manager
4. Remove old DisplayManager and DisplayManager_OLED files
5. Update documentation and examples