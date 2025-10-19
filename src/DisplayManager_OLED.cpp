#include "DisplayManager_OLED.h"
#include <Arduino.h>

// Icon definitions (8x8 bitmaps)
const uint8_t DisplayManager::ICON_TEMP[] = {
    0b00011000,
    0b00100100,
    0b00100100,
    0b00100100,
    0b01111110,
    0b11111111,
    0b11111111,
    0b01111110
};

const uint8_t DisplayManager::ICON_PH[] = {
    0b11111110,
    0b10000010,
    0b10111010,
    0b10101010,
    0b10111010,
    0b10000010,
    0b10000010,
    0b11111110
};

const uint8_t DisplayManager::ICON_TDS[] = {
    0b01000010,
    0b10100101,
    0b01000010,
    0b00000000,
    0b01000010,
    0b10100101,
    0b01000010,
    0b00000000
};

const uint8_t DisplayManager::ICON_HEATER[] = {
    0b00100100,
    0b01001001,
    0b10010010,
    0b00100100,
    0b01001001,
    0b10010010,
    0b00100100,
    0b01000010
};

const uint8_t DisplayManager::ICON_CO2[] = {
    0b01111110,
    0b10000001,
    0b10011001,
    0b10100101,
    0b10100101,
    0b10011001,
    0b10000001,
    0b01111110
};

const uint8_t DisplayManager::ICON_WIFI[] = {
    0b00111100,
    0b01000010,
    0b10011001,
    0b00100100,
    0b00011000,
    0b00000000,
    0b00011000,
    0b00011000
};

const uint8_t DisplayManager::ICON_WIFI_OFF[] = {
    0b10111101,
    0b01000011,
    0b10011000,
    0b00100101,
    0b00011010,
    0b00000100,
    0b00011000,
    0b00011000
};

const uint8_t DisplayManager::ICON_DROPLET[] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100
};

const uint8_t DisplayManager::ICON_CALENDAR[] = {
    0b11111111,
    0b10000001,
    0b11111111,
    0b10101001,
    0b10101001,
    0b10101001,
    0b10101001,
    0b11111111
};

const uint8_t DisplayManager::ICON_DOSING[] = {
    0b00111100,
    0b01000010,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b00111100,
    0b00011000
};

DisplayManager::DisplayManager() 
    : display(nullptr),
      lastUpdate(0),
      lastScreenSwitch(0),
      currentScreen(0),
      currentTemp(0),
      targetTemp(0),
      currentPH(0),
      targetPH(0),
      currentTDS(0),
      ambientTemp(0),
      heaterActive(false),
      co2Active(false),
      dosingActive(false),
      waterChangeDate("Never"),
      wifiConnected(false),
      ipAddress("0.0.0.0"),
      currentTime("--:--:--"),
      trendIndex(0),
      animationFrame(0),
      lastAnimation(0)
{
    // Initialize trends
    for (uint8_t i = 0; i < TREND_SIZE; i++) {
        tempTrend[i] = 0;
        phTrend[i] = 0;
        tdsTrend[i] = 0;
    }
}

DisplayManager::~DisplayManager() {
    if (display) {
        delete display;
    }
}

bool DisplayManager::begin() {
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

void DisplayManager::update() {
    unsigned long now = millis();
    
    // Handle animation
    if (now - lastAnimation >= ANIMATION_INTERVAL) {
        lastAnimation = now;
        animationFrame = (animationFrame + 1) % 8;
    }
    
    // Auto-switch screens
    if (now - lastScreenSwitch >= SCREEN_SWITCH_INTERVAL) {
        lastScreenSwitch = now;
        currentScreen = (currentScreen + 1) % NUM_SCREENS;
    }
    
    // Update display
    if (now - lastUpdate >= UPDATE_INTERVAL) {
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
    }
}

void DisplayManager::drawStatusBar() {
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
    display->drawStr(88, 9, currentTime.c_str());
    
    // Screen indicator dots
    for (uint8_t i = 0; i < NUM_SCREENS; i++) {
        if (i == currentScreen) {
            display->drawDisc(57 + i * 6, 6, 2);
        } else {
            display->drawCircle(57 + i * 6, 6, 2);
        }
    }
}

void DisplayManager::drawScreen0() {
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
    display->drawStr(28, 56, waterChangeDate.c_str());
    
    // Wave animation at bottom
    drawWaveAnimation(0, 58, 128, 6);
}

void DisplayManager::drawScreen1() {
    // Graph screen
    display->setFont(u8g2_font_5x7_tf);
    
    // Temperature graph
    display->drawStr(2, 18, "Temp");
    drawGraph(2, 20, 124, 12, tempTrend, TREND_SIZE, 20.0, 30.0);
    
    // pH graph
    display->drawStr(2, 38, "pH");
    drawGraph(2, 40, 124, 12, phTrend, TREND_SIZE, 6.0, 8.0);
    
    // TDS graph
    display->drawStr(2, 58, "TDS");
    drawGraph(2, 60, 124, 4, tdsTrend, TREND_SIZE, 0, 500);
}

void DisplayManager::drawScreen2() {
    // Network & system info
    display->setFont(u8g2_font_6x10_tf);
    
    // WiFi status
    drawIcon(2, 14, wifiConnected ? ICON_WIFI : ICON_WIFI_OFF);
    display->drawStr(14, 21, wifiConnected ? "Connected" : "Disconnected");
    
    // IP address
    display->setFont(u8g2_font_5x7_tf);
    display->drawStr(14, 29, ipAddress.c_str());
    
    // Ambient temperature
    display->setFont(u8g2_font_6x10_tf);
    drawIcon(2, 38, ICON_TEMP);
    char ambStr[16];
    snprintf(ambStr, sizeof(ambStr), "Room: %.1fC", ambientTemp);
    display->drawStr(14, 45, ambStr);
    
    // Uptime
    unsigned long uptime = millis() / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    char uptimeStr[20];
    snprintf(uptimeStr, sizeof(uptimeStr), "Up: %luh %lum", hours, minutes);
    display->drawStr(2, 56, uptimeStr);
}

void DisplayManager::drawIcon(uint8_t x, uint8_t y, const uint8_t* icon) {
    display->drawXBMP(x, y, 8, 8, icon);
}

void DisplayManager::drawGraph(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float* data, uint8_t len, float min, float max) {
    // Draw frame
    display->drawFrame(x, y, w, h);
    
    // Draw graph line
    float scale = (h - 2) / (max - min);
    uint8_t step = w / len;
    
    for (uint8_t i = 0; i < len - 1; i++) {
        float val1 = constrain(data[i], min, max);
        float val2 = constrain(data[i + 1], min, max);
        
        uint8_t y1 = y + h - 1 - (uint8_t)((val1 - min) * scale);
        uint8_t y2 = y + h - 1 - (uint8_t)((val2 - min) * scale);
        
        display->drawLine(x + 1 + i * step, y1, x + 1 + (i + 1) * step, y2);
    }
}

void DisplayManager::drawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float value, float min, float max) {
    display->drawFrame(x, y, w, h);
    
    float percentage = (value - min) / (max - min);
    percentage = constrain(percentage, 0.0, 1.0);
    
    uint8_t fillWidth = (uint8_t)((w - 2) * percentage);
    if (fillWidth > 0) {
        display->drawBox(x + 1, y + 1, fillWidth, h - 2);
    }
}

void DisplayManager::drawWaveAnimation(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint8_t i = 0; i < w; i++) {
        float wave = sin((i + animationFrame * 2) * 0.2) * (h / 2.0);
        uint8_t waveY = y + h / 2 + (int8_t)wave;
        display->drawPixel(x + i, waveY);
    }
}

void DisplayManager::addToTrend(float* trend, float value) {
    trend[trendIndex] = value;
}

void DisplayManager::updateTemperature(float current, float target) {
    currentTemp = current;
    targetTemp = target;
    addToTrend(tempTrend, current);
}

void DisplayManager::updatePH(float current, float target) {
    currentPH = current;
    targetPH = target;
    addToTrend(phTrend, current);
}

void DisplayManager::updateTDS(float tds) {
    currentTDS = tds;
    addToTrend(tdsTrend, tds);
    trendIndex = (trendIndex + 1) % TREND_SIZE;
}

void DisplayManager::updateAmbientTemperature(float temp) {
    ambientTemp = temp;
}

void DisplayManager::updateHeaterState(bool active) {
    heaterActive = active;
}

void DisplayManager::updateCO2State(bool active) {
    co2Active = active;
}

void DisplayManager::updateDosingState(bool active) {
    dosingActive = active;
}

void DisplayManager::updateWaterChangeDate(const char* date) {
    waterChangeDate = String(date);
}

void DisplayManager::updateNetworkStatus(bool connected, const char* ip) {
    wifiConnected = connected;
    ipAddress = String(ip);
}

void DisplayManager::updateTime(const char* time) {
    currentTime = String(time);
}

void DisplayManager::clear() {
    display->clearBuffer();
    display->sendBuffer();
}

void DisplayManager::setBrightness(uint8_t brightness) {
    display->setContrast(brightness);
}

void DisplayManager::setContrast(uint8_t contrast) {
    display->setContrast(contrast);
}

void DisplayManager::nextScreen() {
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
    lastScreenSwitch = millis();
}

void DisplayManager::setScreen(uint8_t screen) {
    if (screen < NUM_SCREENS) {
        currentScreen = screen;
        lastScreenSwitch = millis();
    }
}

void DisplayManager::test() {
    display->clearBuffer();
    display->setFont(u8g2_font_ncenB14_tr);
    display->drawStr(20, 30, "Display");
    display->drawStr(30, 50, "Test OK");
    display->sendBuffer();
}
