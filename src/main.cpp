#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include "TemperatureSensor.h"
#include "AmbientTempSensor.h"
#include "PHSensor.h"
#include "TDSSensor.h"
#include "AdaptivePID.h"
#include "RelayController.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "OTAManager.h"
#include "SystemTasks.h"

// Watchdog timeout in seconds
#define WDT_TIMEOUT 30

// WiFi and MQTT clients
WiFiClient wifiClient;
PubSubClient mqttClientObj(wifiClient);

// Managers
ConfigManager* configMgr;
extern WiFiManager* wifiMgr;
WebServerManager* webServer;
OTAManager* otaManager;

// MQTT reconnection
unsigned long lastMqttReconnect = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // 5 seconds

void setupMQTT() {
    SystemConfig& config = configMgr->getConfig();
    
    if (strlen(config.mqttServer) > 0) {
        mqttClientObj.setServer(config.mqttServer, config.mqttPort);
        Serial.printf("MQTT configured: %s:%d\n", config.mqttServer, config.mqttPort);
    }
}

void reconnectMQTT() {
    if (!wifiMgr->isConnected()) return;
    
    unsigned long now = millis();
    if (now - lastMqttReconnect < MQTT_RECONNECT_INTERVAL) return;
    lastMqttReconnect = now;
    
    SystemConfig& config = configMgr->getConfig();
    
    if (strlen(config.mqttServer) == 0) return;
    
    Serial.print("Attempting MQTT connection...");
    
    String clientId = String(config.mqttClientId);
    
    // Prepare Last Will and Testament (LWT)
    char lwt_topic[128];
    snprintf(lwt_topic, sizeof(lwt_topic), "%s/status", config.mqttTopicPrefix);
    const char* lwt_message = "offline";
    
    bool connected = false;
    if (strlen(config.mqttUser) > 0) {
        // Connect with authentication and LWT
        // connect(clientId, user, password, willTopic, willQoS, willRetain, willMessage)
        connected = mqttClientObj.connect(clientId.c_str(), 
                                          config.mqttUser, 
                                          config.mqttPassword,
                                          lwt_topic,
                                          1,        // QoS 1
                                          true,     // Retain
                                          lwt_message);
    } else {
        // Connect without authentication but with LWT
        // connect(clientId, willTopic, willQoS, willRetain, willMessage)
        connected = mqttClientObj.connect(clientId.c_str(),
                                          lwt_topic,
                                          1,        // QoS 1
                                          true,     // Retain
                                          lwt_message);
    }
    
    if (connected) {
        Serial.println("connected");
        
        // Publish online status (retained)
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/status", config.mqttTopicPrefix);
        mqttClientObj.publish(topic, "online", true);  // Retained message
        
        Serial.printf("[MQTT] LWT configured: %s = offline (on disconnect)\n", lwt_topic);
        Serial.printf("[MQTT] Status published: %s = online\n", topic);
    } else {
        Serial.printf("failed, rc=%d\n", mqttClientObj.state());
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Disable Bluetooth to free up memory (not used in this application)
    #if CONFIG_BT_ENABLED
        btStop();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        esp_bt_mem_release(ESP_BT_MODE_BTDM);
        Serial.println("Bluetooth disabled - freeing RAM and Flash");
    #endif
    
    Serial.println("\n\n=================================");
    Serial.println("Aquarium Controller Starting...");
    Serial.println("=================================\n");
    
    // Initialize Watchdog Timer
    Serial.println("Configuring watchdog timer...");
    esp_task_wdt_init(WDT_TIMEOUT, true); // 30 second timeout, panic on timeout
    esp_task_wdt_add(NULL); // Add current thread to watchdog
    Serial.printf("Watchdog enabled: %d second timeout\n", WDT_TIMEOUT);
    
    // Initialize configuration manager
    configMgr = new ConfigManager();
    configMgr->begin();
    configMgr->printConfig();
    
    SystemConfig& config = configMgr->getConfig();
    
    // Initialize event logger
    Serial.println("\nInitializing event logger...");
    eventLogger = new EventLogger();
    if (eventLogger->begin()) {
        Serial.println("Event logger initialized successfully");
        eventLogger->info("system", "Aquarium controller starting");
        eventLogger->info("system", "Watchdog timer enabled (30 second timeout)");
    } else {
        Serial.println("WARNING: Event logger initialization failed!");
    }
    
    // Initialize WiFi
    wifiMgr = new WiFiManager(configMgr);
    wifiMgr->begin();
    
    if (eventLogger) {
        eventLogger->info("network", "WiFi initialized");
    }
    
    // Initialize sensors
    Serial.println("\nInitializing sensors...");
    tempSensor = new TemperatureSensor(config.tempSensorPin);
    ambientSensor = new AmbientTempSensor(config.ambientSensorPin);
    phSensor = new PHSensor(config.phSensorPin);
    tdsSensor = new TDSSensor(config.tdsSensorPin);
    
    if (!tempSensor->begin()) {
        Serial.println("ERROR: Temperature sensor initialization failed!");
        if (eventLogger) {
            eventLogger->error("sensors", "Temperature sensor initialization failed");
        }
    }
    if (!ambientSensor->begin()) {
        Serial.println("WARNING: Ambient temperature sensor initialization failed!");
        if (eventLogger) {
            eventLogger->warning("sensors", "Ambient temperature sensor initialization failed");
        }
    }
    phSensor->begin();
    tdsSensor->begin();
    
    if (eventLogger) {
        eventLogger->info("sensors", "All sensors initialized");
        
        // Check pH calibration age
        if (phSensor->isCalibrationExpired()) {
            eventLogger->error("calibration", "pH calibration EXPIRED - requires immediate recalibration");
            sendMQTTAlert("calibration", "pH sensor calibration expired (>60 days) - accuracy not guaranteed", true);
        } else if (phSensor->needsCalibration()) {
            eventLogger->warning("calibration", "pH calibration aging - recalibration recommended");
            sendMQTTAlert("calibration", "pH sensor calibration aging (>30 days) - recalibration recommended", false);
        }
    }
    
    // Initialize PID controllers with ML-Enhanced features (Phases 1, 2, 3)
    Serial.println("\nInitializing ML-Enhanced PID controllers...");
    tempPID = new AdaptivePID("temp-pid", 2.0, 0.5, 1.0);
    co2PID = new AdaptivePID("co2-pid", 2.0, 0.5, 1.0);
    
    // ===== Temperature PID Configuration =====
    tempPID->begin();
    tempPID->setTarget(config.tempTarget);
    tempPID->setSafetyLimits(config.tempSafetyMax, config.tempSafetyMax - config.tempTarget);
    tempPID->setOutputLimits(0, 100);
    
    // Phase 1: Hardware timer, profiling, ML cache
    tempPID->enableHardwareTimer(100000);            // 100ms = 10 Hz control loop
    tempPID->enablePerformanceProfiling(true);       // Track CPU usage
    tempPID->enableMLAdaptation(true);               // ML parameter adaptation
    
    // Phase 2: Dual-core ML processing and Kalman filtering
    tempPID->enableDualCoreML(true);                 // ML on Core 0, control on Core 1
    tempPID->enableKalmanFilter(true, 0.001f, 0.1f); // Smooth sensor noise
    
    // Phase 3: Health monitoring, bumpless transfer, predictive feed-forward
    tempPID->enableHealthMonitoring(true);           // Automated diagnostics
    // Feed-forward model: TDS (0.1) + Ambient Temp (0.3) influence heater output
    tempPID->enableFeedForwardModel(true, 0.1f, 0.3f, 0.0f);
    
    // Traditional PID features
    tempPID->enableDerivativeFilter(true, 0.7);         // Smooth derivative
    tempPID->enableSetpointRamping(true, 0.5);          // Ramp at 0.5°C per second
    tempPID->enableIntegralWindupPrevention(true, 50.0); // Limit integral
    
    Serial.println("✓ Temperature PID: Phase 1+2+3 enabled (PSRAM, Dual-Core, Kalman, Health, Feed-Forward)");
    
    // ===== CO2/pH PID Configuration =====
    co2PID->begin();
    co2PID->setTarget(config.phTarget);
    co2PID->setSafetyLimits(config.phSafetyMin, config.phTarget - config.phSafetyMin);
    co2PID->setOutputLimits(0, 100);
    
    // Phase 1: Hardware timer, profiling, ML cache
    co2PID->enableHardwareTimer(100000);             // 100ms = 10 Hz control loop
    co2PID->enablePerformanceProfiling(true);        // Track CPU usage
    co2PID->enableMLAdaptation(true);                // ML parameter adaptation
    
    // Phase 2: Dual-core ML processing and Kalman filtering
    co2PID->enableDualCoreML(true);                  // ML on Core 0, control on Core 1
    co2PID->enableKalmanFilter(true, 0.002f, 0.2f);  // Higher noise tolerance for pH
    
    // Phase 3: Health monitoring, bumpless transfer, predictive feed-forward
    co2PID->enableHealthMonitoring(true);            // Automated diagnostics
    // Feed-forward model: pH (0.2) influence (TDS has minimal effect on CO2)
    co2PID->enableFeedForwardModel(true, 0.0f, 0.1f, 0.2f);
    
    // Traditional PID features
    co2PID->enableDerivativeFilter(true, 0.8);           // Higher filtering for pH
    co2PID->enableSetpointRamping(true, 0.1);            // Slower ramp for pH (0.1 pH/sec)
    co2PID->enableIntegralWindupPrevention(true, 30.0);  // Smaller integral for pH
    
    Serial.println("✓ CO2/pH PID: Phase 1+2+3 enabled (PSRAM, Dual-Core, Kalman, Health, Feed-Forward)");
    
    if (eventLogger) {
        eventLogger->info("control", "ML-Enhanced PID controllers initialized (Phase 1+2+3)");
    }
    
    // Initialize relays
    Serial.println("\nInitializing relays...");
    heaterRelay = new RelayController(config.heaterRelayPin, "Heater", false);
    co2Relay = new RelayController(config.co2RelayPin, "CO2", false);
    
    heaterRelay->begin();
    co2Relay->begin();
    
    // Enable time proportional for heater
    heaterRelay->setMode(TIME_PROPORTIONAL);
    heaterRelay->setWindowSize(15000); // 15 second window for heater

    // Enable time proportional for CO2
    co2Relay->setMode(TIME_PROPORTIONAL);
    co2Relay->setWindowSize(10000); // 10 second window for CO2
    
    // Initialize Water Change Assistant
    Serial.println("\nInitializing water change assistant...");
    waterChangeAssistant = new WaterChangeAssistant();
    waterChangeAssistant->begin(configMgr); // Pass ConfigManager to use tank dimensions
    waterChangeAssistant->setSchedule(SCHEDULE_WEEKLY, 25.0); // 25% weekly (if not saved in NVS)
    
    if (eventLogger) {
        eventLogger->info("waterchange", "Water change assistant ready");
        
        // Check if water change is overdue
        if (waterChangeAssistant->isChangeOverdue()) {
            eventLogger->warning("waterchange", "Water change is overdue");
            sendMQTTAlert("waterchange", "Water change overdue - maintenance required", false);
        }
    }
    
    // Initialize Pattern Learner
    Serial.println("\nInitializing pattern learner (ML system)...");
    patternLearner = new PatternLearner();
    patternLearner->begin();
    
    if (eventLogger) {
        if (patternLearner->isEnabled()) {
            if (patternLearner->arePatternsEstablished()) {
                eventLogger->info("ml", "Pattern learning active - anomaly detection enabled");
                Serial.printf("Pattern confidence: %.1f%%\n", patternLearner->getPatternConfidence() * 100);
            } else {
                eventLogger->info("ml", "Pattern learning active - collecting baseline data");
                Serial.println("Learning mode: Collecting data to establish patterns...");
            }
        } else {
            eventLogger->info("ml", "Pattern learning disabled");
        }
    }
    
    // Initialize Dosing Pump
    Serial.println("\nInitializing dosing pump...");
    // GPIO 25: DRV8871 IN1, GPIO 33: DRV8871 IN2, PWM Channel 1
    // (Changed from GPIO 26 to GPIO 33 to avoid conflict with heater relay)
    dosingPump = new DosingPump(25, 33, 1);
    dosingPump->begin();
    
    // Set default safety limits
    dosingPump->setSafetyLimits(50.0, 200.0);  // 50 mL per dose, 200 mL per day
    dosingPump->setSafetyEnabled(true);
    
    // Initialize Water Change Predictor
    Serial.println("\nInitializing water change predictor...");
    wcPredictor = new WaterChangePredictor();
    wcPredictor->begin();
    wcPredictor->setTargetTDSThreshold(400.0);  // 400 ppm - suitable for Australian tap water (typically 50-300 ppm)
    
    // Initialize Display Manager
    Serial.println("\nInitializing display...");
    displayMgr = new OLEDDisplayManager();
    if (!displayMgr->begin()) {
        Serial.println("WARNING: Display initialization failed!");
        if (eventLogger) {
            eventLogger->warning("display", "Display initialization failed");
        }
        delete displayMgr;
        displayMgr = nullptr;
    } else {
        Serial.println("Display initialized successfully");
        if (eventLogger) {
            eventLogger->info("display", "SSD1309 OLED display initialized");
        }
    }
    
    // Configure default schedule (weekly dosing at 9:00 AM, 5 mL)
    dosingPump->setSchedule(DOSE_WEEKLY, 9, 0, 5.0);  // Weekly at 9:00 AM, 5 mL
    dosingPump->enableSchedule(false);  // Disabled by default until calibrated
    
    if (eventLogger) {
        if (dosingPump->isCalibrated()) {
            float flowRate = dosingPump->getFlowRate();
            eventLogger->info("dosing", "Dosing pump initialized and calibrated");
            Serial.printf("Flow rate: %.3f mL/sec\n", flowRate);
        } else {
            eventLogger->info("dosing", "Dosing pump initialized - calibration required");
            Serial.println("Dosing pump needs calibration before use");
        }
    }
    
    // Initialize MQTT
    mqttClient = &mqttClientObj;
    setupMQTT();
    
    // Initialize web server
    Serial.println("\nStarting web server...");
    webServer = new WebServerManager(configMgr);
    webServer->begin();
    
    // Initialize OTA
    Serial.println("\nInitializing OTA updates...");
    otaManager = new OTAManager();
    otaManager->begin(webServer->server);
    
    // Start FreeRTOS tasks
    Serial.println("\nStarting system tasks...");
    initializeTasks();
    
    Serial.println("\n=================================");
    Serial.println("Aquarium Controller Ready!");
    Serial.println("=================================");
    Serial.printf("Access web interface at: http://%s\n", wifiMgr->getIPAddress().c_str());
    Serial.printf("OTA updates at: http://%s/update\n", wifiMgr->getIPAddress().c_str());
    Serial.println("=================================\n");
}

void loop() {
    // Feed the watchdog timer
    esp_task_wdt_reset();
    
    // Update WiFi manager
    wifiMgr->update();
    
    // Handle MQTT connection
    if (!mqttClient->connected()) {
        reconnectMQTT();
    } else {
        mqttClient->loop();
    }
    
    // Update config manager (deferred NVS saves)
    if (configMgr) {
        configMgr->update();
    }
    
    // Update water change assistant
    if (waterChangeAssistant) {
        waterChangeAssistant->update();
    }
    
    // Update dosing pump
    if (dosingPump) {
        dosingPump->update();
    }
    
    // Update web server
    webServer->update();
    
    // Handle OTA
    otaManager->update();
    
    // Small delay to prevent watchdog issues
    delay(10);
}
