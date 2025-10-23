#include "NotificationManager.h"
#include "Logger.h"

NotificationManager::NotificationManager()
    : maxNotifications(100),
      cooldownMs(60000),  // 1 minute cooldown by default
      lastNotificationTime(0) {
}

void NotificationManager::begin() {
    LOG_INFO("NotifyMgr", "Notification manager initialized");
}

void NotificationManager::update() {
    // Periodic cleanup of old notifications
    static unsigned long lastCleanup = 0;
    unsigned long now = millis();
    
    if (now - lastCleanup > 300000) {  // Clean up every 5 minutes
        lastCleanup = now;
        pruneOldNotifications();
    }
}

void NotificationManager::notify(NotificationLevel level, const char* category, const char* message) {
    // Check cooldown to prevent notification spam
    String key = makeNotificationKey(level, category, message);
    if (isInCooldown(key)) {
        LOG_DEBUG("NotifyMgr", "Notification in cooldown, skipping: %s", message);
        return;
    }
    
    // Create notification
    Notification notif;
    notif.timestamp = millis();
    notif.level = level;
    notif.category = category;
    notif.message = message;
    notif.acknowledged = false;
    
    // Add to history
    notifications.push_back(notif);
    
    // Log the notification
    const char* levelStr[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    LOG_INFO("NotifyMgr", "[%s] %s: %s", levelStr[level], category, message);
    
    // Call all registered callbacks
    for (auto& callback : callbacks) {
        callback(notif);
    }
    
    // Update cooldown tracking
    lastNotificationKey = key;
    lastNotificationTime = millis();
    
    // Prune if we've exceeded max notifications
    if (notifications.size() > maxNotifications) {
        pruneOldNotifications();
    }
}

void NotificationManager::info(const char* category, const char* message) {
    notify(NOTIFY_INFO, category, message);
}

void NotificationManager::warning(const char* category, const char* message) {
    notify(NOTIFY_WARNING, category, message);
}

void NotificationManager::error(const char* category, const char* message) {
    notify(NOTIFY_ERROR, category, message);
}

void NotificationManager::critical(const char* category, const char* message) {
    notify(NOTIFY_CRITICAL, category, message);
}

void NotificationManager::clearNotifications() {
    notifications.clear();
    LOG_INFO("NotifyMgr", "All notifications cleared");
}

void NotificationManager::acknowledgeNotification(size_t index) {
    if (index < notifications.size()) {
        notifications[index].acknowledged = true;
        LOG_DEBUG("NotifyMgr", "Notification %zu acknowledged", index);
    }
}

void NotificationManager::acknowledgeAll() {
    for (auto& notif : notifications) {
        notif.acknowledged = true;
    }
    LOG_INFO("NotifyMgr", "All notifications acknowledged");
}

std::vector<Notification> NotificationManager::getNotificationsByLevel(NotificationLevel level) const {
    std::vector<Notification> filtered;
    for (const auto& notif : notifications) {
        if (notif.level == level) {
            filtered.push_back(notif);
        }
    }
    return filtered;
}

std::vector<Notification> NotificationManager::getNotificationsByCategory(const char* category) const {
    std::vector<Notification> filtered;
    String cat = category;
    for (const auto& notif : notifications) {
        if (notif.category == cat) {
            filtered.push_back(notif);
        }
    }
    return filtered;
}

std::vector<Notification> NotificationManager::getUnacknowledged() const {
    std::vector<Notification> filtered;
    for (const auto& notif : notifications) {
        if (!notif.acknowledged) {
            filtered.push_back(notif);
        }
    }
    return filtered;
}

size_t NotificationManager::getUnacknowledgedCount() const {
    size_t count = 0;
    for (const auto& notif : notifications) {
        if (!notif.acknowledged) {
            count++;
        }
    }
    return count;
}

void NotificationManager::addCallback(NotificationCallback callback) {
    callbacks.push_back(callback);
    LOG_DEBUG("NotifyMgr", "Notification callback registered");
}

void NotificationManager::pruneOldNotifications() {
    // Keep only the most recent maxNotifications
    if (notifications.size() > maxNotifications) {
        size_t toRemove = notifications.size() - maxNotifications;
        notifications.erase(notifications.begin(), notifications.begin() + toRemove);
        LOG_DEBUG("NotifyMgr", "Pruned %zu old notifications", toRemove);
    }
}

String NotificationManager::makeNotificationKey(NotificationLevel level, const char* category, const char* message) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%d:%s:%s", level, category, message);
    return String(buf);
}

bool NotificationManager::isInCooldown(const String& key) {
    if (key != lastNotificationKey) {
        return false;  // Different notification
    }
    
    unsigned long now = millis();
    return (now - lastNotificationTime) < cooldownMs;
}
