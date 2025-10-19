#ifndef SYSTEM_TASKS_H
#define SYSTEM_TASKS_H

#include <Arduino.h>
#include "TemperatureSensor.h"
#include "AmbientTempSensor.h"
#include "PHSensor.h"
#include "TDSSensor.h"
#include "AdaptivePID.h"
#include "RelayController.h"
#include "WaterChangePredictor.h"
#include "ConfigManager.h"
#include "EventLogger.h"
#include "WaterChangeAssistant.h"
#include "PatternLearner.h"
#include "DosingPump.h"
#include "DisplayManager.h"
#include <PubSubClient.h>

// Shared data structure with mutex protection
struct SensorData {
    float temperature;
    float ambientTemp;
    float ph;
    float tds;
    float tempPIDOutput;
    float co2PIDOutput;
    bool heaterState;
    bool co2State;
    bool tempEmergencyStop;
    bool co2EmergencyStop;
    unsigned long lastUpdate;
};

// Task handles
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t controlTaskHandle;
extern TaskHandle_t mqttTaskHandle;

// Shared data and mutex
extern SensorData sharedSensorData;
extern SemaphoreHandle_t dataMutex;

// Hardware objects (accessed by tasks)
extern TemperatureSensor* tempSensor;
extern AmbientTempSensor* ambientSensor;
extern PHSensor* phSensor;
extern TDSSensor* tdsSensor;
extern AdaptivePID* tempPID;
extern AdaptivePID* co2PID;
extern RelayController* heaterRelay;
extern RelayController* co2Relay;
extern ConfigManager* configMgr;
extern PubSubClient* mqttClient;
extern EventLogger* eventLogger;
extern WaterChangeAssistant* waterChangeAssistant;
extern PatternLearner* patternLearner;
extern DosingPump* dosingPump;
extern WaterChangePredictor* wcPredictor;
extern DisplayManager* displayMgr;

// Forward declarations for components
class WiFiManager;
extern WiFiManager* wifiMgr;

// Task functions
void sensorTask(void* parameter);
void controlTask(void* parameter);
void mqttTask(void* parameter);
void displayTask(void* parameter);

// Initialize all tasks
void initializeTasks();

// Helper functions for safe data access
void updateSensorData(float temp, float ambientTemp, float ph, float tds);
SensorData getSensorData();

// MQTT Alert publishing
void sendMQTTAlert(const char* category, const char* message, bool critical = true);

#endif
