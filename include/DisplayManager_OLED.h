#ifndef DISPLAYMANAGER_OLED_H
#define DISPLAYMANAGER_OLED_H

#include <Arduino.h>
#include <U8g2lib.h>

// I2C pins (using default ESP32 I2C)
// SDA: GPIO 21
// SCL: GPIO 22
// I2C Address: 0x3C (typical for SSD1309)

class DisplayManager {
private:
    // U8g2 display object for SSD1309 128x64 OLED
    U8G2_SSD1309_128X64_NONAME0_F_HW_I2C* display;
    
    // Display update timing
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000; // 1 Hz (once per second)
    
    // Cached sensor data
    float cachedTemp;
    float cachedPH;
    float cachedTDS;
    float cachedAmbientTemp;
    float cachedTargetTemp;
    float cachedTargetPH;
    bool cachedHeaterState;
    bool cachedCO2State;
    String cachedWaterChangeDate;
    String cachedIPAddress;
    bool cachedWiFiConnected;
    
    // Display helper methods
    void drawMainScreen();
    void drawIcon(int x, int y, const char* icon);
    
public:
    DisplayManager();
    ~DisplayManager();
    
    // Initialization
    bool begin();
    
    // Main update loop (call from main loop)
    void update();
    
    // Data update methods (call from sensor tasks)
    void updateTemperature(float temp, float target);
    void updatePH(float ph, float target);
    void updateTDS(float tds);
    void updateAmbientTemperature(float temp);
    void updateHeaterState(bool state);
    void updateCO2State(bool state);
    void updateWaterChangeDate(const String& date);
    void updateNetworkStatus(bool connected, const String& ipAddress);
    
    // Display control
    void clear();
    void setBrightness(uint8_t brightness); // 0-255
    
    // Display test
    void test();
};

#endif // DISPLAYMANAGER_OLED_H
