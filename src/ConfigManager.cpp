#include "ConfigManager.h"
#include <ArduinoJson.h>

ConfigManager::ConfigManager() {
    prefs = new Preferences();
}

ConfigManager::~ConfigManager() {
    if (prefs) {
        prefs->end();
        delete prefs;
    }
}

bool ConfigManager::begin() {
    load();
    Serial.println("ConfigManager initialized");
    return true;
}

void ConfigManager::load() {
    if (!prefs->begin("system-config", true)) {
        Serial.println("No saved configuration, using defaults");
        return;
    }
    
    prefs->getString("wifiSSID", config.wifiSSID, sizeof(config.wifiSSID));
    prefs->getString("wifiPass", config.wifiPassword, sizeof(config.wifiPassword));
    
    prefs->getString("mqttServer", config.mqttServer, sizeof(config.mqttServer));
    config.mqttPort = prefs->getInt("mqttPort", 1883);
    prefs->getString("mqttUser", config.mqttUser, sizeof(config.mqttUser));
    prefs->getString("mqttPass", config.mqttPassword, sizeof(config.mqttPassword));
    prefs->getString("mqttClient", config.mqttClientId, sizeof(config.mqttClientId));
    prefs->getString("mqttPrefix", config.mqttTopicPrefix, sizeof(config.mqttTopicPrefix));
    config.mqttPublishIndividual = prefs->getBool("mqttPubInd", true);
    config.mqttPublishJSON = prefs->getBool("mqttPubJSON", false);
    
    prefs->getString("ntpServer", config.ntpServer, sizeof(config.ntpServer));
    config.gmtOffsetSec = prefs->getInt("gmtOffset", 0);
    config.daylightOffsetSec = prefs->getInt("dstOffset", 0);
    
    config.tempTarget = prefs->getFloat("tempTarget", 25.0);
    config.tempSafetyMax = prefs->getFloat("tempSafetyMax", 30.0);
    config.phTarget = prefs->getFloat("phTarget", 6.8);
    config.phSafetyMin = prefs->getFloat("phSafetyMin", 6.0);
    
    config.tempSensorPin = prefs->getUChar("tempPin", 4);
    config.phSensorPin = prefs->getUChar("phPin", 34);
    config.tdsSensorPin = prefs->getUChar("tdsPin", 35);
    config.heaterRelayPin = prefs->getUChar("heaterPin", 26);
    config.co2RelayPin = prefs->getUChar("co2Pin", 27);
    
    prefs->end();
    Serial.println("Configuration loaded from NVS");
}

void ConfigManager::save() {
    if (!prefs->begin("system-config", false)) {
        Serial.println("ERROR: Failed to save configuration");
        return;
    }
    
    prefs->putString("wifiSSID", config.wifiSSID);
    prefs->putString("wifiPass", config.wifiPassword);
    
    prefs->putString("mqttServer", config.mqttServer);
    prefs->putInt("mqttPort", config.mqttPort);
    prefs->putString("mqttUser", config.mqttUser);
    prefs->putString("mqttPass", config.mqttPassword);
    prefs->putString("mqttClient", config.mqttClientId);
    prefs->putString("mqttPrefix", config.mqttTopicPrefix);
    prefs->putBool("mqttPubInd", config.mqttPublishIndividual);
    prefs->putBool("mqttPubJSON", config.mqttPublishJSON);
    
    prefs->putString("ntpServer", config.ntpServer);
    prefs->putInt("gmtOffset", config.gmtOffsetSec);
    prefs->putInt("dstOffset", config.daylightOffsetSec);
    
    prefs->putFloat("tempTarget", config.tempTarget);
    prefs->putFloat("tempSafetyMax", config.tempSafetyMax);
    prefs->putFloat("phTarget", config.phTarget);
    prefs->putFloat("phSafetyMin", config.phSafetyMin);
    
    prefs->putUChar("tempPin", config.tempSensorPin);
    prefs->putUChar("phPin", config.phSensorPin);
    prefs->putUChar("tdsPin", config.tdsSensorPin);
    prefs->putUChar("heaterPin", config.heaterRelayPin);
    prefs->putUChar("co2Pin", config.co2RelayPin);
    
    prefs->end();
    Serial.println("Configuration saved to NVS");
}

void ConfigManager::reset() {
    config = SystemConfig(); // Reset to defaults
    save();
    Serial.println("Configuration reset to defaults");
}

void ConfigManager::setConfig(const SystemConfig& newConfig) {
    config = newConfig;
    save();
}

void ConfigManager::setWiFi(const char* ssid, const char* password) {
    strncpy(config.wifiSSID, ssid, sizeof(config.wifiSSID) - 1);
    strncpy(config.wifiPassword, password, sizeof(config.wifiPassword) - 1);
    save();
}

void ConfigManager::setMQTT(const char* server, int port, const char* user, const char* password, const char* topicPrefix, bool publishIndividual, bool publishJSON) {
    strncpy(config.mqttServer, server, sizeof(config.mqttServer) - 1);
    config.mqttPort = port;
    strncpy(config.mqttUser, user, sizeof(config.mqttUser) - 1);
    strncpy(config.mqttPassword, password, sizeof(config.mqttPassword) - 1);
    if (topicPrefix != nullptr && strlen(topicPrefix) > 0) {
        strncpy(config.mqttTopicPrefix, topicPrefix, sizeof(config.mqttTopicPrefix) - 1);
    }
    config.mqttPublishIndividual = publishIndividual;
    config.mqttPublishJSON = publishJSON;
    save();
}

void ConfigManager::setTemperatureTarget(float target, float safetyMax) {
    config.tempTarget = target;
    config.tempSafetyMax = safetyMax;
    save();
}

void ConfigManager::setPHTarget(float target, float safetyMin) {
    config.phTarget = target;
    config.phSafetyMin = safetyMin;
    save();
}

void ConfigManager::setNTP(const char* server, int gmtOffset, int dstOffset) {
    strncpy(config.ntpServer, server, sizeof(config.ntpServer) - 1);
    config.gmtOffsetSec = gmtOffset;
    config.daylightOffsetSec = dstOffset;
    save();
}

void ConfigManager::printConfig() {
    Serial.println("=== System Configuration ===");
    Serial.printf("WiFi SSID: %s\n", config.wifiSSID);
    Serial.printf("MQTT Server: %s:%d\n", config.mqttServer, config.mqttPort);
    Serial.printf("MQTT Client: %s\n", config.mqttClientId);
    Serial.printf("MQTT Topic Prefix: %s\n", config.mqttTopicPrefix);
    Serial.printf("NTP Server: %s (GMT%+d, DST%+d)\n", config.ntpServer, 
                  config.gmtOffsetSec / 3600, config.daylightOffsetSec / 3600);
    Serial.printf("Temperature Target: %.1f°C (Max: %.1f°C)\n", config.tempTarget, config.tempSafetyMax);
    Serial.printf("pH Target: %.1f (Min: %.1f)\n", config.phTarget, config.phSafetyMin);
    Serial.printf("Pins - Temp:%d Ambient:%d pH:%d TDS:%d Heater:%d CO2:%d\n",
                  config.tempSensorPin, config.ambientSensorPin, config.phSensorPin, config.tdsSensorPin,
                  config.heaterRelayPin, config.co2RelayPin);
    Serial.println("===========================");
}

String ConfigManager::exportToJSON() {
    StaticJsonDocument<1024> doc;
    
    doc["wifiSSID"] = config.wifiSSID;
    doc["wifiPassword"] = config.wifiPassword;
    doc["mqttServer"] = config.mqttServer;
    doc["mqttPort"] = config.mqttPort;
    doc["mqttUser"] = config.mqttUser;
    doc["mqttPassword"] = config.mqttPassword;
    doc["mqttClientId"] = config.mqttClientId;
    doc["mqttTopicPrefix"] = config.mqttTopicPrefix;
    doc["tempTarget"] = config.tempTarget;
    doc["tempSafetyMax"] = config.tempSafetyMax;
    doc["phTarget"] = config.phTarget;
    doc["phSafetyMin"] = config.phSafetyMin;
    doc["tempSensorPin"] = config.tempSensorPin;
    doc["ambientSensorPin"] = config.ambientSensorPin;
    doc["phSensorPin"] = config.phSensorPin;
    doc["tdsSensorPin"] = config.tdsSensorPin;
    doc["heaterRelayPin"] = config.heaterRelayPin;
    doc["co2RelayPin"] = config.co2RelayPin;
    
    String json;
    serializeJsonPretty(doc, json);
    return json;
}

bool ConfigManager::importFromJSON(const String& json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.printf("ERROR: Failed to parse configuration JSON: %s\n", error.c_str());
        return false;
    }
    
    // Import all fields
    if (doc.containsKey("wifiSSID")) 
        strncpy(config.wifiSSID, doc["wifiSSID"], sizeof(config.wifiSSID) - 1);
    if (doc.containsKey("wifiPassword")) 
        strncpy(config.wifiPassword, doc["wifiPassword"], sizeof(config.wifiPassword) - 1);
    if (doc.containsKey("mqttServer")) 
        strncpy(config.mqttServer, doc["mqttServer"], sizeof(config.mqttServer) - 1);
    if (doc.containsKey("mqttPort")) 
        config.mqttPort = doc["mqttPort"];
    if (doc.containsKey("mqttUser")) 
        strncpy(config.mqttUser, doc["mqttUser"], sizeof(config.mqttUser) - 1);
    if (doc.containsKey("mqttPassword")) 
        strncpy(config.mqttPassword, doc["mqttPassword"], sizeof(config.mqttPassword) - 1);
    if (doc.containsKey("mqttClientId")) 
        strncpy(config.mqttClientId, doc["mqttClientId"], sizeof(config.mqttClientId) - 1);
    if (doc.containsKey("mqttTopicPrefix")) 
        strncpy(config.mqttTopicPrefix, doc["mqttTopicPrefix"], sizeof(config.mqttTopicPrefix) - 1);
    if (doc.containsKey("tempTarget")) 
        config.tempTarget = doc["tempTarget"];
    if (doc.containsKey("tempSafetyMax")) 
        config.tempSafetyMax = doc["tempSafetyMax"];
    if (doc.containsKey("phTarget")) 
        config.phTarget = doc["phTarget"];
    if (doc.containsKey("phSafetyMin")) 
        config.phSafetyMin = doc["phSafetyMin"];
    if (doc.containsKey("tempSensorPin")) 
        config.tempSensorPin = doc["tempSensorPin"];
    if (doc.containsKey("ambientSensorPin")) 
        config.ambientSensorPin = doc["ambientSensorPin"];
    if (doc.containsKey("phSensorPin")) 
        config.phSensorPin = doc["phSensorPin"];
    if (doc.containsKey("tdsSensorPin")) 
        config.tdsSensorPin = doc["tdsSensorPin"];
    if (doc.containsKey("heaterRelayPin")) 
        config.heaterRelayPin = doc["heaterRelayPin"];
    if (doc.containsKey("co2RelayPin")) 
        config.co2RelayPin = doc["co2RelayPin"];
    
    save();
    Serial.println("Configuration imported successfully");
    return true;
}
