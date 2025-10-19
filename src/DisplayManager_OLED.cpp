#include "DisplayManager_OLED.h"
#include <WiFi.h>
#include <time.h>

// Constructor
DisplayManager::DisplayManager() 
    : lastUpdate(0), 
      cachedTemp(0.0), 
      cachedPH(0.0), 
      cachedTDS(0.0),
      cachedAmbientTemp(0.0),
      cachedTargetTemp(0.0),
      cachedTargetPH(0.0),
      cachedHeaterState(false),
      cachedCO2State(false),
      cachedWiFiConnected(false),
      cachedWaterChangeDate("N/A"),
      cachedIPAddress("0.0.0.0") {
    
    // Initialize display object for SSD1309 128x64 OLED
    // Using hardware I2C on default ESP32 pins (SDA=21, SCL=22)
    display = new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
}

// Destructor
DisplayManager::~DisplayManager() {
    if (display) {
        delete display;
        display = nullptr;
    }
}

// Initialize the display
bool DisplayManager::begin() {
    if (!display) {
        Serial.println("Display object is null");
        return false;
    }
    
    if (!display->begin()) {
        Serial.println("SSD1309 OLED display initialization failed");
        return false;
    }
    
    Serial.println("SSD1309 OLED display initialized successfully");
    
    // Set contrast (0-255, 128 is medium)
    display->setContrast(128);
    
    // Clear display
    display->clearBuffer();
    display->sendBuffer();
    
    return true;
}

// Main update loop - call this frequently from main loop
void DisplayManager::update() {
    unsigned long now = millis();
    
    // Update display at 1 Hz
    if (now - lastUpdate >= UPDATE_INTERVAL) {
        lastUpdate = now;
        drawMainScreen();
    }
}

// Draw the main information screen
void DisplayManager::drawMainScreen() {
    display->clearBuffer();
    
    // Set font for normal text
    display->setFont(u8g2_font_6x10_tf);
    
    // Line 1: Temperature with heater indicator
    display->setCursor(0, 10);
    display->print("Temp: ");
    display->print(cachedTemp, 1);
    display->print("C ");
    if (cachedHeaterState) {
        display->print("[HEAT]");
    } else {
        display->print("(");
        display->print(cachedTargetTemp, 1);
        display->print("C)");
    }
    
    // Line 2: pH with CO2 indicator
    display->setCursor(0, 22);
    display->print("pH: ");
    display->print(cachedPH, 2);
    display->print(" ");
    if (cachedCO2State) {
        display->print("[CO2]");
    } else {
        display->print("(");
        display->print(cachedTargetPH, 2);
        display->print(")");
    }
    
    // Line 3: TDS
    display->setCursor(0, 34);
    display->print("TDS: ");
    display->print(cachedTDS, 0);
    display->print(" ppm");
    
    // Line 4: Ambient temperature (if available)
    if (cachedAmbientTemp > 0) {
        display->setCursor(0, 46);
        display->print("Room: ");
        display->print(cachedAmbientTemp, 1);
        display->print("C");
    }
    
    // Line 5: WiFi status and IP
    display->setCursor(0, 58);
    if (cachedWiFiConnected) {
        display->print("WiFi: ");
        display->print(cachedIPAddress);
    } else {
        display->print("WiFi: Disconnected");
    }
    
    // Right side: Status icons and info
    // Display current time if NTP is synchronized
    time_t now = time(nullptr);
    if (now > 1000000000) {  // Valid NTP time
        struct tm* timeinfo = localtime(&now);
        char timeStr[9];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);
        
        display->setFont(u8g2_font_5x7_tf);
        display->setCursor(82, 10);
        display->print(timeStr);
    }
    
    // Water change date (small font, right aligned)
    display->setFont(u8g2_font_5x7_tf);
    display->setCursor(0, 10);
    display->print("Last WC: ");
    display->print(cachedWaterChangeDate);
    
    // Send buffer to display
    display->sendBuffer();
}

// Update temperature data
void DisplayManager::updateTemperature(float temp, float target) {
    cachedTemp = temp;
    cachedTargetTemp = target;
}

// Update pH data
void DisplayManager::updatePH(float ph, float target) {
    cachedPH = ph;
    cachedTargetPH = target;
}

// Update TDS data
void DisplayManager::updateTDS(float tds) {
    cachedTDS = tds;
}

// Update ambient temperature
void DisplayManager::updateAmbientTemperature(float temp) {
    cachedAmbientTemp = temp;
}

// Update heater state
void DisplayManager::updateHeaterState(bool state) {
    cachedHeaterState = state;
}

// Update CO2 state
void DisplayManager::updateCO2State(bool state) {
    cachedCO2State = state;
}

// Update water change date
void DisplayManager::updateWaterChangeDate(const String& date) {
    cachedWaterChangeDate = date;
}

// Update network status
void DisplayManager::updateNetworkStatus(bool connected, const String& ipAddress) {
    cachedWiFiConnected = connected;
    cachedIPAddress = ipAddress;
}

// Clear the display
void DisplayManager::clear() {
    display->clearBuffer();
    display->sendBuffer();
}

// Set display brightness
void DisplayManager::setBrightness(uint8_t brightness) {
    display->setContrast(brightness);
}

// Display test - shows that display is working
void DisplayManager::test() {
    Serial.println("Running SSD1309 OLED display test...");
    
    display->clearBuffer();
    display->setFont(u8g2_font_ncenB14_tr);
    
    // Draw test pattern
    display->setCursor(10, 30);
    display->print("Aquarium");
    display->setCursor(10, 50);
    display->print("Controller");
    display->sendBuffer();
    
    delay(2000);
    
    // Draw box test
    display->clearBuffer();
    display->drawFrame(0, 0, 128, 64);
    display->drawFrame(5, 5, 118, 54);
    display->setFont(u8g2_font_6x10_tf);
    display->setCursor(20, 35);
    display->print("Display OK");
    display->sendBuffer();
    
    delay(2000);
    
    // Fill test
    display->clearBuffer();
    for (int i = 0; i < 128; i += 4) {
        display->drawVLine(i, 0, 64);
    }
    display->sendBuffer();
    
    delay(1000);
    
    display->clearBuffer();
    display->sendBuffer();
    
    Serial.println("Display test complete");
}
