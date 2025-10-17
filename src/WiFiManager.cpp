#include "WiFiManager.h"
#include "SystemTasks.h"
#include <time.h>

WiFiManager::WiFiManager(ConfigManager* configMgr) 
    : config(configMgr), dnsServer(nullptr), apMode(false), 
      connected(false), timeSynced(false), lastReconnectAttempt(0), lastConnectionCheck(0),
      reconnectAttempts(0), reconnectInterval(MIN_RECONNECT_INTERVAL) {
}

WiFiManager::~WiFiManager() {
    if (dnsServer) {
        delete dnsServer;
    }
}

void WiFiManager::begin() {
    SystemConfig& cfg = config->getConfig();
    
    // Try to connect to configured WiFi
    if (strlen(cfg.wifiSSID) > 0) {
        Serial.printf("Attempting to connect to WiFi: %s\n", cfg.wifiSSID);
        connectToWiFi();
        
        // Wait up to 20 seconds for connection
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            reconnectAttempts = 0;
            reconnectInterval = MIN_RECONNECT_INTERVAL;
            Serial.printf("Connected to WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
            if (eventLogger) {
                eventLogger->info("network", "WiFi connected successfully");
            }
            
            // Synchronize time with NTP
            syncTime();
            
            return;
        }
    }
    
    // If no WiFi configured or connection failed, start AP mode
    Serial.println("Starting Access Point mode");
    if (eventLogger) {
        eventLogger->warning("network", "WiFi connection failed, starting AP mode");
    }
    startAccessPoint();
}

void WiFiManager::connectToWiFi() {
    SystemConfig& cfg = config->getConfig();
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSSID, cfg.wifiPassword);
    apMode = false;
}

void WiFiManager::startAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("AquariumController", "12345678");
    
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("AP IP address: %s\n", IP.toString().c_str());
    
    // Start DNS server for captive portal
    if (!dnsServer) {
        dnsServer = new DNSServer();
    }
    dnsServer->start(53, "*", IP);
    
    apMode = true;
    connected = false;
}

void WiFiManager::update() {
    if (apMode) {
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
    } else {
        unsigned long now = millis();
        
        // Periodic connection check
        if (now - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
            lastConnectionCheck = now;
            
            if (WiFi.status() != WL_CONNECTED) {
                if (connected) {
                    // Just lost connection
                    connected = false;
                    Serial.println("WiFi connection lost!");
                    if (eventLogger) {
                        eventLogger->error("network", "WiFi connection lost");
                    }
                    sendMQTTAlert("network", "WiFi connection lost", false);
                }
                
                // Attempt reconnection with exponential backoff
                if (now - lastReconnectAttempt > reconnectInterval) {
                    reconnectAttempts++;
                    lastReconnectAttempt = now;
                    
                    Serial.printf("WiFi reconnection attempt #%d (interval: %lu ms)\n", 
                                  reconnectAttempts, reconnectInterval);
                    
                    if (eventLogger) {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "WiFi reconnection attempt #%d", reconnectAttempts);
                        eventLogger->warning("network", msg);
                    }
                    
                    connectToWiFi();
                    
                    // Increase backoff interval
                    reconnectInterval = getNextReconnectInterval();
                    
                    // Check if we should give up and switch to AP mode
                    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
                        Serial.println("Max reconnect attempts reached, switching to AP mode");
                        if (eventLogger) {
                            eventLogger->error("network", "Max WiFi reconnect attempts reached, switching to AP mode");
                        }
                        startAccessPoint();
                    }
                }
            } else if (!connected) {
                // Reconnected!
                connected = true;
                resetReconnectBackoff();
                Serial.printf("WiFi reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
                if (eventLogger) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "WiFi reconnected after %d attempts", reconnectAttempts);
                    eventLogger->info("network", msg);
                }
                sendMQTTAlert("network", "WiFi connection restored", false);
                
                // Re-sync time after reconnection
                syncTime();
            }
            
            // Monitor signal strength
            if (connected) {
                int rssi = WiFi.RSSI();
                if (rssi < -80) {
                    if (eventLogger) {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Weak WiFi signal: %d dBm", rssi);
                        eventLogger->warning("network", msg);
                    }
                }
            }
        }
    }
}

void WiFiManager::resetReconnectBackoff() {
    reconnectAttempts = 0;
    reconnectInterval = MIN_RECONNECT_INTERVAL;
}

unsigned long WiFiManager::getNextReconnectInterval() {
    // Exponential backoff: double each time, up to max
    unsigned long nextInterval = reconnectInterval * 2;
    if (nextInterval > MAX_RECONNECT_INTERVAL) {
        nextInterval = MAX_RECONNECT_INTERVAL;
    }
    return nextInterval;
}

bool WiFiManager::isConnected() {
    return connected;
}

bool WiFiManager::isAPMode() {
    return apMode;
}

String WiFiManager::getIPAddress() {
    if (apMode) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

int WiFiManager::getReconnectAttempts() {
    return reconnectAttempts;
}

int WiFiManager::getSignalStrength() {
    if (connected && !apMode) {
        return WiFi.RSSI();
    }
    return -100; // Very weak/no signal
}

void WiFiManager::syncTime() {
    SystemConfig& cfg = config->getConfig();
    
    Serial.println("\nSynchronizing time with NTP server...");
    Serial.printf("NTP Server: %s\n", cfg.ntpServer);
    Serial.printf("GMT Offset: %+d hours\n", cfg.gmtOffsetSec / 3600);
    Serial.printf("DST Offset: %+d hours\n", cfg.daylightOffsetSec / 3600);
    
    // Configure time with NTP
    configTime(cfg.gmtOffsetSec, cfg.daylightOffsetSec, cfg.ntpServer);
    
    // Wait for time to be set (up to 10 seconds)
    Serial.print("Waiting for NTP sync");
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts < 20) {
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Check if time is valid (year > 2020)
        if (timeinfo.tm_year > (2020 - 1900)) {
            timeSynced = true;
            Serial.println(" Success!");
            Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            
            if (eventLogger) {
                char timeStr[32];
                snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                String msg = "Time synced via NTP: " + String(timeStr);
                eventLogger->info("system", msg.c_str());
            }
            return;
        }
        
        Serial.print(".");
        delay(500);
        attempts++;
    }
    
    // Failed to sync
    timeSynced = false;
    Serial.println(" Failed!");
    Serial.println("WARNING: Time synchronization failed - schedules may not work correctly");
    
    if (eventLogger) {
        eventLogger->warning("system", "NTP time synchronization failed");
    }
}

bool WiFiManager::isTimeSynced() {
    return timeSynced;
}

String WiFiManager::getLocalTime() {
    if (!timeSynced) {
        return "Time not synced";
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    return String(timeStr);
}
