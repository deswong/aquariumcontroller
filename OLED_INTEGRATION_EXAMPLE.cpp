// Integration example for OLEDDisplayManager in main.cpp
// This shows the minimal changes needed to integrate the OLED display manager

// Replace the old display manager include:
// #include "DisplayManager.h"
// #include "DisplayManager_OLED.h" 
// #include "UnifiedDisplayManager.h"

// With:
#include "OLEDDisplayManager.h"

// Global variable (replace existing displayMgr declaration)
OLEDDisplayManager* displayMgr = nullptr;

// In setup() function:
void setup() {
    // ... existing initialization code ...
    
    // Replace existing display initialization with:
    Serial.println("\nInitializing OLED display...");
    displayMgr = new OLEDDisplayManager();
    
    if (!displayMgr->begin()) {
        Serial.println("WARNING: OLED display initialization failed!");
        if (eventLogger) {
            eventLogger->warning("display", "OLED display initialization failed");
        }
        delete displayMgr;
        displayMgr = nullptr;
    } else {
        Serial.println("OLED display initialized successfully");
        if (eventLogger) {
            eventLogger->info("display", "SSD1309 OLED display initialized");
        }
    }
    
    // ... rest of setup code ...
}

// Add this function to update display with sensor data:
void updateDisplayData() {
    if (!displayMgr) return;
    
    // Update temperature data
    if (tempSensor) {
        float temp = tempSensor->getTemperature();
        float targetTemp = tempController ? tempController->getSetpoint() : 26.0;
        displayMgr->updateTemperature(temp, targetTemp);
    }
    
    // Update pH data
    if (phSensor) {
        float ph = phSensor->getPH();
        float targetPH = phController ? phController->getSetpoint() : 7.0;
        displayMgr->updatePH(ph, targetPH);
    }
    
    // Update TDS data
    if (tdsSensor) {
        displayMgr->updateTDS(tdsSensor->getTDS());
    }
    
    // Update ambient temperature
    if (ambientTempSensor) {
        displayMgr->updateAmbientTemperature(ambientTempSensor->getTemperature());
    }
    
    // Update control states
    if (relayController) {
        displayMgr->updateHeaterState(relayController->getHeaterState());
        displayMgr->updateCO2State(relayController->getCO2State());
    }
    
    // Update dosing state
    if (dosingPump) {
        displayMgr->updateDosingState(dosingPump->isActive());
    }
    
    // Update water change date (if you have this info)
    if (wcPredictor) {
        // You might have a method to get last water change date
        displayMgr->updateWaterChangeDate("2025-10-15");  // Example date
    }
    
    // Update network status
    if (wifiMgr) {
        bool connected = wifiMgr->isConnected();
        String ip = connected ? WiFi.localIP().toString() : "0.0.0.0";
        displayMgr->updateNetworkStatus(connected, ip.c_str());
    }
    
    // Update time (if you have NTP or RTC)
    // displayMgr->updateTime("12:34:56");  // Format: HH:MM:SS
    
    // Update the display (handles screen cycling and animations)
    displayMgr->update();
}

// In your main loop, call the display update:
void loop() {
    // ... existing loop code ...
    
    // Update display data (call this regularly)
    updateDisplayData();
    
    // ... rest of loop code ...
}

/*
Optional: If you want to remove the old display manager files after testing:

1. Test the new OLED display manager thoroughly
2. Once confirmed working, remove old files:
   - include/DisplayManager.h
   - src/DisplayManager.cpp
   - include/DisplayManager_OLED.h  
   - src/DisplayManager_OLED.cpp
   - UNIFIED_DISPLAY_MANAGER.md (if not needed)
   - UNIFIED_DISPLAY_INTEGRATION_EXAMPLE.cpp

3. Update any documentation references to point to OLEDDisplayManager
*/