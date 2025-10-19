#include "WebServer.h"
#include "SystemTasks.h"

WebServerManager::WebServerManager(ConfigManager* configMgr) 
    : config(configMgr), lastBroadcast(0) {
    server = new AsyncWebServer(80);
    ws = new AsyncWebSocket("/ws");
}

WebServerManager::~WebServerManager() {
    delete ws;
    delete server;
}

void WebServerManager::begin() {
    // Initialize SPIFFS for serving web files
    if (!SPIFFS.begin(true)) {
        Serial.println("ERROR: Failed to mount SPIFFS");
        return;
    }
    
    // Setup WebSocket
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("WebSocket client #%u connected\n", client->id());
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
        } else if (type == WS_EVT_DATA) {
            handleWebSocketMessage(arg, data, len);
        }
    });
    
    server->addHandler(ws);
    
    setupRoutes();
    
    server->begin();
    Serial.println("Web server started");
}

void WebServerManager::setupRoutes() {
    // Serve index.html from SPIFFS
    server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });
    
    // API: Get current sensor data
    server->on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
        SensorData data = getSensorData();
        
        StaticJsonDocument<1024> doc;
        doc["temperature"] = data.temperature;
        doc["ambientTemp"] = data.ambientTemp;
        doc["ph"] = data.ph;
        doc["tds"] = data.tds;
        doc["heaterState"] = data.heaterState;
        doc["co2State"] = data.co2State;
        doc["tempEmergency"] = data.tempEmergencyStop;
        doc["co2Emergency"] = data.co2EmergencyStop;
        doc["tempPIDOutput"] = data.tempPIDOutput;
        doc["co2PIDOutput"] = data.co2PIDOutput;
        
        // Add PID performance metrics
        if (tempPID) {
            JsonObject tempMetrics = doc.createNestedObject("tempPIDMetrics");
            tempMetrics["settlingTime"] = tempPID->getSettlingTime();
            tempMetrics["maxOvershoot"] = tempPID->getMaxOvershoot();
            tempMetrics["steadyStateError"] = tempPID->getSteadyStateError();
            tempMetrics["settled"] = tempPID->isSystemSettled();
            tempMetrics["effectiveTarget"] = tempPID->getEffectiveTarget();
            tempMetrics["integral"] = tempPID->getIntegral();
        }
        
        if (co2PID) {
            JsonObject co2Metrics = doc.createNestedObject("co2PIDMetrics");
            co2Metrics["settlingTime"] = co2PID->getSettlingTime();
            co2Metrics["maxOvershoot"] = co2PID->getMaxOvershoot();
            co2Metrics["steadyStateError"] = co2PID->getSteadyStateError();
            co2Metrics["settled"] = co2PID->isSystemSettled();
            co2Metrics["effectiveTarget"] = co2PID->getEffectiveTarget();
            co2Metrics["integral"] = co2PID->getIntegral();
        }
        
        // Add WiFi metrics
        if (wifiMgr) {
            JsonObject wifiMetrics = doc.createNestedObject("wifi");
            wifiMetrics["connected"] = wifiMgr->isConnected();
            wifiMetrics["rssi"] = wifiMgr->getSignalStrength();
            wifiMetrics["reconnectAttempts"] = wifiMgr->getReconnectAttempts();
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Update targets
    server->on("/api/targets", HTTP_POST, [this](AsyncWebServerRequest* request) {}, 
               NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error) {
            if (doc.containsKey("temperature")) {
                float tempTarget = doc["temperature"];
                float tempSafetyMax = doc.containsKey("tempSafetyMax") ? 
                                     doc["tempSafetyMax"].as<float>() : config->getConfig().tempSafetyMax;
                tempPID->setTarget(tempTarget);
                config->setTemperatureTarget(tempTarget, tempSafetyMax);
            }
            if (doc.containsKey("ph")) {
                float phTarget = doc["ph"];
                float phSafetyMin = doc.containsKey("phSafetyMin") ? 
                                   doc["phSafetyMin"].as<float>() : config->getConfig().phSafetyMin;
                co2PID->setTarget(phTarget);
                config->setPHTarget(phTarget, phSafetyMin);
            }
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        }
    });
    
    // API: Emergency stop
    server->on("/api/emergency", HTTP_POST, [](AsyncWebServerRequest* request) {
        heaterRelay->safetyDisable();
        co2Relay->safetyDisable();
        Serial.println("EMERGENCY STOP triggered via web interface");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Clear emergency stop
    server->on("/api/emergency/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
        tempPID->clearEmergencyStop();
        co2PID->clearEmergencyStop();
        heaterRelay->safetyEnable();
        co2Relay->safetyEnable();
        Serial.println("Emergency stop cleared via web interface");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Get system logs
    server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
        int count = 200; // Default
        if (request->hasParam("count")) {
            count = request->getParam("count")->value().toInt();
            if (count > 500) count = 500; // Max limit
        }
        
        if (!eventLogger) {
            request->send(500, "application/json", "{\"error\":\"Event logger not initialized\"}");
            return;
        }
        
        std::vector<LogEvent> logs = eventLogger->getRecentLogs(count);
        
        String json = "{\"logs\":[";
        for (size_t i = 0; i < logs.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"timestamp\":" + String(logs[i].timestamp / 1000) + ",";
            json += "\"level\":\"" + String(logs[i].level == EVENT_INFO ? "INFO" : 
                                           logs[i].level == EVENT_WARNING ? "WARNING" : 
                                           logs[i].level == EVENT_ERROR ? "ERROR" : "CRITICAL") + "\",";
            json += "\"category\":\"" + String(logs[i].category) + "\",";
            json += "\"message\":\"" + String(logs[i].message) + "\"";
            json += "}";
        }
        json += "],\"count\":" + String(logs.size()) + "}";
        
        request->send(200, "application/json", json);
    });
    
    // API: pH Calibration
    server->on("/api/calibrate/start", HTTP_POST, [](AsyncWebServerRequest* request) {
        phSensor->startCalibration();
        Serial.println("Web: pH calibration started (using ambient temperature)");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server->on("/api/calibrate/end", HTTP_POST, [](AsyncWebServerRequest* request) {
        phSensor->endCalibration();
        Serial.println("Web: pH calibration ended (switched to water temperature)");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server->on("/api/calibrate/point", HTTP_POST, [](AsyncWebServerRequest* request) {}, 
               NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error && doc.containsKey("ph") && doc.containsKey("temp")) {
            float ph = doc["ph"];
            float temp = doc["temp"];
            float refPH = doc.containsKey("refPH") ? doc["refPH"].as<float>() : 0;
            
            bool success = phSensor->calibratePoint(ph, temp, refPH);
            if (success) {
                Serial.printf("Web: Calibrated pH %.2f at %.1f°C (ref pH: %.2f)\n", 
                             ph, temp, refPH > 0 ? refPH : ph);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Calibration failed\"}");
            }
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing required fields: ph, temp\"}");
        }
    });
    
    server->on("/api/calibrate/save", HTTP_POST, [](AsyncWebServerRequest* request) {
        phSensor->saveCalibration();
        Serial.println("Web: pH calibration saved to NVS");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server->on("/api/calibrate/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        phSensor->resetCalibration();
        Serial.println("Web: pH calibration reset");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Get current pH voltage (for calibration feedback)
    server->on("/api/calibrate/voltage", HTTP_GET, [](AsyncWebServerRequest* request) {
        // Get current sensor data
        SensorData data = getSensorData();
        
        StaticJsonDocument<256> doc;
        doc["ambientTemp"] = data.ambientTemp;
        doc["waterTemp"] = data.temperature;
        doc["currentPH"] = data.ph;
        doc["calibrating"] = phSensor->inCalibrationMode();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Get settings
    server->on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        SystemConfig& cfg = config->getConfig();
        
        StaticJsonDocument<1536> doc;
        doc["wifiSSID"] = cfg.wifiSSID;
        doc["mqttServer"] = cfg.mqttServer;
        doc["mqttPort"] = cfg.mqttPort;
        doc["mqttUser"] = cfg.mqttUser;
        doc["mqttTopicPrefix"] = cfg.mqttTopicPrefix;
        doc["mqttPublishIndividual"] = cfg.mqttPublishIndividual;
        doc["mqttPublishJSON"] = cfg.mqttPublishJSON;
        doc["ntpServer"] = cfg.ntpServer;
        doc["gmtOffsetSec"] = cfg.gmtOffsetSec;
        doc["daylightOffsetSec"] = cfg.daylightOffsetSec;
        doc["tempTarget"] = cfg.tempTarget;
        doc["tempSafetyMax"] = cfg.tempSafetyMax;
        doc["phTarget"] = cfg.phTarget;
        doc["phSafetyMin"] = cfg.phSafetyMin;
        doc["tempSensorPin"] = cfg.tempSensorPin;
        doc["ambientSensorPin"] = cfg.ambientSensorPin;
        doc["phSensorPin"] = cfg.phSensorPin;
        doc["tdsSensorPin"] = cfg.tdsSensorPin;
        doc["heaterRelayPin"] = cfg.heaterRelayPin;
        doc["co2RelayPin"] = cfg.co2RelayPin;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Save settings
    server->on("/api/settings", HTTP_POST, [this](AsyncWebServerRequest* request) {}, 
               NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<1536> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error) {
            // Get current config
            SystemConfig& cfg = config->getConfig();
            
            // Update WiFi settings
            if (doc.containsKey("wifiSSID") && doc.containsKey("wifiPassword")) {
                config->setWiFi(doc["wifiSSID"], doc["wifiPassword"]);
            }
            
            // Update MQTT settings
            if (doc.containsKey("mqttServer")) {
                const char* topicPrefix = doc.containsKey("mqttTopicPrefix") ? 
                                         doc["mqttTopicPrefix"].as<const char*>() : nullptr;
                bool publishIndividual = doc.containsKey("mqttPublishIndividual") ? 
                                        doc["mqttPublishIndividual"].as<bool>() : true;
                bool publishJSON = doc.containsKey("mqttPublishJSON") ? 
                                  doc["mqttPublishJSON"].as<bool>() : false;
                config->setMQTT(doc["mqttServer"], 
                              doc["mqttPort"] | 1883,
                              doc["mqttUser"] | "",
                              doc["mqttPassword"] | "",
                              topicPrefix,
                              publishIndividual,
                              publishJSON);
            }
            
            // Update NTP settings
            if (doc.containsKey("ntpServer")) {
                strncpy(cfg.ntpServer, doc["ntpServer"].as<const char*>(), sizeof(cfg.ntpServer) - 1);
            }
            if (doc.containsKey("gmtOffsetSec")) {
                cfg.gmtOffsetSec = doc["gmtOffsetSec"].as<int>();
            }
            if (doc.containsKey("daylightOffsetSec")) {
                cfg.daylightOffsetSec = doc["daylightOffsetSec"].as<int>();
            }
            
            // Update control settings
            if (doc.containsKey("tempTarget")) {
                cfg.tempTarget = doc["tempTarget"].as<float>();
            }
            if (doc.containsKey("tempSafetyMax")) {
                cfg.tempSafetyMax = doc["tempSafetyMax"].as<float>();
            }
            if (doc.containsKey("phTarget")) {
                cfg.phTarget = doc["phTarget"].as<float>();
            }
            if (doc.containsKey("phSafetyMin")) {
                cfg.phSafetyMin = doc["phSafetyMin"].as<float>();
            }
            
            // Update pin assignments
            if (doc.containsKey("tempSensorPin")) {
                cfg.tempSensorPin = doc["tempSensorPin"].as<uint8_t>();
            }
            if (doc.containsKey("ambientSensorPin")) {
                cfg.ambientSensorPin = doc["ambientSensorPin"].as<uint8_t>();
            }
            if (doc.containsKey("phSensorPin")) {
                cfg.phSensorPin = doc["phSensorPin"].as<uint8_t>();
            }
            if (doc.containsKey("tdsSensorPin")) {
                cfg.tdsSensorPin = doc["tdsSensorPin"].as<uint8_t>();
            }
            if (doc.containsKey("heaterRelayPin")) {
                cfg.heaterRelayPin = doc["heaterRelayPin"].as<uint8_t>();
            }
            if (doc.containsKey("co2RelayPin")) {
                cfg.co2RelayPin = doc["co2RelayPin"].as<uint8_t>();
            }
            
            // Save all changes
            config->save();
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
            
            // Publish offline status before restart
            if (mqttClient && mqttClient->connected()) {
                char topic[128];
                snprintf(topic, sizeof(topic), "%s/status", cfg.mqttTopicPrefix);
                mqttClient->publish(topic, "offline", true);
                delay(100);  // Give time for message to send
            }
            
            // Restart after settings change
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "application/json", "{\"status\":\"error\"}");
        }
    });
    
    // API: Get event logs
    server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!eventLogger) {
            request->send(500, "application/json", "{\"error\":\"Event logger not initialized\"}");
            return;
        }
        
        int count = 50; // Default to 50 most recent
        if (request->hasParam("count")) {
            count = request->getParam("count")->value().toInt();
        }
        
        std::vector<LogEvent> logs = eventLogger->getRecentLogs(count);
        
        String json = "[";
        for (size_t i = 0; i < logs.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"timestamp\":" + String(logs[i].timestamp) + ",";
            json += "\"level\":" + String(logs[i].level) + ",";
            json += "\"category\":\"" + String(logs[i].category) + "\",";
            json += "\"message\":\"" + String(logs[i].message) + "\"";
            json += "}";
        }
        json += "]";
        
        request->send(200, "application/json", json);
    });
    
    // API: Clear event logs
    server->on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!eventLogger) {
            request->send(500, "application/json", "{\"error\":\"Event logger not initialized\"}");
            return;
        }
        
        eventLogger->clearLogs();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Export configuration
    server->on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!configMgr) {
            request->send(500, "application/json", "{\"error\":\"Config manager not initialized\"}");
            return;
        }
        
        String json = configMgr->exportToJSON();
        request->send(200, "application/json", json);
    });
    
    // API: Import configuration
    server->on("/api/config/import", HTTP_POST, [this](AsyncWebServerRequest* request) {}, 
        NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!configMgr) {
            request->send(500, "application/json", "{\"error\":\"Config manager not initialized\"}");
            return;
        }
        
        String json = "";
        for (size_t i = 0; i < len; i++) {
            json += (char)data[i];
        }
        
        if (configMgr->importFromJSON(json)) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
            if (eventLogger) {
                eventLogger->info("config", "Configuration imported from JSON");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid configuration JSON\"}");
        }
    });
    
    // API: Water Change - Get Status
    server->on("/api/waterchange/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        StaticJsonDocument<512> doc;
        doc["inProgress"] = waterChangeAssistant->isInProgress();
        doc["phase"] = waterChangeAssistant->getCurrentPhase();
        doc["phaseDescription"] = waterChangeAssistant->getPhaseDescription();
        doc["phaseElapsed"] = waterChangeAssistant->getPhaseElapsedTime();
        doc["systemsPaused"] = waterChangeAssistant->areSystemsPaused();
        doc["currentVolume"] = waterChangeAssistant->getCurrentChangeVolume();
        doc["tankVolume"] = waterChangeAssistant->getTankVolume();
        doc["schedule"] = (int)waterChangeAssistant->getSchedule();
        doc["daysSinceLastChange"] = waterChangeAssistant->getDaysSinceLastChange();
        doc["daysUntilNextChange"] = waterChangeAssistant->getDaysUntilNextChange();
        doc["isOverdue"] = waterChangeAssistant->isChangeOverdue();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Water Change - Start
    server->on("/api/waterchange/start", HTTP_POST, [](AsyncWebServerRequest* request) {}, 
        NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        float volume = 0; // Use scheduled volume
        if (len > 0) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            if (!error && doc.containsKey("volume")) {
                volume = doc["volume"];
            }
        }
        
        if (waterChangeAssistant->startWaterChange(volume)) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Failed to start water change\"}");
        }
    });
    
    // API: Water Change - Advance Phase
    server->on("/api/waterchange/advance", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        if (waterChangeAssistant->advancePhase()) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Failed to advance phase\"}");
        }
    });
    
    // API: Water Change - Cancel
    server->on("/api/waterchange/cancel", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        if (waterChangeAssistant->cancelWaterChange()) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"No water change in progress\"}");
        }
    });
    
    // API: Water Change - Get History
    server->on("/api/waterchange/history", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        int count = 10; // Default
        if (request->hasParam("count")) {
            count = request->getParam("count")->value().toInt();
        }
        
        auto records = waterChangeAssistant->getRecentHistory(count);
        
        String json = "[";
        for (size_t i = 0; i < records.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"timestamp\":" + String(records[i].timestamp) + ",";
            json += "\"volume\":" + String(records[i].volumeChanged) + ",";
            json += "\"tempBefore\":" + String(records[i].tempBefore) + ",";
            json += "\"tempAfter\":" + String(records[i].tempAfter) + ",";
            json += "\"phBefore\":" + String(records[i].phBefore) + ",";
            json += "\"phAfter\":" + String(records[i].phAfter) + ",";
            json += "\"duration\":" + String(records[i].durationMinutes) + ",";
            json += "\"successful\":" + String(records[i].completedSuccessfully ? "true" : "false");
            json += "}";
        }
        json += "]";
        
        request->send(200, "application/json", json);
    });
    
    // API: Water Change - Set Schedule
    server->on("/api/waterchange/schedule", HTTP_POST, [](AsyncWebServerRequest* request) {}, 
        NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (error) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        if (doc.containsKey("schedule") && doc.containsKey("volumePercent")) {
            int schedule = doc["schedule"];
            float volumePercent = doc["volumePercent"];
            waterChangeAssistant->setSchedule((WaterChangeSchedule)schedule, volumePercent);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing required fields\"}");
        }
    });
    
    // API: Water Change - Statistics
    server->on("/api/waterchange/stats", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"Water change assistant not initialized\"}");
            return;
        }
        
        StaticJsonDocument<256> doc;
        doc["totalChanges"] = waterChangeAssistant->getHistoryCount();
        doc["changesThisMonth"] = waterChangeAssistant->getTotalChangesThisMonth();
        doc["volumeThisMonth"] = waterChangeAssistant->getTotalVolumeChangedThisMonth();
        doc["averageVolume"] = waterChangeAssistant->getAverageChangeVolume();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // ====================
    // Pattern Learning API
    // ====================
    
    // API: Pattern Learning - Status
    server->on("/api/pattern/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        String response = patternLearner->getStatusJSON();
        request->send(200, "application/json", response);
    });
    
    // API: Pattern Learning - Hourly Patterns
    server->on("/api/pattern/hourly", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        StaticJsonDocument<2048> doc;
        JsonArray hours = doc.createNestedArray("hours");
        
        for (int i = 0; i < 24; i++) {
            JsonObject hour = hours.createNestedObject();
            hour["hour"] = i;
            hour["temp"] = patternLearner->getExpectedTemp(i);
            hour["tempStdDev"] = patternLearner->getTempStdDev(i);
            hour["ph"] = patternLearner->getExpectedPH(i);
            hour["phStdDev"] = patternLearner->getPHStdDev(i);
            hour["tds"] = patternLearner->getExpectedTDS(i);
            hour["tdsStdDev"] = patternLearner->getTDSStdDev(i);
            hour["samples"] = patternLearner->getSampleCount(i);
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Pattern Learning - Recent Anomalies
    server->on("/api/pattern/anomalies", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        int count = 20;
        if (request->hasParam("count")) {
            count = request->getParam("count")->value().toInt();
        }
        
        std::vector<Anomaly> anomalies = patternLearner->getRecentAnomalies(count);
        
        DynamicJsonDocument doc(4096);
        JsonArray array = doc.to<JsonArray>();
        
        for (const auto& a : anomalies) {
            JsonObject obj = array.createNestedObject();
            obj["timestamp"] = a.timestamp;
            obj["type"] = a.type;
            obj["actual"] = a.actualValue;
            obj["expected"] = a.expectedValue;
            obj["deviation"] = a.deviation;
            obj["severity"] = a.severity;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Pattern Learning - Seasonal Stats
    server->on("/api/pattern/seasonal", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        SeasonalStats stats = patternLearner->getSeasonalStats();
        
        StaticJsonDocument<256> doc;
        doc["season"] = stats.season;
        doc["avgAmbient"] = stats.avgAmbientTemp;
        doc["avgWater"] = stats.avgWaterTemp;
        doc["avgPH"] = stats.avgPH;
        doc["avgTDS"] = stats.avgTDS;
        doc["daysCollected"] = stats.daysCollected;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Pattern Learning - Configuration
    server->on("/api/pattern/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        PatternConfig cfg = patternLearner->getConfig();
        
        StaticJsonDocument<256> doc;
        doc["enabled"] = cfg.enabled;
        doc["minSamples"] = cfg.minSamplesForAnomaly;
        doc["tempThreshold"] = cfg.tempAnomalyThreshold;
        doc["phThreshold"] = cfg.phAnomalyThreshold;
        doc["tdsThreshold"] = cfg.tdsAnomalyThreshold;
        doc["autoSeasonalAdapt"] = cfg.autoSeasonalAdapt;
        doc["alertOnAnomaly"] = cfg.alertOnAnomaly;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // API: Pattern Learning - Update Configuration
    server->on("/api/pattern/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!patternLearner) {
                request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
                return;
            }
            
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            PatternConfig cfg = patternLearner->getConfig();
            
            if (doc.containsKey("enabled")) cfg.enabled = doc["enabled"];
            if (doc.containsKey("tempThreshold")) cfg.tempAnomalyThreshold = doc["tempThreshold"];
            if (doc.containsKey("phThreshold")) cfg.phAnomalyThreshold = doc["phThreshold"];
            if (doc.containsKey("tdsThreshold")) cfg.tdsAnomalyThreshold = doc["tdsThreshold"];
            if (doc.containsKey("autoSeasonalAdapt")) cfg.autoSeasonalAdapt = doc["autoSeasonalAdapt"];
            if (doc.containsKey("alertOnAnomaly")) cfg.alertOnAnomaly = doc["alertOnAnomaly"];
            
            patternLearner->setConfig(cfg);
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Pattern Learning - Reset Patterns
    server->on("/api/pattern/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        patternLearner->reset();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Pattern Learning - Clear Anomaly History
    server->on("/api/pattern/anomalies/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!patternLearner) {
            request->send(500, "application/json", "{\"error\":\"Pattern learner not initialized\"}");
            return;
        }
        
        patternLearner->clearAnomalyHistory();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // ============================================================
    // DOSING PUMP API ENDPOINTS
    // ============================================================
    
    // API: Get dosing pump status
    server->on("/api/dosing/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        String stateStr;
        switch (dosingPump->getState()) {
            case PUMP_IDLE: stateStr = "idle"; break;
            case PUMP_DOSING: stateStr = "dosing"; break;
            case PUMP_PAUSED: stateStr = "paused"; break;
            case PUMP_PRIMING: stateStr = "priming"; break;
            case PUMP_BACKFLUSHING: stateStr = "backflushing"; break;
            case PUMP_CALIBRATING: stateStr = "calibrating"; break;
            case PUMP_ERROR: stateStr = "error"; break;
            default: stateStr = "unknown"; break;
        }
        
        float progress = dosingPump->getProgress();
        float hoursUntilNext = dosingPump->getHoursUntilNextDose();
        
        String json = "{";
        json += "\"state\":\"" + stateStr + "\",";
        json += "\"calibrated\":" + String(dosingPump->isCalibrated() ? "true" : "false") + ",";
        json += "\"flowRate\":" + String(dosingPump->getFlowRate(), 3) + ",";
        json += "\"currentSpeed\":" + String(dosingPump->getCurrentSpeed()) + ",";
        json += "\"targetVolume\":" + String(dosingPump->getTargetVolume(), 2) + ",";
        json += "\"volumePumped\":" + String(dosingPump->getVolumePumped(), 2) + ",";
        json += "\"progress\":" + String(progress, 1) + ",";
        json += "\"scheduleEnabled\":" + String(dosingPump->isScheduleEnabled() ? "true" : "false") + ",";
        json += "\"hoursUntilNext\":" + String(hoursUntilNext, 1) + ",";
        json += "\"dailyVolume\":" + String(dosingPump->getDailyVolumeDosed(), 2) + ",";
        json += "\"remainingDaily\":" + String(dosingPump->getRemainingDailyVolume(), 2) + ",";
        json += "\"safetyEnabled\":" + String(dosingPump->isSafetyEnabled() ? "true" : "false");
        json += "}";
        
        request->send(200, "application/json", json);
    });
    
    // API: Start manual dosing
    server->on("/api/dosing/start", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(256);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            float volume = doc["volume"] | 5.0;
            int speed = doc["speed"] | 100;
            
            if (!dosingPump->isCalibrated()) {
                request->send(400, "application/json", "{\"error\":\"Pump not calibrated\"}");
                return;
            }
            
            dosingPump->start(volume, speed);
            
            if (eventLogger) {
                eventLogger->info("dosing", "Manual dose started: " + String(volume, 1) + " mL");
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Stop dosing
    server->on("/api/dosing/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        dosingPump->stop();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Pause dosing
    server->on("/api/dosing/pause", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        dosingPump->pause();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Resume dosing
    server->on("/api/dosing/resume", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        dosingPump->resume();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Prime pump
    server->on("/api/dosing/prime", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(128);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            int duration = doc["duration"] | 10;
            int speed = doc["speed"] | 50;
            
            dosingPump->prime(duration, speed);
            
            if (eventLogger) {
                eventLogger->info("dosing", "Priming pump for " + String(duration) + " seconds");
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Backflush pump
    server->on("/api/dosing/backflush", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(128);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            int duration = doc["duration"] | 10;
            int speed = doc["speed"] | 30;
            
            dosingPump->backflush(duration, speed);
            
            if (eventLogger) {
                eventLogger->info("dosing", "Backflushing pump for " + String(duration) + " seconds");
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Clean pump
    server->on("/api/dosing/clean", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(128);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            int cycles = doc["cycles"] | 3;
            
            dosingPump->runCleaning(cycles);
            
            if (eventLogger) {
                eventLogger->info("dosing", "Running " + String(cycles) + " cleaning cycles");
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Start calibration
    server->on("/api/dosing/calibrate/start", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(128);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            int speed = doc["speed"] | 100;
            
            dosingPump->startCalibration(speed);
            
            if (eventLogger) {
                eventLogger->info("dosing", "Calibration started at " + String(speed) + "% speed");
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Finish calibration
    server->on("/api/dosing/calibrate/finish", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(128);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            float measuredML = doc["measuredML"] | 0.0;
            int seconds = doc["seconds"] | 0;
            
            if (measuredML <= 0 || seconds <= 0) {
                request->send(400, "application/json", "{\"error\":\"Invalid measurements\"}");
                return;
            }
            
            dosingPump->finishCalibration(measuredML, seconds);
            
            float flowRate = dosingPump->getFlowRate();
            
            if (eventLogger) {
                eventLogger->info("dosing", "Calibration complete: " + String(flowRate, 3) + " mL/sec");
            }
            
            String json = "{\"status\":\"ok\",\"flowRate\":" + String(flowRate, 3) + "}";
            request->send(200, "application/json", json);
        });
    
    // API: Cancel calibration
    server->on("/api/dosing/calibrate/cancel", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        dosingPump->cancelCalibration();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // API: Get dosing history
    server->on("/api/dosing/history", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        int count = 20;
        if (request->hasParam("count")) {
            count = request->getParam("count")->value().toInt();
            if (count < 1) count = 1;
            if (count > 100) count = 100;
        }
        
        std::vector<DosingRecord> history = dosingPump->getHistory(count);
        
        String json = "{\"history\":[";
        for (size_t i = 0; i < history.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"timestamp\":" + String(history[i].timestamp) + ",";
            json += "\"volumeDosed\":" + String(history[i].volumeDosed, 2) + ",";
            json += "\"duration\":" + String(history[i].durationMs / 1000.0, 1) + ",";
            json += "\"success\":" + String(history[i].success ? "true" : "false") + ",";
            json += "\"type\":\"" + String(history[i].type) + "\"";
            json += "}";
        }
        json += "]}";
        
        request->send(200, "application/json", json);
    });
    
    // API: Get dosing schedule
    server->on("/api/dosing/schedule", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        DosingScheduleConfig schedule = dosingPump->getScheduleConfig();
        
        String scheduleStr;
        switch (schedule.schedule) {
            case DOSE_MANUAL: scheduleStr = "manual"; break;
            case DOSE_DAILY: scheduleStr = "daily"; break;
            case DOSE_WEEKLY: scheduleStr = "weekly"; break;
            case DOSE_CUSTOM: scheduleStr = "custom"; break;
            default: scheduleStr = "manual"; break;
        }
        
        String json = "{";
        json += "\"enabled\":" + String(schedule.enabled ? "true" : "false") + ",";
        json += "\"schedule\":\"" + scheduleStr + "\",";
        json += "\"customDays\":" + String(schedule.customDays) + ",";
        json += "\"hour\":" + String(schedule.hour) + ",";
        json += "\"minute\":" + String(schedule.minute) + ",";
        json += "\"doseVolume\":" + String(schedule.doseVolume, 2) + ",";
        json += "\"lastDoseTime\":" + String(schedule.lastDoseTime) + ",";
        json += "\"nextDoseTime\":" + String(schedule.nextDoseTime);
        json += "}";
        
        request->send(200, "application/json", json);
    });
    
    // API: Set dosing schedule
    server->on("/api/dosing/schedule", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(256);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            DosingScheduleConfig schedule;
            schedule.enabled = doc["enabled"] | false;
            
            String scheduleType = doc["schedule"] | "manual";
            if (scheduleType == "daily") schedule.schedule = DOSE_DAILY;
            else if (scheduleType == "weekly") schedule.schedule = DOSE_WEEKLY;
            else if (scheduleType == "custom") schedule.schedule = DOSE_CUSTOM;
            else schedule.schedule = DOSE_MANUAL;
            
            schedule.customDays = doc["customDays"] | 0;
            schedule.hour = doc["hour"] | 9;
            schedule.minute = doc["minute"] | 0;
            schedule.doseVolume = doc["doseVolume"] | 5.0;
            
            dosingPump->setSchedule(schedule.schedule, schedule.hour, schedule.minute, schedule.doseVolume);
            
            if (eventLogger) {
                eventLogger->info("dosing", "Schedule updated: " + scheduleType);
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Set safety limits
    server->on("/api/dosing/safety", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!dosingPump) {
                request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(128);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            // Get current schedule config to extract limits if available
            DosingScheduleConfig schedule = dosingPump->getScheduleConfig();
            int maxDose = 50;  // Default 50 mL
            int maxDaily = 500; // Default 500 mL
            
            if (doc.containsKey("maxDose")) {
                maxDose = doc["maxDose"];
            }
            
            if (doc.containsKey("maxDaily")) {
                maxDaily = doc["maxDaily"];
            }
            
            dosingPump->setSafetyLimits(maxDose, maxDaily);
            
            if (doc.containsKey("enabled")) {
                bool enabled = doc["enabled"];
                dosingPump->setSafetyEnabled(enabled);
            }
            
            if (eventLogger) {
                eventLogger->info("dosing", "Safety limits updated");
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Get statistics
    server->on("/api/dosing/stats", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!dosingPump) {
            request->send(500, "application/json", "{\"error\":\"Dosing pump not initialized\"}");
            return;
        }
        
        unsigned long totalRuntime = dosingPump->getTotalRuntime();
        unsigned int totalDoses = dosingPump->getTotalDoses();
        float avgVolume = dosingPump->getAverageDoseVolume();
        float totalVolume = dosingPump->getTotalVolumeDosed();
        int daysSinceCal = dosingPump->getDaysSinceCalibration();
        
        String json = "{";
        json += "\"totalRuntime\":" + String(totalRuntime) + ",";
        json += "\"totalDoses\":" + String(totalDoses) + ",";
        json += "\"averageVolume\":" + String(avgVolume, 2) + ",";
        json += "\"totalVolume\":" + String(totalVolume, 2) + ",";
        json += "\"daysSinceCalibration\":" + String(daysSinceCal);
        json += "}";
        
        request->send(200, "application/json", json);
    });
    
    // API: Get system time info
    server->on("/api/time/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!wifiMgr) {
            request->send(500, "application/json", "{\"error\":\"WiFi manager not initialized\"}");
            return;
        }
        
        bool synced = wifiMgr->isTimeSynced();
        String localTime = wifiMgr->getLocalTime();
        
        time_t now;
        time(&now);
        
        String json = "{";
        json += "\"synced\":" + String(synced ? "true" : "false") + ",";
        json += "\"localTime\":\"" + localTime + "\",";
        json += "\"unixTime\":" + String(now);
        json += "}";
        
        request->send(200, "application/json", json);
    });
    
    // API: Set NTP configuration
    server->on("/api/time/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!configMgr) {
                request->send(500, "application/json", "{\"error\":\"Config manager not initialized\"}");
                return;
            }
            
            DynamicJsonDocument doc(256);
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            const char* ntpServer = doc["ntpServer"] | "pool.ntp.org";
            int gmtOffset = doc["gmtOffset"] | 0;
            int dstOffset = doc["dstOffset"] | 0;
            
            configMgr->setNTP(ntpServer, gmtOffset, dstOffset);
            
            if (eventLogger) {
                eventLogger->info("system", "NTP configuration updated");
            }
            
            // Trigger time re-sync
            if (wifiMgr && wifiMgr->isConnected()) {
                wifiMgr->syncTime();
            }
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        });
    
    // API: Restart device
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        
        // Publish offline status before restart
        if (mqttClient && mqttClient->connected()) {
            SystemConfig& cfg = config->getConfig();
            char topic[128];
            snprintf(topic, sizeof(topic), "%s/status", cfg.mqttTopicPrefix);
            mqttClient->publish(topic, "offline", true);
            delay(100);  // Give time for message to send
        }
        
        delay(1000);
        ESP.restart();
    });
}

void WebServerManager::handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // Handle incoming WebSocket messages if needed
    }
}

void WebServerManager::broadcastSensorData() {
    SensorData data = getSensorData();
    
    StaticJsonDocument<512> doc;
    doc["temperature"] = data.temperature;
    doc["ph"] = data.ph;
    doc["tds"] = data.tds;
    doc["heaterState"] = data.heaterState;
    doc["co2State"] = data.co2State;
    doc["emergency"] = data.tempEmergencyStop || data.co2EmergencyStop;
    
    String response;
    serializeJson(doc, response);
    ws->textAll(response);
}

void WebServerManager::update() {
    ws->cleanupClients();
    
    unsigned long now = millis();
    if (now - lastBroadcast > BROADCAST_INTERVAL) {
        broadcastSensorData();
        lastBroadcast = now;
    }
}
