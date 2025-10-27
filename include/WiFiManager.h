#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "ConfigManager.h"

class WiFiManager {
private:
    ConfigManager* config;
    DNSServer* dnsServer;
    bool apMode;
    bool connected;
    bool timeSynced;
    unsigned long lastReconnectAttempt;
    unsigned long lastConnectionCheck;
    unsigned long lastNTPSync;
    int reconnectAttempts;
    unsigned long reconnectInterval;
    
    // Reconnection parameters
    static const unsigned long MIN_RECONNECT_INTERVAL = 5000;   // 5 seconds
    static const unsigned long MAX_RECONNECT_INTERVAL = 300000; // 5 minutes
    static const int MAX_RECONNECT_ATTEMPTS = 10;
    static const unsigned long CONNECTION_CHECK_INTERVAL = 5000; // 5 seconds
    // Note: SNTP handles automatic hourly sync internally (configured in syncTime())
    
    void startAccessPoint();
    void connectToWiFi();
    void resetReconnectBackoff();
    unsigned long getNextReconnectInterval();

public:
    WiFiManager(ConfigManager* configMgr);
    ~WiFiManager();
    
    void begin();
    void update();
    void syncTime();
    bool isConnected();
    bool isTimeSynced();
    bool isAPMode();
    String getIPAddress();
    int getReconnectAttempts();
    int getSignalStrength(); // RSSI in dBm
    String getLocalTime();
    String getTimeOnly(); // Get just HH:MM:SS for OLED display
};

#endif
