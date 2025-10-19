#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "SystemTasks.h"
#include "ConfigManager.h"
#include "PHSensor.h"
#include "WiFiManager.h"

class WebServerManager {
private:
    AsyncWebSocket* ws;
    ConfigManager* config;
    unsigned long lastBroadcast;
    static const unsigned long BROADCAST_INTERVAL = 1000; // 1 second
    
    void setupRoutes();
    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
    void broadcastSensorData();

public:
    AsyncWebServer* server; // Public for OTA access
    
    WebServerManager(ConfigManager* configMgr);
    ~WebServerManager();
    
    void begin();
    void update();
};

#endif
