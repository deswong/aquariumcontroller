#include "DisplayManager.h"
#include "SystemTasks.h"
#include <Arduino.h>
#include <WiFi.h>

// Constructor
DisplayManager::DisplayManager() {
    display = nullptr;
    encoderPos = 0;
    lastEncoderPos = 0;
    buttonPressed = false;
    lastButtonState = HIGH;
    lastButtonTime = 0;
    lastEncoderTime = 0;
    currentScreen = SCREEN_MAIN;
    menuSelection = 0;
    inMenu = false;
    lastUpdate = 0;
    lastInteraction = 0;
    screenOn = true;
    
    // Initialize cached values
    cachedTemp = 0.0f;
    cachedPH = 0.0f;
    cachedTDS = 0.0f;
    cachedAmbientTemp = 0.0f;
    cachedTargetTemp = 0.0f;
    cachedTargetPH = 0.0f;
    cachedHeaterState = false;
    cachedCO2State = false;
    cachedWaterChangeDays = 0.0f;
    cachedWaterChangeConfidence = 0;
}

// Destructor
DisplayManager::~DisplayManager() {
    if (display) {
        delete display;
    }
}

// Initialize display and pins
bool DisplayManager::begin() {
    Serial.println("Initializing display...");
    
    // Initialize encoder pins
    pinMode(BTN_ENC, INPUT_PULLUP);
    pinMode(BTN_EN1, INPUT_PULLUP);
    pinMode(BTN_EN2, INPUT_PULLUP);
    
    // Initialize buzzer
    pinMode(BEEPER, OUTPUT);
    digitalWrite(BEEPER, LOW);
    
    // Create display object (Software SPI, Full Buffer mode)
    display = new U8G2_ST7920_128X64_F_SW_SPI(U8G2_R0, LCD_SCK, LCD_MOSI, LCD_CS, LCD_RESET);
    
    if (!display) {
        Serial.println("Failed to create display object!");
        return false;
    }
    
    // Initialize display
    display->begin();
    display->setContrast(255);
    
    // Show splash screen
    display->clearBuffer();
    display->setFont(u8g2_font_ncenB10_tr);
    display->drawStr(10, 20, "Aquarium");
    display->drawStr(10, 40, "Controller");
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(30, 55, "Initializing...");
    display->sendBuffer();
    
    beepConfirm();
    delay(2000);
    
    Serial.println("Display initialized successfully");
    return true;
}

// Main update loop
void DisplayManager::update() {
    unsigned long now = millis();
    
    // Check for screen timeout
    if (screenOn && (now - lastInteraction > SCREEN_TIMEOUT)) {
        sleep();
        return;
    }
    
    // Read encoder and button
    readEncoder();
    readButton();
    
    // Handle menu navigation
    if (inMenu || currentScreen != SCREEN_MAIN) {
        handleMenuNavigation();
    }
    
    // Update display at fixed interval
    if (now - lastUpdate >= UPDATE_INTERVAL) {
        lastUpdate = now;
        
        if (screenOn) {
            display->clearBuffer();
            
            switch (currentScreen) {
                case SCREEN_MAIN:
                    drawMainScreen();
                    break;
                case SCREEN_MENU:
                    drawMenuScreen();
                    break;
                case SCREEN_SENSORS:
                    drawSensorsScreen();
                    break;
                case SCREEN_CONTROL:
                    drawControlScreen();
                    break;
                case SCREEN_WATER_CHANGE:
                    drawWaterChangeScreen();
                    break;
                case SCREEN_DOSING:
                    drawDosingScreen();
                    break;
                case SCREEN_SETTINGS:
                    drawSettingsScreen();
                    break;
                case SCREEN_INFO:
                    drawInfoScreen();
                    break;
            }
            
            display->sendBuffer();
        }
    }
}

// Read rotary encoder
void DisplayManager::readEncoder() {
    unsigned long now = millis();
    
    if (now - lastEncoderTime < ENCODER_DEBOUNCE) {
        return;
    }
    
    static int lastCLK = HIGH;
    int currentCLK = digitalRead(BTN_EN1);
    int currentDT = digitalRead(BTN_EN2);
    
    if (currentCLK != lastCLK && currentCLK == LOW) {
        if (currentDT != currentCLK) {
            encoderPos++;
        } else {
            encoderPos--;
        }
        lastEncoderTime = now;
        lastInteraction = now;
        wake();
    }
    
    lastCLK = currentCLK;
}

// Read encoder button
void DisplayManager::readButton() {
    unsigned long now = millis();
    bool currentState = digitalRead(BTN_ENC);
    
    if (currentState != lastButtonState) {
        if (now - lastButtonTime > DEBOUNCE_DELAY) {
            lastButtonState = currentState;
            lastButtonTime = now;
            
            if (currentState == LOW) {
                buttonPressed = true;
                lastInteraction = now;
                beep(30);
                wake();
            }
        }
    }
}

// Handle menu navigation
void DisplayManager::handleMenuNavigation() {
    int delta = encoderPos - lastEncoderPos;
    lastEncoderPos = encoderPos;
    
    if (delta != 0) {
        menuSelection += delta;
        
        // Wrap around menu
        if (currentScreen == SCREEN_MENU) {
            if (menuSelection < 0) menuSelection = MENU_COUNT - 1;
            if (menuSelection >= MENU_COUNT) menuSelection = 0;
        }
    }
    
    if (buttonPressed) {
        buttonPressed = false;
        
        if (currentScreen == SCREEN_MAIN) {
            // Enter menu
            currentScreen = SCREEN_MENU;
            menuSelection = 0;
            inMenu = true;
        } else if (currentScreen == SCREEN_MENU) {
            // Select menu item
            switch (menuSelection) {
                case MENU_SENSORS:
                    currentScreen = SCREEN_SENSORS;
                    break;
                case MENU_CONTROL:
                    currentScreen = SCREEN_CONTROL;
                    break;
                case MENU_WATER_CHANGE:
                    currentScreen = SCREEN_WATER_CHANGE;
                    break;
                case MENU_DOSING:
                    currentScreen = SCREEN_DOSING;
                    break;
                case MENU_SETTINGS:
                    currentScreen = SCREEN_SETTINGS;
                    break;
                case MENU_INFO:
                    currentScreen = SCREEN_INFO;
                    break;
                case MENU_BACK:
                    currentScreen = SCREEN_MAIN;
                    inMenu = false;
                    break;
            }
        } else {
            // Back to menu from any screen
            currentScreen = SCREEN_MENU;
        }
    }
}

// Beep sounds
void DisplayManager::beep(int duration) {
    digitalWrite(BEEPER, HIGH);
    delay(duration);
    digitalWrite(BEEPER, LOW);
}

void DisplayManager::beepError() {
    beep(100);
    delay(50);
    beep(100);
}

void DisplayManager::beepConfirm() {
    beep(50);
    delay(50);
    beep(50);
}

// Draw main screen
void DisplayManager::drawMainScreen() {
    drawHeader("Aquarium Status");
    
    // Temperature
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(0, 20, "Temp:");
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1fC [%.1fC]", cachedTemp, cachedTargetTemp);
    display->drawStr(35, 20, tempStr);
    
    // pH
    display->drawStr(0, 30, "pH:");
    char phStr[16];
    snprintf(phStr, sizeof(phStr), "%.2f [%.2f]", cachedPH, cachedTargetPH);
    display->drawStr(35, 30, phStr);
    
    // TDS
    display->drawStr(0, 40, "TDS:");
    char tdsStr[16];
    snprintf(tdsStr, sizeof(tdsStr), "%.0f ppm", cachedTDS);
    display->drawStr(35, 40, tdsStr);
    
    // Status line
    display->drawStr(0, 50, "Heat:");
    display->drawStr(30, 50, cachedHeaterState ? "ON " : "OFF");
    display->drawStr(60, 50, "CO2:");
    display->drawStr(85, 50, cachedCO2State ? "ON " : "OFF");
    
    // Water change prediction
    if (cachedWaterChangeDays > 0) {
        display->drawStr(0, 60, "Next WC:");
        char wcStr[16];
        snprintf(wcStr, sizeof(wcStr), "%.1f days", cachedWaterChangeDays);
        display->drawStr(50, 60, wcStr);
    }
    
    // Press for menu indicator
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw menu screen
void DisplayManager::drawMenuScreen() {
    drawHeader("Main Menu");
    
    const char* menuItems[] = {
        "Sensor Data",
        "Control",
        "Water Change",
        "Dosing Pump",
        "Settings",
        "Info",
        "< Back"
    };
    
    int startItem = 0;
    if (menuSelection > 3) {
        startItem = menuSelection - 3;
    }
    
    for (int i = 0; i < 5 && (startItem + i) < MENU_COUNT; i++) {
        bool selected = (startItem + i) == menuSelection;
        drawMenuItem(menuItems[startItem + i], 20 + (i * 10), selected);
    }
}

// Draw sensors screen
void DisplayManager::drawSensorsScreen() {
    drawHeader("Sensor Data");
    
    display->setFont(u8g2_font_6x10_tr);
    
    char str[32];
    
    // Water temperature
    snprintf(str, sizeof(str), "Water: %.2fC", cachedTemp);
    display->drawStr(0, 20, str);
    
    // Ambient temperature
    snprintf(str, sizeof(str), "Ambient: %.2fC", cachedAmbientTemp);
    display->drawStr(0, 30, str);
    
    // pH
    snprintf(str, sizeof(str), "pH: %.2f", cachedPH);
    display->drawStr(0, 40, str);
    
    // TDS
    snprintf(str, sizeof(str), "TDS: %.0f ppm", cachedTDS);
    display->drawStr(0, 50, str);
    
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw control screen
void DisplayManager::drawControlScreen() {
    drawHeader("Control Status");
    
    display->setFont(u8g2_font_6x10_tr);
    
    // Heater
    display->drawStr(0, 20, "Heater:");
    display->drawStr(60, 20, cachedHeaterState ? "ON" : "OFF");
    
    // Target temp
    char str[32];
    snprintf(str, sizeof(str), "Target: %.1fC", cachedTargetTemp);
    display->drawStr(0, 30, str);
    
    // CO2
    display->drawStr(0, 45, "CO2 Solenoid:");
    display->drawStr(80, 45, cachedCO2State ? "ON" : "OFF");
    
    // Target pH
    snprintf(str, sizeof(str), "Target: %.2f", cachedTargetPH);
    display->drawStr(0, 55, str);
    
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw water change screen
void DisplayManager::drawWaterChangeScreen() {
    drawHeader("Water Change");
    
    display->setFont(u8g2_font_6x10_tr);
    
    if (cachedWaterChangeDays > 0) {
        char str[32];
        snprintf(str, sizeof(str), "Predicted: %.1f days", cachedWaterChangeDays);
        display->drawStr(0, 25, str);
        
        snprintf(str, sizeof(str), "Confidence: %d%%", cachedWaterChangeConfidence);
        display->drawStr(0, 35, str);
        
        // Progress bar
        float daysUntilChange = cachedWaterChangeDays;
        float maxDays = 14.0f; // Assume 2 weeks max
        float percent = 100.0f - ((daysUntilChange / maxDays) * 100.0f);
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        
        display->drawStr(0, 48, "TDS Buildup:");
        drawProgressBar(0, 50, 128, 8, percent);
    } else {
        display->drawStr(10, 30, "Learning...");
        display->drawStr(10, 40, "Need more data");
    }
    
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw dosing screen
void DisplayManager::drawDosingScreen() {
    drawHeader("Dosing Pump");
    
    display->setFont(u8g2_font_6x10_tr);
    
    // Status
    if (dosingPump) {
        bool isRunning = dosingPump->isDosing();
        display->drawStr(0, 20, "Status:");
        display->drawStr(60, 20, isRunning ? "RUNNING" : "IDLE");
        
        // Today's volume
        char str[32];
        float todayVolume = dosingPump->getDailyVolumeDosed();
        snprintf(str, sizeof(str), "Today: %.1f mL", todayVolume);
        display->drawStr(0, 30, str);
        
        // Max daily limit
        float maxDaily = dosingPump->getRemainingDailyVolume() + todayVolume;
        snprintf(str, sizeof(str), "Max: %.1f mL", maxDaily);
        display->drawStr(0, 40, str);
        
        // Progress bar
        float percent = (todayVolume / maxDaily) * 100.0f;
        if (percent > 100) percent = 100;
        drawProgressBar(0, 45, 128, 8, percent);
    } else {
        display->drawStr(20, 30, "Not available");
    }
    
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw settings screen
void DisplayManager::drawSettingsScreen() {
    drawHeader("Settings");
    
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(10, 30, "Configure via");
    display->drawStr(10, 40, "Web Interface");
    
    // Show IP if WiFi connected
    if (WiFi.status() == WL_CONNECTED) {
        display->setFont(u8g2_font_5x7_tr);
        display->drawStr(10, 52, WiFi.localIP().toString().c_str());
    }
    
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw info screen
void DisplayManager::drawInfoScreen() {
    drawHeader("System Info");
    
    display->setFont(u8g2_font_6x10_tr);
    
    // Uptime
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    char str[32];
    if (days > 0) {
        snprintf(str, sizeof(str), "Uptime: %lud %luh", days, hours % 24);
    } else {
        snprintf(str, sizeof(str), "Uptime: %luh %lum", hours, minutes % 60);
    }
    display->drawStr(0, 20, str);
    
    // WiFi status
    display->drawStr(0, 30, "WiFi:");
    if (WiFi.status() == WL_CONNECTED) {
        display->drawStr(40, 30, "Connected");
        int rssi = WiFi.RSSI();
        snprintf(str, sizeof(str), "RSSI: %d dBm", rssi);
        display->drawStr(0, 40, str);
    } else {
        display->drawStr(40, 30, "Disconnected");
    }
    
    // Free heap
    snprintf(str, sizeof(str), "Free RAM: %d KB", ESP.getFreeHeap() / 1024);
    display->drawStr(0, 50, str);
    
    display->setFont(u8g2_font_5x7_tr);
    display->drawStr(30, 63, "[Press for menu]");
}

// Draw header
void DisplayManager::drawHeader(const char* title) {
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(0, 10, title);
    display->drawHLine(0, 12, 128);
}

// Draw menu item
void DisplayManager::drawMenuItem(const char* text, int y, bool selected) {
    display->setFont(u8g2_font_6x10_tr);
    
    if (selected) {
        display->drawBox(0, y - 8, 128, 10);
        display->setDrawColor(0);
        display->drawStr(5, y, text);
        display->setDrawColor(1);
    } else {
        display->drawStr(5, y, text);
    }
}

// Draw progress bar
void DisplayManager::drawProgressBar(int x, int y, int width, int height, float percent) {
    // Draw outline
    display->drawFrame(x, y, width, height);
    
    // Draw fill
    int fillWidth = (int)((width - 2) * (percent / 100.0f));
    if (fillWidth > 0) {
        display->drawBox(x + 1, y + 1, fillWidth, height - 2);
    }
}

// Update methods
void DisplayManager::updateTemperature(float temp, float target) {
    cachedTemp = temp;
    cachedTargetTemp = target;
}

void DisplayManager::updatePH(float ph, float target) {
    cachedPH = ph;
    cachedTargetPH = target;
}

void DisplayManager::updateTDS(float tds) {
    cachedTDS = tds;
}

void DisplayManager::updateAmbientTemperature(float temp) {
    cachedAmbientTemp = temp;
}

void DisplayManager::updateHeaterState(bool state) {
    cachedHeaterState = state;
}

void DisplayManager::updateCO2State(bool state) {
    cachedCO2State = state;
}

void DisplayManager::updateWaterChangePrediction(float days, int confidence) {
    cachedWaterChangeDays = days;
    cachedWaterChangeConfidence = confidence;
}

// Screen control
void DisplayManager::setScreen(MenuScreen screen) {
    currentScreen = screen;
}

void DisplayManager::wake() {
    if (!screenOn) {
        screenOn = true;
        display->setPowerSave(0);
        beep(20);
    }
}

void DisplayManager::sleep() {
    screenOn = false;
    display->setPowerSave(1);
}

// Encoder access
int DisplayManager::getEncoderDelta() {
    int delta = encoderPos - lastEncoderPos;
    lastEncoderPos = encoderPos;
    return delta;
}

bool DisplayManager::wasButtonPressed() {
    if (buttonPressed) {
        buttonPressed = false;
        return true;
    }
    return false;
}

// Display test
void DisplayManager::test() {
    display->clearBuffer();
    display->setFont(u8g2_font_ncenB10_tr);
    display->drawStr(20, 30, "Display Test");
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(30, 45, "All Systems OK");
    display->sendBuffer();
    
    beepConfirm();
    delay(2000);
}
