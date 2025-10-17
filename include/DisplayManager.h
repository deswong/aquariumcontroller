#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>

// Pin definitions for Ender 3 Pro display (RepRapDiscount Full Graphic Smart Controller)
// Software SPI for maximum flexibility
#define LCD_CS      15  // Chip Select
#define LCD_A0      2   // Data/Command (DC)
#define LCD_RESET   0   // Reset
#define LCD_SCK     18  // SPI Clock
#define LCD_MOSI    23  // SPI Data

// Rotary encoder pins
#define BTN_ENC     13  // Encoder button (click)
#define BTN_EN1     14  // Encoder pin A
#define BTN_EN2     16  // Encoder pin B

// Buzzer pin
#define BEEPER      17  // PWM capable for tones

// Menu system
enum MenuScreen {
    SCREEN_MAIN,
    SCREEN_MENU,
    SCREEN_SENSORS,
    SCREEN_CONTROL,
    SCREEN_WATER_CHANGE,
    SCREEN_DOSING,
    SCREEN_SETTINGS,
    SCREEN_INFO
};

enum MenuItem {
    MENU_SENSORS = 0,
    MENU_CONTROL,
    MENU_WATER_CHANGE,
    MENU_DOSING,
    MENU_SETTINGS,
    MENU_INFO,
    MENU_BACK,
    MENU_COUNT
};

class DisplayManager {
private:
    // U8g2 display object (ST7920 128x64 in serial mode)
    U8G2_ST7920_128X64_F_SW_SPI* display;
    
    // Encoder state
    int encoderPos;
    int lastEncoderPos;
    bool buttonPressed;
    bool lastButtonState;
    unsigned long lastButtonTime;
    unsigned long lastEncoderTime;
    
    // Menu state
    MenuScreen currentScreen;
    int menuSelection;
    bool inMenu;
    
    // Display update timing
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 200; // 5 Hz
    
    // Debounce settings
    static const unsigned long DEBOUNCE_DELAY = 50;
    static const unsigned long ENCODER_DEBOUNCE = 10;
    
    // Screen timeout
    unsigned long lastInteraction;
    static const unsigned long SCREEN_TIMEOUT = 300000; // 5 minutes
    bool screenOn;
    
    // Helper methods
    void readEncoder();
    void readButton();
    void handleMenuNavigation();
    void beep(int duration = 50);
    void beepError();
    void beepConfirm();
    
    // Screen drawing methods
    void drawMainScreen();
    void drawMenuScreen();
    void drawSensorsScreen();
    void drawControlScreen();
    void drawWaterChangeScreen();
    void drawDosingScreen();
    void drawSettingsScreen();
    void drawInfoScreen();
    
    // UI elements
    void drawHeader(const char* title);
    void drawStatusBar();
    void drawMenuItem(const char* text, int y, bool selected);
    void drawProgressBar(int x, int y, int width, int height, float percent);
    void drawGraph(int x, int y, int width, int height, float* data, int dataCount, float minVal, float maxVal);
    
    // Sensor data cache (updated externally)
    float cachedTemp;
    float cachedPH;
    float cachedTDS;
    float cachedAmbientTemp;
    float cachedTargetTemp;
    float cachedTargetPH;
    bool cachedHeaterState;
    bool cachedCO2State;
    float cachedWaterChangeDays;
    int cachedWaterChangeConfidence;
    
public:
    DisplayManager();
    ~DisplayManager();
    
    // Initialization
    bool begin();
    
    // Main update loop (call frequently)
    void update();
    
    // Data update methods (call from sensor tasks)
    void updateTemperature(float temp, float target);
    void updatePH(float ph, float target);
    void updateTDS(float tds);
    void updateAmbientTemperature(float temp);
    void updateHeaterState(bool state);
    void updateCO2State(bool state);
    void updateWaterChangePrediction(float days, int confidence);
    
    // Screen control
    void setScreen(MenuScreen screen);
    void wake();
    void sleep();
    bool isAwake() { return screenOn; }
    
    // Encoder access (for external control if needed)
    int getEncoderDelta();
    bool wasButtonPressed();
    
    // Display test
    void test();
};

#endif // DISPLAYMANAGER_H
