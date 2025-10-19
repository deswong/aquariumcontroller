#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
// #include <AsyncElegantOTA.h>  // Temporarily disabled
#include <ESPAsyncWebServer.h>

class OTAManager {
private:
    bool initialized;
    
public:
    OTAManager();
    
    void begin(AsyncWebServer* server);
    void update();
};

#endif
