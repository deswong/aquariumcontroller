#include "OLEDDisplayManager.h"
#include <Arduino.h>
#include <WiFi.h>

// Icon definitions (8x8 bitmaps)
const uint8_t OLEDDisplayManager::ICON_TEMP[] = {
    0b00011000,
    0b00100100,
    0b00100100,
    0b00100100,
    0b01111110,
    0b11111111,
    0b11111111,
    0b01111110
};

const uint8_t OLEDDisplayManager::ICON_PH[] = {
    0b11111110,
    0b10000010,
    0b10111010,
    0b10101010,
    0b10111010,
    0b10000010,
    0b10000010,
    0b11111110
};

const uint8_t OLEDDisplayManager::ICON_TDS[] = {
    0b01000010,
    0b10100101,
    0b01000010,
    0b00000000,
    0b01000010,
    0b10100101,
    0b01000010,
    0b00000000
};

const uint8_t OLEDDisplayManager::ICON_HEATER[] = {
    0b00100100,
    0b01001001,
    0b10010010,
    0b00100100,
    0b01001001,
    0b10010010,
    0b00100100,
    0b01000010
};

const uint8_t OLEDDisplayManager::ICON_CO2[] = {
    0b01111110,
    0b10000001,
    0b10011001,
    0b10100101,
    0b10100101,
    0b10011001,
    0b10000001,
    0b01111110
};

const uint8_t OLEDDisplayManager::ICON_WIFI[] = {
    0b00111100,
    0b01000010,
    0b10011001,
    0b00100100,
    0b00011000,
    0b00000000,
    0b00011000,
    0b00011000
};

const uint8_t OLEDDisplayManager::ICON_WIFI_OFF[] = {
    0b10111101,
    0b01000011,
    0b10011000,
    0b00100101,
    0b00011010,
    0b00000100,
    0b00011000,
    0b00011000
};

const uint8_t OLEDDisplayManager::ICON_DROPLET[] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100
};

const uint8_t OLEDDisplayManager::ICON_CALENDAR[] = {
    0b11111111,
    0b10000001,
    0b11111111,
    0b10101001,
    0b10101001,
    0b10101001,
    0b10101001,
    0b11111111
};

const uint8_t OLEDDisplayManager::ICON_DOSING[] = {
    0b00111100,
    0b01000010,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b00111100,
    0b00011000
};

OLEDDisplayManager::OLEDDisplayManager() 
    : display(nullptr),
      lastUpdate(0),
      lastScreenSwitch(0),
      lastAnimation(0),
      lastDataUpdate(0),
      currentScreen(0),
      animationFrame(0),
      needsRedraw(true),
      dataChanged(true),
      currentTemp(0),
      targetTemp(0),
      currentPH(0),
      targetPH(0),
      currentTDS(0),
      ambientTemp(0),
      heaterActive(false),
      co2Active(false),
      dosingActive(false),
      wifiConnected(false),
      trendIndex(0),
      performanceMonitoring(false),
      updateCount(0),
      totalUpdateTime(0)
{
    // Initialize char arrays
    strcpy(waterChangeDate, "Never");
    strcpy(ipAddress, "0.0.0.0");
    strcpy(currentTime, "--:--:--");
    
    // Initialize trends
    for (uint8_t i = 0; i < TREND_SIZE; i++) {
        tempTrend[i] = 0;
        phTrend[i] = 0;
        tdsTrend[i] = 0;
    }
}
}

OLEDDisplayManager::~OLEDDisplayManager() {
    if (display) {
        delete display;
    }
}

bool OLEDDisplayManager::begin() {
    display = new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
    
    if (!display) {
        return false;
    }
    
    display->begin();
    display->setContrast(128);
    display->clearBuffer();
    display->setFont(u8g2_font_6x10_tf);
    
    // Startup animation
    for (int i = 0; i < 128; i += 8) {
        display->clearBuffer();
        display->drawFrame(0, 0, i, 64);
        display->sendBuffer();
        delay(10);
    }
    
    display->clearBuffer();
    display->setFont(u8g2_font_ncenB14_tr);
    display->drawStr(10, 30, "Aquarium");
    display->drawStr(10, 50, "Controller");
    display->sendBuffer();
    delay(1000);
    
    return true;
}

void OLEDDisplayManager::update() {
    unsigned long now = millis();
    unsigned long updateStart = performanceMonitoring ? micros() : 0;
    bool shouldAnimate = false;
    bool shouldRedraw = false;
    
    // Handle animation timing (8 FPS - smoother)
    if (now - lastAnimation >= ANIMATION_INTERVAL) {
        lastAnimation = now;
        animationFrame = (animationFrame + 1) % 8;
        shouldAnimate = true;
    }
    
    // Auto-switch screens
    if (now - lastScreenSwitch >= SCREEN_SWITCH_INTERVAL) {
        lastScreenSwitch = now;
        currentScreen = (currentScreen + 1) % NUM_SCREENS;
        needsRedraw = true;
    }
    
    // Check if we need to update display
    if (now - lastUpdate >= FAST_UPDATE_INTERVAL) {
        // Only redraw if something changed or animation needs update
        if (needsRedraw || dataChanged || shouldAnimate) {
            lastUpdate = now;
            
            display->clearBuffer();
            
            switch (currentScreen) {
                case 0:
                    drawScreen0();
                    break;
                case 1:
                    drawScreen1();
                    break;
                case 2:
                    drawScreen2();
                    break;
            }
            
            drawStatusBar();
            display->sendBuffer();
            
            // Reset flags
            needsRedraw = false;
            dataChanged = false;
            
            // Performance monitoring
            if (performanceMonitoring) {
                updateCount++;
                totalUpdateTime += (micros() - updateStart);
                
                // Print stats every 100 updates
                if (updateCount % 100 == 0) {
                    Serial.printf("[Display] Avg update time: %lu μs\n", 
                                totalUpdateTime / updateCount);
                }
            }
        }
    }
}



void OLEDDisplayManager::updateTemperature(float current, float target) {
    if (currentTemp != current || targetTemp != target) {
        currentTemp = current;
        targetTemp = target;
        addToTrend(tempTrend, current);
        dataChanged = true;
    }
}

void OLEDDisplayManager::updatePH(float current, float target) {
    if (currentPH != current || targetPH != target) {
        currentPH = current;
        targetPH = target;
        addToTrend(phTrend, current);
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateTDS(float tds) {
    if (currentTDS != tds) {
        currentTDS = tds;
        addToTrend(tdsTrend, tds);
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateAmbientTemperature(float temp) {
    if (ambientTemp != temp) {
        ambientTemp = temp;
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateHeaterState(bool active) {
    if (heaterActive != active) {
        heaterActive = active;
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateCO2State(bool active) {
    if (co2Active != active) {
        co2Active = active;
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateDosingState(bool active) {
    if (dosingActive != active) {
        dosingActive = active;
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateWaterChangeDate(const char* date) {
    if (strcmp(waterChangeDate, date) != 0) {
        strncpy(waterChangeDate, date, sizeof(waterChangeDate) - 1);
        waterChangeDate[sizeof(waterChangeDate) - 1] = '\0';
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateNetworkStatus(bool connected, const char* ip) {
    bool statusChanged = false;
    
    if (wifiConnected != connected) {
        wifiConnected = connected;
        statusChanged = true;
    }
    
    if (strcmp(ipAddress, ip) != 0) {
        strncpy(ipAddress, ip, sizeof(ipAddress) - 1);
        ipAddress[sizeof(ipAddress) - 1] = '\0';
        statusChanged = true;
    }
    
    if (statusChanged) {
        dataChanged = true;
    }
}

void OLEDDisplayManager::updateTime(const char* time) {
    if (strcmp(currentTime, time) != 0) {
        strncpy(currentTime, time, sizeof(currentTime) - 1);
        currentTime[sizeof(currentTime) - 1] = '\0';
        dataChanged = true;
    }
}

void OLEDDisplayManager::clear() {
    if (display) {
        display->clearBuffer();
        display->sendBuffer();
    }
}

void OLEDDisplayManager::setBrightness(uint8_t brightness) {
    if (display) {
        display->setContrast(brightness);
    }
}

void OLEDDisplayManager::setContrast(uint8_t contrast) {
    if (display) {
        display->setContrast(contrast);
    }
}

void OLEDDisplayManager::test() {
    if (!display) return;
    
    display->clearBuffer();
    display->setFont(u8g2_font_ncenB10_tr);
    display->drawStr(10, 20, "Display Test");
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(10, 35, "SSD1309 OLED");
    display->drawStr(10, 50, "All systems OK");
    display->sendBuffer();
}

void OLEDDisplayManager::nextScreen() {
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
    lastScreenSwitch = millis();
    needsRedraw = true;
}

void OLEDDisplayManager::setScreen(uint8_t screen) {
    if (screen < NUM_SCREENS && screen != currentScreen) {
        currentScreen = screen;
        lastScreenSwitch = millis();
        needsRedraw = true;
    }
}

void OLEDDisplayManager::addToTrend(float* trend, float value) {
    trend[trendIndex] = value;
    trendIndex = (trendIndex + 1) % TREND_SIZE;
}

void OLEDDisplayManager::drawStatusBar() {
    // Top status bar
    display->drawLine(0, 10, 127, 10);
    
    // WiFi icon
    if (wifiConnected) {
        drawIcon(2, 2, ICON_WIFI);
    } else {
        drawIcon(2, 2, ICON_WIFI_OFF);
    }
    
    // Time (right aligned)
    display->setFont(u8g2_font_5x7_tf);
    display->drawStr(88, 9, currentTime);
    
    // Screen indicator dots
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        if (i == currentScreen) {
            display->drawDisc(57 + i * 6, 6, 2);
        } else {
            display->drawCircle(57 + i * 6, 6, 2);
        }
    }
}

void OLEDDisplayManager::drawScreen0() {
    // Main status screen with icons and progress bars
    display->setFont(u8g2_font_6x10_tf);
    
    // Temperature
    drawIcon(2, 14, ICON_TEMP);
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1fC", currentTemp);
    display->drawStr(14, 21, tempStr);
    
    if (heaterActive) {
        drawIcon(60, 14, ICON_HEATER);
    }
    
    drawProgressBar(75, 14, 50, 6, currentTemp, 20.0, 30.0);
    snprintf(tempStr, sizeof(tempStr), "%.1f", targetTemp);
    display->setFont(u8g2_font_5x7_tf);
    display->drawStr(110, 20, tempStr);
    
    // pH
    display->setFont(u8g2_font_6x10_tf);
    drawIcon(2, 26, ICON_PH);
    char phStr[16];
    snprintf(phStr, sizeof(phStr), "%.2f", currentPH);
    display->drawStr(14, 33, phStr);
    
    if (co2Active) {
        drawIcon(60, 26, ICON_CO2);
    }
    
    drawProgressBar(75, 26, 50, 6, currentPH, 6.0, 8.0);
    snprintf(phStr, sizeof(phStr), "%.1f", targetPH);
    display->setFont(u8g2_font_5x7_tf);
    display->drawStr(110, 32, phStr);
    
    // TDS
    display->setFont(u8g2_font_6x10_tf);
    drawIcon(2, 38, ICON_TDS);
    char tdsStr[16];
    snprintf(tdsStr, sizeof(tdsStr), "%.0f ppm", currentTDS);
    display->drawStr(14, 45, tdsStr);
    
    if (dosingActive) {
        drawIcon(75, 38, ICON_DOSING);
        display->setFont(u8g2_font_5x7_tf);
        display->drawStr(85, 44, "DOSING");
    }
    
    // Water change
    drawIcon(2, 50, ICON_CALENDAR);
    display->setFont(u8g2_font_5x7_tf);
    display->drawStr(12, 56, "WC:");
    display->drawStr(28, 56, waterChangeDate);
    
    // Wave animation at bottom
    drawWaveAnimation(0, 58, 128, 6);
}

void OLEDDisplayManager::drawScreen1() {
    // Graph screen showing trends
    display->setFont(u8g2_font_5x7_tf);
    
    // Temperature graph
    display->drawStr(2, 20, "Temp");
    drawGraph(2, 22, 40, 15, tempTrend, TREND_SIZE, 20.0, 30.0);
    
    // pH graph
    display->drawStr(45, 20, "pH");
    drawGraph(45, 22, 40, 15, phTrend, TREND_SIZE, 6.0, 8.0);
    
    // TDS graph
    display->drawStr(88, 20, "TDS");
    drawGraph(88, 22, 38, 15, tdsTrend, TREND_SIZE, 0, 1000);
    
    // Current values
    display->setFont(u8g2_font_6x10_tf);
    char buffer[16];
    
    snprintf(buffer, sizeof(buffer), "%.1f", currentTemp);
    display->drawStr(2, 52, buffer);
    
    snprintf(buffer, sizeof(buffer), "%.2f", currentPH);
    display->drawStr(45, 52, buffer);
    
    snprintf(buffer, sizeof(buffer), "%.0f", currentTDS);
    display->drawStr(88, 52, buffer);
    
    // Status indicators
    if (heaterActive) display->drawStr(2, 62, "H");
    if (co2Active) display->drawStr(45, 62, "C");
    if (dosingActive) display->drawStr(88, 62, "D");
}

void OLEDDisplayManager::drawScreen2() {
    // Network & system info screen
    display->setFont(u8g2_font_6x10_tf);
    
    // Network status
    display->drawStr(2, 20, "Network:");
    display->setFont(u8g2_font_5x7_tf);
    if (wifiConnected) {
        display->drawStr(2, 30, "Connected");
        display->drawStr(2, 38, ipAddress);
    } else {
        display->drawStr(2, 30, "Disconnected");
    }
    
    // System info
    display->setFont(u8g2_font_6x10_tf);
    display->drawStr(2, 50, "System:");
    display->setFont(u8g2_font_5x7_tf);
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Uptime: %lum", millis() / 60000);
    display->drawStr(2, 58, buffer);
    
    snprintf(buffer, sizeof(buffer), "Heap: %luK", ESP.getFreeHeap() / 1024);
    display->drawStr(70, 58, buffer);
}

void OLEDDisplayManager::drawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float value, float min, float max) {
    // Draw outline
    display->drawFrame(x, y, w, h);
    
    // Calculate fill width based on value within min/max range
    if (max > min) { // Avoid division by zero
        float percent = constrain((value - min) / (max - min), 0.0, 1.0);
        uint8_t fillWidth = (uint8_t)(percent * (w - 2));
        
        if (fillWidth > 0) {
            display->drawBox(x + 1, y + 1, fillWidth, h - 2);
        }
    }
}

void OLEDDisplayManager::drawGraph(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float* data, uint8_t len, float min, float max) {
    // Draw frame
    display->drawFrame(x, y, w, h);
    
    if (max <= min) return; // Avoid division by zero
    
    // Pre-calculate scale factor
    float scale = (h - 2) / (max - min);
    uint8_t baseY = y + h - 1;
    
    // Draw data points with optimized calculations
    uint8_t step = (len > w - 2) ? len / (w - 2) : 1; // Skip points if too many
    
    for (uint8_t i = 1; i < len && i < w - 2; i += step) {
        if (i > 0) {
            // Constrain values once
            float val1 = constrain(data[i - 1], min, max);
            float val2 = constrain(data[i], min, max);
            
            // Pre-calculated Y positions
            uint8_t y1 = baseY - (uint8_t)((val1 - min) * scale);
            uint8_t y2 = baseY - (uint8_t)((val2 - min) * scale);
            
            display->drawLine(x + i, y1, x + i + step, y2);
        }
    }
}

void OLEDDisplayManager::drawIcon(uint8_t x, uint8_t y, const uint8_t* icon) {
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t line = icon[row];
        for (uint8_t col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                display->drawPixel(x + col, y + row);
            }
        }
    }
}

void OLEDDisplayManager::drawWaveAnimation(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint8_t i = 0; i < w; i++) {
        float wave = sin((i + animationFrame * 4) * 0.2) * (h / 4) + (h / 2);
        uint8_t waveY = y + (uint8_t)wave;
        display->drawPixel(x + i, waveY);
    }
}

void OLEDDisplayManager::forceRedraw() {
    needsRedraw = true;
    dataChanged = true;
}

void OLEDDisplayManager::enablePerformanceMonitoring(bool enable) {
    performanceMonitoring = enable;
    if (enable) {
        updateCount = 0;
        totalUpdateTime = 0;
        Serial.println("[Display] Performance monitoring enabled");
    } else {
        Serial.println("[Display] Performance monitoring disabled");
    }
}