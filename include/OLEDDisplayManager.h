#ifndef OLED_DISPLAY_MANAGER_H
#define OLED_DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

class OLEDDisplayManager {
private:
    U8G2_SSD1309_128X64_NONAME0_F_HW_I2C* display;
    
    // Display state
    unsigned long lastUpdate;
    unsigned long lastScreenSwitch;
    unsigned long lastAnimation;
    unsigned long lastDataUpdate;
    uint8_t currentScreen;
    uint8_t animationFrame;
    bool needsRedraw;
    bool dataChanged;
    
    // Timing constants - Optimized for performance
    static const unsigned long FAST_UPDATE_INTERVAL = 100;   // 10 Hz for animations
    static const unsigned long DATA_UPDATE_INTERVAL = 500;   // 2 Hz for data
    static const unsigned long SCREEN_SWITCH_INTERVAL = 5000; // 5 seconds
    static const unsigned long ANIMATION_INTERVAL = 125;     // 8 FPS (smoother)
    static const uint8_t NUM_SCREENS = 3;
    
    // Sensor data
    float currentTemp;
    float targetTemp;
    float currentPH;
    float targetPH;
    float currentTDS;
    float ambientTemp;
    bool heaterActive;
    bool co2Active;
    bool dosingActive;
    char waterChangeDate[16];  // "2024-12-31" + null terminator
    bool wifiConnected;
    char ipAddress[16];        // "255.255.255.255" + null terminator
    char currentTime[9];       // "23:59:59" + null terminator
    
    // Trends (for graphing) - Reduced size for memory efficiency
    static const uint8_t TREND_SIZE = 24;  // Reduced from 32 to save ~96 bytes
    float tempTrend[TREND_SIZE];
    float phTrend[TREND_SIZE];
    float tdsTrend[TREND_SIZE];
    uint8_t trendIndex;
    
    // Performance monitoring
    bool performanceMonitoring;
    unsigned long updateCount;
    unsigned long totalUpdateTime;
    
    // Private methods
    void drawScreen0(); // Main status screen
    void drawScreen1(); // Graph screen
    void drawScreen2(); // Network & system info
    
    void drawStatusBar();
    void drawIcon(uint8_t x, uint8_t y, const uint8_t* icon);
    void drawGraph(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float* data, uint8_t len, float min, float max);
    void drawProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float value, float min, float max);
    void drawWaveAnimation(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    void addToTrend(float* trend, float value);
    
    // Icons (8x8 bitmaps)
    static const uint8_t ICON_TEMP[];
    static const uint8_t ICON_PH[];
    static const uint8_t ICON_TDS[];
    static const uint8_t ICON_HEATER[];
    static const uint8_t ICON_CO2[];
    static const uint8_t ICON_WIFI[];
    static const uint8_t ICON_WIFI_OFF[];
    static const uint8_t ICON_DROPLET[];
    static const uint8_t ICON_CALENDAR[];
    static const uint8_t ICON_DOSING[];

public:
    OLEDDisplayManager();
    ~OLEDDisplayManager();
    
    bool begin();
    void update();
    void clear();
    void setBrightness(uint8_t brightness);
    void setContrast(uint8_t contrast);
    void test();
    
    // Data update methods
    void updateTemperature(float current, float target);
    void updatePH(float current, float target);
    void updateTDS(float tds);
    void updateAmbientTemperature(float temp);
    void updateHeaterState(bool active);
    void updateCO2State(bool active);
    void updateDosingState(bool active);
    void updateWaterChangeDate(const char* date);
    void updateNetworkStatus(bool connected, const char* ip);
    void updateTime(const char* time);
    
    // Screen control
    void nextScreen();
    void setScreen(uint8_t screen);
    
    // Performance optimization
    void forceRedraw();              // Force immediate redraw
    void enablePerformanceMonitoring(bool enable);
    unsigned long getLastUpdateTime() const { return lastUpdate; }
};

#endif // OLED_DISPLAY_MANAGER_H