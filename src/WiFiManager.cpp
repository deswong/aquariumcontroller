#include "WiFiManager.h"
#include "SystemTasks.h"
#include <time.h>
#include <esp_sntp.h>

WiFiManager::WiFiManager(ConfigManager* configMgr) 
    : config(configMgr), dnsServer(nullptr), apMode(false), 
      connected(false), timeSynced(false), lastReconnectAttempt(0), lastConnectionCheck(0),
      lastNTPSync(0), reconnectAttempts(0), reconnectInterval(MIN_RECONNECT_INTERVAL) {
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
        
        // Check SNTP sync status periodically (SNTP handles auto-sync internally)
        if (connected && !timeSynced && (now - lastNTPSync > 60000)) {
            // If not synced after 1 minute, check if SNTP succeeded in background
            time_t nowTime;
            struct tm timeinfo;
            time(&nowTime);
            localtime_r(&nowTime, &timeinfo);
            
            if (timeinfo.tm_year > (2020 - 1900)) {
                timeSynced = true;
                lastNTPSync = now;
                Serial.println("SNTP background sync completed!");
                if (eventLogger) {
                    eventLogger->info("system", "SNTP background sync completed");
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
    
    Serial.println("\nConfiguring SNTP (Simple Network Time Protocol)...");
    Serial.printf("Primary NTP Server: %s\n", cfg.ntpServer);
    Serial.printf("GMT Offset: %+d hours\n", cfg.gmtOffsetSec / 3600);
    Serial.printf("DST Offset: %+d hours\n", cfg.daylightOffsetSec / 3600);
    
    // Stop SNTP if already running
    if (sntp_enabled()) {
        sntp_stop();
    }
    
    // Configure SNTP with optimizations
    sntp_setoperatingmode(SNTP_OPMODE_POLL);              // Poll mode (not broadcast/multicast)
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);            // Smooth time adjustment (no sudden jumps)
    sntp_set_sync_interval(3600000);                       // Auto-sync every 1 hour
    
    // Set multiple NTP servers for redundancy (Australian servers with global fallback)
    sntp_setservername(0, cfg.ntpServer);                  // Primary: au.pool.ntp.org
    sntp_setservername(1, "time.google.com");              // Fallback 1: Google
    sntp_setservername(2, "pool.ntp.org");                 // Fallback 2: Global pool
    
    // Set timezone using configuration values
    // Brisbane default: AEST-10 (UTC+10, no DST)
    char tzStr[64];
    int hours = cfg.gmtOffsetSec / 3600;
    int dstHours = cfg.daylightOffsetSec / 3600;
    
    if (dstHours > 0) {
        // With DST (e.g., Sydney/Melbourne): AEST-10AEDT,M10.1.0,M4.1.0/3
        snprintf(tzStr, sizeof(tzStr), "AEST-%d AEDT,M10.1.0,M4.1.0/3", hours);
    } else {
        // Without DST (e.g., Brisbane/Queensland): AEST-10
        snprintf(tzStr, sizeof(tzStr), "AEST-%d", hours);
    }
    
    setenv("TZ", tzStr, 1);
    tzset();
    
    // Start SNTP
    sntp_init();
    
    // Wait for initial sync (non-blocking, up to 10 seconds)
    Serial.print("Waiting for SNTP sync");
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts < 20) {
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Check if time is valid (year > 2020)
        if (timeinfo.tm_year > (2020 - 1900)) {
            timeSynced = true;
            lastNTPSync = millis();
            
            // Get sync status
            sntp_sync_status_t status = sntp_get_sync_status();
            const char* statusStr = (status == SNTP_SYNC_STATUS_COMPLETED) ? "completed" : 
                                   (status == SNTP_SYNC_STATUS_IN_PROGRESS) ? "in progress" : "reset";
            
            Serial.println(" Success!");
            Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d (sync: %s)\n",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, statusStr);
            Serial.println("SNTP will auto-sync every 1 hour to compensate for RTC drift");
            
            if (eventLogger) {
                char timeStr[64];
                snprintf(timeStr, sizeof(timeStr), "Time synced via SNTP: %04d-%02d-%02d %02d:%02d:%02d",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                eventLogger->info("system", timeStr);
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
    Serial.println("WARNING: SNTP synchronization failed - schedules may not work correctly");
    Serial.println("SNTP will continue retrying in background...");
    
    if (eventLogger) {
        eventLogger->warning("system", "SNTP time synchronization failed (will retry)");
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

String WiFiManager::getTimeOnly() {
    if (!timeSynced) {
        return "--:--:--";
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    return String(timeStr);
}
