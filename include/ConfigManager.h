#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "ESP32_Random.h"

struct SystemConfig {
    // WiFi settings
    char wifiSSID[32];
    char wifiPassword[64];
    
    // MQTT settings
    char mqttServer[64];
    int mqttPort;
    char mqttUser[32];
    char mqttPassword[64];
    char mqttClientId[32];
    char mqttTopicPrefix[64];
    bool mqttPublishIndividual;  // Publish to individual topics (temp, ph, etc.)
    bool mqttPublishJSON;         // Publish all data as JSON to {prefix}/data
    
    // Time/NTP settings
    char ntpServer[64];
    int gmtOffsetSec;
    int daylightOffsetSec;
    
    // Tank dimensions (for volume calculator)
    float tankLength;   // cm
    float tankWidth;    // cm
    float tankHeight;   // cm
    
    // Temperature control
    float tempTarget;
    float tempSafetyMax;
    
    // CO2/pH control
    float phTarget;
    float phSafetyMin;
    
    // Pin assignments
    uint8_t tempSensorPin;
    uint8_t ambientSensorPin;
    uint8_t phSensorPin;
    uint8_t tdsSensorPin;
    uint8_t heaterRelayPin;
    uint8_t co2RelayPin;
    
    // Default constructor
    SystemConfig() {
        strcpy(wifiSSID, "");
        strcpy(wifiPassword, "");
        strcpy(mqttServer, "");
        mqttPort = 1883;
        strcpy(mqttUser, "");
        strcpy(mqttPassword, "");
        // Generate unique MQTT Client ID using ESP32 hardware RNG
        // Format: aquarium-XXXXXX (based on MAC address + random bytes)
        ESP32_Random::generateDeviceID(mqttClientId, sizeof(mqttClientId));
        strcpy(mqttTopicPrefix, "aquarium");
        mqttPublishIndividual = true;   // Individual topics enabled by default
        mqttPublishJSON = false;         // JSON disabled by default (opt-in)
        
        strcpy(ntpServer, "au.pool.ntp.org");  // Australian NTP servers
        gmtOffsetSec = 36000;      // AEST (UTC+10) - Brisbane, Queensland
        daylightOffsetSec = 0;     // Queensland does not observe Daylight Saving Time
        
        tankLength = 0;            // No default tank dimensions
        tankWidth = 0;
        tankHeight = 0;
        
        tempTarget = 25.0;         // 25°C - typical for tropical aquariums
        tempSafetyMax = 30.0;      // 30°C - safety limit for Australian climate
        phTarget = 6.8;            // Slightly acidic for planted tanks
        phSafetyMin = 6.0;         // Minimum safe pH
        
        tempSensorPin = 4;
        ambientSensorPin = 5;
        phSensorPin = 34;
        tdsSensorPin = 35;
        heaterRelayPin = 26;
        co2RelayPin = 27;
    }
};

class ConfigManager {
private:
    Preferences* prefs;
    SystemConfig config;
    
    // Deferred saving (dirty flag pattern)
    bool isDirty;
    unsigned long lastSaveTime;
    static const unsigned long SAVE_DELAY_MS = 5000; // 5 seconds

public:
    ConfigManager();
    ~ConfigManager();
    
    bool begin();
    void load();
    void save();
    void reset();
    
    // Deferred save management
    void markDirty();
    void update();      // Call this in main loop
    void forceSave();   // Immediate save for critical operations
    
    SystemConfig& getConfig() { return config; }
    void setConfig(const SystemConfig& newConfig);
    
    // Individual setters (now use deferred save)
    void setWiFi(const char* ssid, const char* password);
    void setMQTT(const char* server, int port, const char* user, const char* password, const char* clientId = nullptr, const char* topicPrefix = nullptr, bool publishIndividual = true, bool publishJSON = false);
    void setNTP(const char* server, int gmtOffset, int dstOffset);
    void setTemperatureTarget(float target, float safetyMax);
    void setPHTarget(float target, float safetyMin);
    void setTankDimensions(float length, float width, float height);
    
    // Backup and restore
    String exportToJSON();
    bool importFromJSON(const String& json);
    
    void printConfig();
};

#endif
