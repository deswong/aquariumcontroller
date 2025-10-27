# NVS Monitoring and Management Features - Implementation Complete

## ✅ Overview

Added comprehensive NVS (Non-Volatile Storage) monitoring and management capabilities to the Aquarium Controller, accessible via web interface, API endpoints, and serial commands.

---

## 🎯 Features Implemented

### 1. **NVS Statistics and Monitoring**
- Real-time NVS usage tracking
- Health status indicators (healthy, elevated, high, critical)
- Automatic alerts based on usage thresholds
- Integration with SystemMonitor for continuous monitoring

### 2. **Web API Endpoints**
Three new REST API endpoints for NVS management:

#### GET `/api/nvs/stats`
Returns detailed NVS statistics in JSON format:
```json
{
  "success": true,
  "used_entries": 145,
  "free_entries": 213,
  "total_entries": 358,
  "namespace_count": 15,
  "usage_percent": 40.5,
  "healthy": true,
  "status": "healthy",
  "recommendation": "Normal operation. NVS usage is healthy.",
  "alert_level": "normal",
  "icon": "✅"
}
```

#### GET `/api/nvs/health`
Quick health check endpoint:
```json
{
  "healthy": true,
  "usage_percent": 40.5,
  "status": "healthy",
  "recommendation": "Normal operation. NVS usage is healthy."
}
```

#### POST `/api/nvs/erase`
Erase all NVS data (DANGEROUS - requires confirmation):
```json
{
  "confirmation": "ERASE_ALL_NVS"
}
```

**Response on success:**
```json
{
  "status": "ok",
  "message": "NVS erased successfully. Device will restart."
}
```

### 3. **Serial Commands**
New serial monitor commands for debugging and maintenance:

| Command | Description |
|---------|-------------|
| `help` | Show all available commands |
| `nvs-stats` | Display detailed NVS statistics |
| `nvs-erase` | Erase NVS with interactive confirmation |
| `sys-info` | Display system information |
| `heap-info` | Display heap memory stats |
| `task-info` | Display FreeRTOS task info |
| `restart` | Restart the device |

### 4. **Automatic Monitoring**
SystemMonitor now includes NVS health checks:
- Runs every 60 seconds (configurable)
- Logs warnings when usage exceeds thresholds
- Generates notifications for critical states

---

## 📊 Health Status Levels

| Usage | Status | Alert Level | Icon | Action |
|-------|--------|-------------|------|--------|
| < 70% | Healthy | Normal | ✅ | None required |
| 70-79% | Elevated | Warning | ⚠️ | Monitor usage |
| 80-89% | High | High | ⚠️ | Consider clearing old data |
| ≥ 90% | Critical | Critical | ⚠️ | Urgent: Clear data or erase NVS |

---

## 🔧 Files Added/Modified

### New Files Created:
1. **`include/NVSHelper.h`** - NVS management class header
2. **`src/NVSHelper.cpp`** - NVS management implementation

### Modified Files:
3. **`src/WebServer.cpp`** 
   - Added `/api/nvs/stats`, `/api/nvs/health`, `/api/nvs/erase` endpoints
   - Integrated with EventLogger for audit trail
   
4. **`include/SystemMonitor.h`**
   - Added `checkNVSHealth()` and `printNVSHealth()` methods
   - Added `nvsWarningPercent` threshold
   
5. **`src/SystemMonitor.cpp`**
   - Implemented NVS health checking
   - Integrated into periodic monitoring cycle
   
6. **`src/main.cpp`**
   - Added `handleSerialCommands()` function
   - Integrated serial command processing in main loop
   - Added NVSHelper include

---

## 💻 Usage Examples

### Web Interface Integration (JavaScript)

```javascript
// Get NVS statistics
async function getNVSStats() {
    const response = await fetch('/api/nvs/stats');
    const data = await response.json();
    
    if (data.success) {
        console.log(`NVS Usage: ${data.usage_percent}%`);
        console.log(`Status: ${data.status}`);
        console.log(`Recommendation: ${data.recommendation}`);
        
        // Update UI
        document.getElementById('nvs-usage').textContent = 
            `${data.usage_percent.toFixed(1)}%`;
        document.getElementById('nvs-status').textContent = 
            `${data.icon} ${data.status}`;
    }
}

// Check NVS health
async function checkNVSHealth() {
    const response = await fetch('/api/nvs/health');
    const data = await response.json();
    
    if (!data.healthy) {
        alert(`NVS Warning: ${data.recommendation}`);
    }
}

// Erase NVS (with confirmation dialog)
async function eraseNVS() {
    const confirmed = confirm(
        'WARNING: This will ERASE ALL configuration data!\n\n' +
        'Are you absolutely sure you want to continue?'
    );
    
    if (!confirmed) return;
    
    const response = await fetch('/api/nvs/erase', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ confirmation: 'ERASE_ALL_NVS' })
    });
    
    const data = await response.json();
    alert(data.message || data.error);
    
    if (data.status === 'ok') {
        // Device will restart
        setTimeout(() => location.reload(), 5000);
    }
}

// Poll NVS health every 5 minutes
setInterval(checkNVSHealth, 5 * 60 * 1000);
```

### Serial Monitor Usage

```
=== Serial Commands Available ===
  help        - Show this help
  nvs-stats   - Display NVS statistics
  nvs-erase   - Erase all NVS data (requires confirmation)
  sys-info    - Display system information
  restart     - Restart the device
===================================

> nvs-stats

╔════════════════════════════════════════╗
║       NVS Flash Statistics             ║
╠════════════════════════════════════════╣
║ Used Entries:     145                  ║
║ Free Entries:     213                  ║
║ Total Entries:    358                  ║
║ Namespace Count:  15                   ║
║ Usage:            40.5%                ║
╠════════════════════════════════════════╣
║ Status: ✅ HEALTHY - Normal Operation ║
║ Action: None required                  ║
╚════════════════════════════════════════╝
```

### C++ Code Usage

```cpp
#include "NVSHelper.h"

// Get statistics
NVSHelper::NVSStats stats = NVSHelper::getStats();
Serial.printf("NVS Usage: %.1f%%\n", stats.usagePercent);

// Print formatted stats
NVSHelper::printStats();

// Check health
if (!NVSHelper::isHealthy()) {
    Serial.println("WARNING: NVS usage high!");
}

// Get JSON for web interface
String json = NVSHelper::getStatsJSON();
server.send(200, "application/json", json);

// Erase (DANGEROUS!)
if (userConfirmed) {
    bool success = NVSHelper::eraseAllWithConfirmation("ERASE_ALL_NVS");
    if (success) {
        ESP.restart();
    }
}
```

---

## 🔒 Safety Features

### NVS Erase Protection

1. **Double Confirmation Required**
   - Web API requires exact string: `"ERASE_ALL_NVS"`
   - Serial command requires typing `"YES"` within 10 seconds
   
2. **Audit Logging**
   - All erase operations logged to EventLogger
   - Timestamp and source (web/serial) recorded
   
3. **Automatic Restart**
   - Device restarts after successful erase
   - Ensures clean re-initialization
   
4. **Graceful Shutdown**
   - Publishes MQTT offline status before restart
   - Closes event logs properly

### Error Handling

```cpp
// All operations check for errors
NVSHelper::NVSStats stats = NVSHelper::getStats();
if (stats.status == "error") {
    Serial.println("ERROR: Could not read NVS");
    // Handle error...
}

// Erase with error checking
bool success = NVSHelper::eraseAll();
if (!success) {
    Serial.println("ERROR: NVS erase failed");
    // Retry or alert user...
}
```

---

## 📈 SystemMonitor Integration

NVS health is now monitored automatically:

```cpp
void SystemMonitor::update() {
    // ... existing checks ...
    
    // Check NVS health (every 60 seconds)
    checkNVSHealth();
}

void SystemMonitor::checkNVSHealth() {
    NVSHelper::NVSStats stats = NVSHelper::getStats();
    
    if (stats.usagePercent >= 90.0f) {
        LOG_ERROR("SysMon", "CRITICAL: NVS nearly full!");
        // Generate notification
    } else if (stats.usagePercent >= 80.0f) {
        LOG_WARN("SysMon", "WARNING: NVS usage high");
    } else if (stats.usagePercent >= nvsWarningPercent) {
        LOG_WARN("SysMon", "NVS usage elevated");
    }
}
```

**Configurable threshold:**
```cpp
sysMonitor->setNVSWarningThreshold(70.0);  // Alert at 70%
```

---

## 🎨 Web UI Integration Suggestions

### Dashboard Widget Example

```html
<div class="nvs-status-card">
    <h3>NVS Storage</h3>
    <div class="progress-bar">
        <div id="nvs-progress" style="width: 40.5%"></div>
    </div>
    <div id="nvs-usage">40.5% used</div>
    <div id="nvs-status">✅ Healthy</div>
    <button onclick="getNVSStats()">Refresh</button>
    <button onclick="eraseNVS()" class="danger">Erase NVS</button>
</div>
```

### Settings Page Section

```html
<div class="settings-section">
    <h2>⚙️ System Maintenance</h2>
    
    <div class="setting-item">
        <label>NVS Storage Status:</label>
        <div id="nvs-info">
            <span id="nvs-used">145</span> / 
            <span id="nvs-total">358</span> entries
            (<span id="nvs-percent">40.5%</span>)
        </div>
    </div>
    
    <div class="setting-item">
        <label>Health:</label>
        <span id="nvs-health-badge">✅ Healthy</span>
    </div>
    
    <div class="setting-item">
        <label>Recommendation:</label>
        <p id="nvs-recommendation">Normal operation. NVS usage is healthy.</p>
    </div>
    
    <div class="danger-zone">
        <h3>⚠️ Danger Zone</h3>
        <button onclick="eraseNVS()" class="btn-danger">
            Erase All Configuration Data
        </button>
        <p class="warning-text">
            This will permanently delete all settings, calibration data, 
            and history. Device will restart.
        </p>
    </div>
</div>
```

---

## 🧪 Testing Commands

### Test NVS Statistics
```bash
# Via serial monitor
nvs-stats

# Via curl
curl http://192.168.1.100/api/nvs/stats
```

### Test NVS Health
```bash
curl http://192.168.1.100/api/nvs/health
```

### Test NVS Erase (BE CAREFUL!)
```bash
curl -X POST http://192.168.1.100/api/nvs/erase \
  -H "Content-Type: application/json" \
  -d '{"confirmation":"ERASE_ALL_NVS"}'
```

---

## 📝 Logging Examples

### Normal Operation
```
[INFO] SysMon: NVS health: 40.5% used (145/358 entries, 15 namespaces)
```

### Warning State
```
[WARN] SysMon: NVS usage elevated: 72.3% (259/358 entries)
```

### Critical State
```
[ERROR] SysMon: CRITICAL: NVS nearly full! 91.6% used (328/358 entries)
[ERROR] SysMon: Action required: Clear old data or erase NVS
```

### Erase Operation
```
[WARNING] system: NVS erase requested - all configuration will be lost
╔════════════════════════════════════════╗
║   NVS ERASE REQUESTED VIA WEB API     ║
╚════════════════════════════════════════╝
✓ NVS erased successfully
[WARNING] system: NVS erased - restarting device
```

---

## 🔍 Troubleshooting

### Problem: NVS stats show "error"
**Solution:** NVS may not be initialized. Check `nvs_flash_init()` in main.cpp

### Problem: Usage always shows 0%
**Solution:** Ensure device has written some configuration first

### Problem: Can't erase NVS
**Possible causes:**
1. Wrong confirmation string (must be exactly "ERASE_ALL_NVS")
2. NVS partition corrupted
3. Flash write protection enabled

**Recovery:**
```bash
# Via esptool
python -m esptool --chip esp32s3 --port /dev/ttyUSB0 erase_region 0x9000 0x8000

# Or full erase
pio run -t erase
pio run -t upload
```

---

## 📚 API Reference Summary

### NVSHelper Class

#### Public Methods:
- `static NVSStats getStats()` - Get current statistics
- `static void printStats()` - Print formatted statistics
- `static String getStatsJSON()` - Get JSON string
- `static bool isHealthy()` - Check if usage < 70%
- `static float getUsagePercent()` - Get usage percentage
- `static bool eraseAll()` - Erase NVS (no confirmation)
- `static bool eraseAllWithConfirmation(String)` - Erase with confirmation
- `static String getHealthStatus(float)` - Get status string
- `static String getRecommendation(float)` - Get recommendation

#### NVSStats Structure:
```cpp
struct NVSStats {
    uint32_t usedEntries;
    uint32_t freeEntries;
    uint32_t totalEntries;
    uint32_t namespaceCount;
    float usagePercent;
    bool healthy;
    String status;  // "healthy", "elevated", "high", "critical", "error"
};
```

---

## ✅ Build Results

**Latest build:**
```
RAM:   [==        ]  15.6% (used 51,196 bytes from 327,680 bytes)
Flash: [===       ]  32.4% (used 1,167,625 bytes from 3,604,480 bytes)
```

**Added code size:** ~12 KB (NVSHelper + monitoring integration)

**Status:** ✅ **BUILD SUCCESSFUL**

---

## 🎯 Summary

All NVS monitoring and management features have been successfully implemented:

✅ Real-time NVS statistics and health monitoring  
✅ Web API endpoints (`/api/nvs/*`)  
✅ Serial command interface  
✅ SystemMonitor integration  
✅ Automatic health checks and alerts  
✅ Safe erase functionality with confirmations  
✅ Complete error handling  
✅ Production-ready and tested  

The system now provides comprehensive visibility into NVS storage usage and easy recovery mechanisms when needed.
