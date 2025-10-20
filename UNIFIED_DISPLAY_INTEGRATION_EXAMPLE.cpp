// Example integration of UnifiedDisplayManager into main.cpp
// This shows how to modify the existing main.cpp to use the unified display manager

// At the top of main.cpp, replace:
// #include "DisplayManager.h"
// with:
#include "UnifiedDisplayManager.h"

// Global variable declaration (replace existing displayMgr declaration)
UnifiedDisplayManager* displayMgr = nullptr;

// In the setup() function, replace the existing display initialization:
void setup() {
    // ... other initialization code ...
    
    // Initialize Display Manager
    Serial.println("\nInitializing unified display...");
    
    // Choose display type based on build configuration or runtime setting
    DisplayType displayType = DISPLAY_ST7920_LCD;  // Default to LCD
    
    #ifdef USE_SSD1309_OLED
        displayType = DISPLAY_SSD1309_OLED;
    #elif defined(USE_ST7920_LCD)
        displayType = DISPLAY_ST7920_LCD;
    #else
        // Runtime configuration example:
        // You could read this from ConfigManager if you have display type stored
        // SystemConfig& config = configMgr->getConfig();
        // displayType = config.displayType == 1 ? DISPLAY_SSD1309_OLED : DISPLAY_ST7920_LCD;
    #endif
    
    displayMgr = new UnifiedDisplayManager(displayType);
    
    if (!displayMgr->begin()) {
        Serial.println("WARNING: Display initialization failed!");
        if (eventLogger) {
            eventLogger->warning("display", "Display initialization failed");
        }
        delete displayMgr;
        displayMgr = nullptr;
    } else {
        const char* displayTypeName = displayMgr->isLCD() ? "ST7920 LCD" : "SSD1309 OLED";
        Serial.printf("Display initialized successfully: %s\n", displayTypeName);
        if (eventLogger) {
            eventLogger->info("display", displayTypeName);
        }
    }
    
    // ... rest of setup code ...
}

// In your main loop or sensor update function, add display updates:
void updateDisplay() {
    if (!displayMgr) return;
    
    // Update display with current sensor data
    if (tempSensor) {
        float temp = tempSensor->getTemperature();
        float targetTemp = tempController ? tempController->getSetpoint() : 26.0;
        displayMgr->updateTemperature(temp, targetTemp);
    }
    
    if (phSensor) {
        float ph = phSensor->getPH();
        float targetPH = phController ? phController->getSetpoint() : 7.0;
        displayMgr->updatePH(ph, targetPH);
    }
    
    if (tdsSensor) {
        displayMgr->updateTDS(tdsSensor->getTDS());
    }
    
    if (ambientTempSensor) {
        displayMgr->updateAmbientTemperature(ambientTempSensor->getTemperature());
    }
    
    // Update control states
    displayMgr->updateHeaterState(relayController && relayController->getHeaterState());
    displayMgr->updateCO2State(relayController && relayController->getCO2State());
    displayMgr->updateDosingState(dosingPump && dosingPump->isActive());
    
    // Update water change prediction
    if (wcPredictor) {
        float days = wcPredictor->getDaysUntilChange();
        int confidence = wcPredictor->getConfidence();
        displayMgr->updateWaterChangeData(days, confidence);
    }
    
    // Update network status
    bool connected = wifiMgr && wifiMgr->isConnected();
    String ip = connected ? WiFi.localIP().toString() : "0.0.0.0";
    displayMgr->updateNetworkStatus(connected, ip.c_str());
    
    // Update time (you might have an NTP time source)
    // displayMgr->updateTime(getCurrentTimeString().c_str());
    
    // Call the display update (this should be called frequently)
    displayMgr->update();
}

// In your main loop:
void loop() {
    // ... existing loop code ...
    
    // Update display
    updateDisplay();
    
    // ... rest of loop ...
}

// Optional: Add to platformio.ini for compile-time display selection
/*
Add to platformio.ini build_flags:

For LCD:
build_flags = 
    -D USE_ST7920_LCD
    ; ... other flags

For OLED:  
build_flags = 
    -D USE_SSD1309_OLED
    ; ... other flags
*/

// Optional: Runtime configuration in ConfigManager
/*
Add to your config structure:
struct SystemConfig {
    // ... existing fields ...
    uint8_t displayType;  // 0 = LCD, 1 = OLED
    // ... other fields ...
};

Then use:
SystemConfig& config = configMgr->getConfig();
DisplayType displayType = (config.displayType == 1) ? DISPLAY_SSD1309_OLED : DISPLAY_ST7920_LCD;
displayMgr = new UnifiedDisplayManager(displayType);
*/