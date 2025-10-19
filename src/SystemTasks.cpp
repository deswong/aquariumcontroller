#include "SystemTasks.h"
#include <ArduinoJson.h>

// Task handles
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// Shared data and mutex
SensorData sharedSensorData;
SemaphoreHandle_t dataMutex = NULL;

// Hardware objects
TemperatureSensor* tempSensor = NULL;
AmbientTempSensor* ambientSensor = NULL;
PHSensor* phSensor = NULL;
TDSSensor* tdsSensor = NULL;
AdaptivePID* tempPID = NULL;
AdaptivePID* co2PID = NULL;
RelayController* heaterRelay = NULL;
RelayController* co2Relay = NULL;
extern ConfigManager* configMgr;
PubSubClient* mqttClient = NULL;
EventLogger* eventLogger = NULL;
WaterChangeAssistant* waterChangeAssistant = NULL;
PatternLearner* patternLearner = NULL;
DosingPump* dosingPump = NULL;
WaterChangePredictor* wcPredictor = NULL;
DisplayManager* displayMgr = NULL;

// Forward declarations
class WiFiManager;
WiFiManager* wifiMgr = NULL;

void updateSensorData(float temp, float ambientTemp, float ph, float tds) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        sharedSensorData.temperature = temp;
        sharedSensorData.ambientTemp = ambientTemp;
        sharedSensorData.ph = ph;
        sharedSensorData.tds = tds;
        sharedSensorData.lastUpdate = millis();
        xSemaphoreGive(dataMutex);
    }
}

SensorData getSensorData() {
    SensorData data;
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        data = sharedSensorData;
        xSemaphoreGive(dataMutex);
    }
    return data;
}

void sensorTask(void* parameter) {
    Serial.println("Sensor task started");
    
    const TickType_t xDelay = pdMS_TO_TICKS(1000); // 1 second
    
    while (true) {
        // Read all sensors
        float temp = tempSensor->readTemperature();
        float ambientTemp = ambientSensor->readTemperature();
        float ph = phSensor->readPH(temp, ambientTemp); // Water temp for normal, ambient for calibration
        float tds = tdsSensor->readTDS(temp); // Temperature compensated
        
        // Update shared data
        updateSensorData(temp, ambientTemp, ph, tds);
        
        // Update water change predictor with current TDS
        if (wcPredictor) {
            wcPredictor->updateTDS(tds);
        }
        
        // Update pattern learner
        if (patternLearner && patternLearner->isEnabled()) {
            patternLearner->update(temp, ph, tds, ambientTemp);
        }
        
        // Log periodically (every 10 seconds)
        static unsigned long lastLog = 0;
        if (millis() - lastLog > 10000) {
            Serial.printf("[SENSORS] Water: %.2f°C, Ambient: %.2f°C, pH: %.2f, TDS: %.0f ppm\n", 
                         temp, ambientTemp, ph, tds);
            lastLog = millis();
        }
        
        vTaskDelay(xDelay);
    }
}

void controlTask(void* parameter) {
    Serial.println("Control task started");
    
    const TickType_t xDelay = pdMS_TO_TICKS(2000); // 2 seconds
    unsigned long lastTime = millis();
    
    while (true) {
        // Check if water change is in progress
        bool systemsShouldPause = false;
        if (waterChangeAssistant && waterChangeAssistant->areSystemsPaused()) {
            systemsShouldPause = true;
        }
        
        // Get current sensor readings
        SensorData data = getSensorData();
        
        // Calculate time delta
        unsigned long currentTime = millis();
        float dt = (currentTime - lastTime) / 1000.0; // Convert to seconds
        lastTime = currentTime;
        
        float tempOutput = 0;
        float co2Output = 0;
        
        if (!systemsShouldPause) {
            // Temperature PID control
            tempOutput = tempPID->compute(data.temperature, dt);
            heaterRelay->setDutyCycle(tempOutput);
            heaterRelay->update(); // Update for time proportional control
            
            // CO2 PID control (based on pH)
            // Lower pH means more CO2, so invert the relationship
            co2Output = co2PID->compute(data.ph, dt);
            co2Relay->setDutyCycle(co2Output);
            co2Relay->update(); // Update for time proportional control;
        } else {
            // Systems paused for water change
            heaterRelay->setDutyCycle(0);
            co2Relay->setDutyCycle(0);
        }
        
        // Update shared status
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            sharedSensorData.tempPIDOutput = tempOutput;
            sharedSensorData.co2PIDOutput = co2Output;
            sharedSensorData.heaterState = heaterRelay->getState();
            sharedSensorData.co2State = co2Relay->getState();
            sharedSensorData.tempEmergencyStop = tempPID->isEmergencyStopped();
            sharedSensorData.co2EmergencyStop = co2PID->isEmergencyStopped();
            xSemaphoreGive(dataMutex);
        }
        
        // Check emergency stops
        if (tempPID->isEmergencyStopped()) {
            heaterRelay->safetyDisable();
            Serial.println("ALERT: Temperature emergency stop active!");
            sendMQTTAlert("temperature", "Temperature emergency stop activated - safety limit exceeded", true);
        }
        
        if (co2PID->isEmergencyStopped()) {
            co2Relay->safetyDisable();
            Serial.println("ALERT: CO2 emergency stop active!");
            sendMQTTAlert("co2", "CO2 emergency stop activated - pH safety limit exceeded", true);
        }
        
        // Log control status periodically
        static unsigned long lastLog = 0;
        if (millis() - lastLog > 10000) {
            Serial.printf("[CONTROL] Temp PID: %.1f%%, CO2 PID: %.1f%%, Heater: %s, CO2: %s\n",
                         tempOutput, co2Output,
                         heaterRelay->getState() ? "ON" : "OFF",
                         co2Relay->getState() ? "ON" : "OFF");
            lastLog = millis();
        }
        
        vTaskDelay(xDelay);
    }
}

void mqttTask(void* parameter) {
    Serial.println("MQTT task started");
    
    const TickType_t xDelay = pdMS_TO_TICKS(5000); // 5 seconds
    
    // Previous values for change detection
    static float lastTemp = -999.0;
    static float lastAmbientTemp = -999.0;
    static float lastPH = -999.0;
    static float lastTDS = -999.0;
    static bool lastHeaterState = false;
    static bool lastCO2State = false;
    static bool firstPublish = true;
    
    // Minimum change threshold to trigger publish (avoid noise)
    const float TEMP_THRESHOLD = 0.1;  // 0.1°C
    const float PH_THRESHOLD = 0.05;   // 0.05 pH
    const float TDS_THRESHOLD = 5.0;   // 5 ppm
    
    while (true) {
        if (!mqttClient->connected()) {
            vTaskDelay(xDelay);
            continue;
        }
        
        // Get current data
        SensorData data = getSensorData();
        SystemConfig config = configMgr->getConfig();
        
        // Publish to MQTT (only on change or first publish)
        char topic[128];
        char payload[64];
        bool anyPublished = false;
        
        // Publish to individual topics (if enabled)
        if (config.mqttPublishIndividual) {
            // Temperature - publish if changed significantly or first time
            if (firstPublish || fabs(data.temperature - lastTemp) >= TEMP_THRESHOLD) {
                snprintf(topic, sizeof(topic), "%s/temperature", config.mqttTopicPrefix);
                snprintf(payload, sizeof(payload), "%.2f", data.temperature);
                mqttClient->publish(topic, payload);
                lastTemp = data.temperature;
                anyPublished = true;
            }
        
        // Ambient temperature
        if (firstPublish || fabs(data.ambientTemp - lastAmbientTemp) >= TEMP_THRESHOLD) {
            snprintf(topic, sizeof(topic), "%s/ambient_temperature", config.mqttTopicPrefix);
            snprintf(payload, sizeof(payload), "%.2f", data.ambientTemp);
            mqttClient->publish(topic, payload);
            lastAmbientTemp = data.ambientTemp;
            anyPublished = true;
        }
        
        // pH
        if (firstPublish || fabs(data.ph - lastPH) >= PH_THRESHOLD) {
            snprintf(topic, sizeof(topic), "%s/ph", config.mqttTopicPrefix);
            snprintf(payload, sizeof(payload), "%.2f", data.ph);
            mqttClient->publish(topic, payload);
            lastPH = data.ph;
            anyPublished = true;
        }
        
        // TDS
        if (firstPublish || fabs(data.tds - lastTDS) >= TDS_THRESHOLD) {
            snprintf(topic, sizeof(topic), "%s/tds", config.mqttTopicPrefix);
            snprintf(payload, sizeof(payload), "%.0f", data.tds);
            mqttClient->publish(topic, payload);
            lastTDS = data.tds;
            anyPublished = true;
        }
        
        // Heater state - always publish state changes
        if (firstPublish || data.heaterState != lastHeaterState) {
            snprintf(topic, sizeof(topic), "%s/heater", config.mqttTopicPrefix);
            mqttClient->publish(topic, data.heaterState ? "ON" : "OFF");
            lastHeaterState = data.heaterState;
            anyPublished = true;
        }
        
        // CO2 state - always publish state changes
        if (firstPublish || data.co2State != lastCO2State) {
            snprintf(topic, sizeof(topic), "%s/co2", config.mqttTopicPrefix);
            mqttClient->publish(topic, data.co2State ? "ON" : "OFF");
            lastCO2State = data.co2State;
            anyPublished = true;
        }
        
            // Emergency stops - always publish immediately
            if (data.tempEmergencyStop || data.co2EmergencyStop) {
                snprintf(topic, sizeof(topic), "%s/alert", config.mqttTopicPrefix);
                snprintf(payload, sizeof(payload), "EMERGENCY_STOP: Temp=%d CO2=%d", 
                        data.tempEmergencyStop, data.co2EmergencyStop);
                mqttClient->publish(topic, payload);
                anyPublished = true;
            }
        } // End individual topic publishing
        
        if (anyPublished && config.mqttPublishIndividual) {
            Serial.println("[MQTT] Data published (values changed)");
        }
        
        // Publish JSON payload (if enabled)
        if (config.mqttPublishJSON) {
            // Only publish JSON if anything changed OR first publish
            if (anyPublished || firstPublish) {
                snprintf(topic, sizeof(topic), "%s/data", config.mqttTopicPrefix);
                
                // Create JSON payload with all sensor data and control outputs
                StaticJsonDocument<1024> doc;
                doc["temperature"] = data.temperature;
                doc["ambient_temp"] = data.ambientTemp;
                doc["ph"] = data.ph;
                doc["tds"] = data.tds;
                doc["heater"] = data.heaterState ? "ON" : "OFF";
                doc["co2"] = data.co2State ? "ON" : "OFF";
                doc["temp_pid_output"] = data.tempPIDOutput;
                doc["co2_pid_output"] = data.co2PIDOutput;
                doc["temp_emergency"] = data.tempEmergencyStop;
                doc["co2_emergency"] = data.co2EmergencyStop;
                
                // Add dosing pump status if available
                if (dosingPump) {
                    JsonObject pump = doc.createNestedObject("dosing_pump");
                    pump["state"] = dosingPump->getStateString();
                    pump["is_dosing"] = dosingPump->isDosing();
                    pump["progress"] = dosingPump->getProgress();
                    pump["volume_pumped"] = dosingPump->getVolumePumped();
                    pump["target_volume"] = dosingPump->getTargetVolume();
                    pump["current_speed"] = dosingPump->getCurrentSpeed();
                    pump["daily_volume"] = dosingPump->getDailyVolumeDosed();
                    pump["schedule_enabled"] = dosingPump->isScheduleEnabled();
                    pump["hours_until_next"] = dosingPump->getHoursUntilNextDose();
                }
                
                // Add water change prediction if available
                if (wcPredictor) {
                    JsonObject wc = doc.createNestedObject("water_change_prediction");
                    wc["days_until_change"] = wcPredictor->getPredictedDaysUntilChange();
                    wc["tds_increase_rate"] = wcPredictor->getCurrentTDSIncreaseRate();
                    wc["confidence_percent"] = wcPredictor->getConfidenceLevel();
                    wc["needs_change"] = wcPredictor->needsWaterChange();
                    wc["days_since_last"] = wcPredictor->getDaysSinceLastChange();
                }
                
                doc["timestamp"] = millis() / 1000;
                
                char jsonPayload[1024];
                serializeJson(doc, jsonPayload);
                mqttClient->publish(topic, jsonPayload);
                
                Serial.println("[MQTT] JSON data published");
            }
        }
        
        firstPublish = false;
        vTaskDelay(xDelay);
    }
}

void displayTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz update rate
    
    Serial.println("[Display] Task started");
    
    for (;;) {
        if (displayMgr != nullptr) {
            // Update display with current sensor data
            displayMgr->update();
            
            // Update display with latest sensor readings (every cycle)
            SensorData data = getSensorData();
            SystemConfig& config = configMgr->getConfig();
            
            displayMgr->updateTemperature(data.temperature, config.tempTarget);
            displayMgr->updatePH(data.ph, config.phTarget);
            displayMgr->updateTDS(data.tds);
            displayMgr->updateAmbientTemperature(data.ambientTemp);
            displayMgr->updateHeaterState(data.heaterState);
            displayMgr->updateCO2State(data.co2State);
            
            // Update water change prediction if available
            if (wcPredictor != nullptr) {
                float daysUntil = wcPredictor->getPredictedDaysUntilChange();
                int confidence = wcPredictor->getConfidenceLevel();
                if (daysUntil > 0) {
                    displayMgr->updateWaterChangePrediction(daysUntil, confidence);
                }
            }
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void initializeTasks() {
    // Create mutex for shared data
    dataMutex = xSemaphoreCreateMutex();
    
    if (dataMutex == NULL) {
        Serial.println("ERROR: Failed to create mutex");
        return;
    }
    
    // Initialize shared data
    memset(&sharedSensorData, 0, sizeof(SensorData));
    sharedSensorData.lastUpdate = millis();
    
    // Create sensor reading task (Core 0, high priority)
    xTaskCreatePinnedToCore(
        sensorTask,
        "SensorTask",
        6144,           // Stack size (increased for safety)
        NULL,
        2,              // Priority
        &sensorTaskHandle,
        0               // Core 0
    );
    
    // Create control task (Core 1, high priority)
    xTaskCreatePinnedToCore(
        controlTask,
        "ControlTask",
        6144,           // Increased for safety
        NULL,
        2,
        &controlTaskHandle,
        1               // Core 1
    );
    
    // Create MQTT task (Core 1, lower priority)
    xTaskCreatePinnedToCore(
        mqttTask,
        "MQTTTask",
        8192,           // Increased from 4096 to prevent stack overflow
        NULL,
        1,
        &mqttTaskHandle,
        1               // Core 1
    );
    
    // Create Display task (Core 1, lowest priority)
    if (displayMgr != nullptr) {
        xTaskCreatePinnedToCore(
            displayTask,
            "DisplayTask",
            4096,
            NULL,
            0,              // Lowest priority
            &displayTaskHandle,
            1               // Core 1
        );
    }
    
    Serial.println("All tasks created successfully");
}

void sendMQTTAlert(const char* category, const char* message, bool critical) {
    if (!mqttClient || !mqttClient->connected() || !configMgr) {
        return; // MQTT not available
    }
    
    // Log the alert first
    if (eventLogger) {
        if (critical) {
            eventLogger->critical(category, message);
        } else {
            eventLogger->error(category, message);
        }
    }
    
    // Publish to MQTT
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/alert", configMgr->getConfig().mqttTopicPrefix);
    
    // Create JSON payload with timestamp
    StaticJsonDocument<512> doc;
    doc["category"] = category;
    doc["message"] = message;
    doc["critical"] = critical;
    doc["timestamp"] = millis() / 1000;
    
    char payload[512];
    serializeJson(doc, payload);
    
    mqttClient->publish(topic, payload, true); // Retain alert messages
    
    Serial.printf("MQTT Alert [%s]: %s\n", category, message);
}
