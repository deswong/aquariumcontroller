#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>

// Notification severity levels
enum NotificationLevel {
    NOTIFY_INFO,
    NOTIFY_WARNING,
    NOTIFY_ERROR,
    NOTIFY_CRITICAL
};

// Notification structure
struct Notification {
    unsigned long timestamp;
    NotificationLevel level;
    String category;      // e.g., "temperature", "ph", "network"
    String message;
    bool acknowledged;
};

// Notification callback type
typedef std::function<void(const Notification&)> NotificationCallback;

class NotificationManager {
public:
    NotificationManager();
    
    void begin();
    void update();
    
    // Send notifications
    void notify(NotificationLevel level, const char* category, const char* message);
    void info(const char* category, const char* message);
    void warning(const char* category, const char* message);
    void error(const char* category, const char* message);
    void critical(const char* category, const char* message);
    
    // Notification history
    const std::vector<Notification>& getNotifications() const { return notifications; }
    void clearNotifications();
    void acknowledgeNotification(size_t index);
    void acknowledgeAll();
    
    // Notification filtering
    std::vector<Notification> getNotificationsByLevel(NotificationLevel level) const;
    std::vector<Notification> getNotificationsByCategory(const char* category) const;
    std::vector<Notification> getUnacknowledged() const;
    
    // Callbacks for external integrations (MQTT, Web, etc.)
    void addCallback(NotificationCallback callback);
    
    // Configuration
    void setMaxNotifications(size_t max) { maxNotifications = max; }
    void setNotificationCooldown(unsigned long ms) { cooldownMs = ms; }
    
    // Statistics
    size_t getNotificationCount() const { return notifications.size(); }
    size_t getUnacknowledgedCount() const;
    
private:
    std::vector<Notification> notifications;
    std::vector<NotificationCallback> callbacks;
    
    size_t maxNotifications;
    unsigned long cooldownMs;
    unsigned long lastNotificationTime;
    String lastNotificationKey;
    
    void pruneOldNotifications();
    String makeNotificationKey(NotificationLevel level, const char* category, const char* message);
    bool isInCooldown(const String& key);
};

#endif // NOTIFICATION_MANAGER_H
