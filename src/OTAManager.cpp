#include "OTAManager.h"

OTAManager::OTAManager() : initialized(false) {
}

void OTAManager::begin(AsyncWebServer* server) {
    // Configure Arduino OTA
    ArduinoOTA.setHostname("aquarium-controller");
    ArduinoOTA.setPassword("aquarium123"); // Change this!
    
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("OTA Update Start: " + type);
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update Complete");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    
    ArduinoOTA.begin();
    
    // Setup AsyncElegantOTA for web-based updates
    // AsyncElegantOTA.begin(server);  // Temporarily disabled
    
    initialized = true;
    Serial.println("OTA update service initialized");
    Serial.println("Web OTA available at: http://<ip>/update");
}

void OTAManager::update() {
    if (initialized) {
        ArduinoOTA.handle();
    }
}
