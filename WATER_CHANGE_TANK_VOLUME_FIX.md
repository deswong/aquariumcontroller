# Water Change Tank Volume Fix

## Problem
The WaterChangeAssistant was storing its own copy of tank volume (defaulting to 75L) in a separate NVS namespace, instead of using the authoritative tank dimensions from ConfigManager. This caused:

1. **Tank volume always defaulting to 75L** - Even when proper dimensions were configured in ConfigManager
2. **Incorrect water change volume calculations** - Volume changed during water changes was based on wrong tank size
3. **Duplicate data storage** - Tank volume stored in two places (ConfigManager dimensions + WaterChangeAssistant volume)
4. **Hard-coded override in main.cpp** - Line 236 forced tank volume to 75L on every boot

## Root Cause
- `WaterChangeAssistant` had its own `tankVolumeLitres` member variable
- Stored separately in NVS namespace "waterchange" with key "tankVol"
- main.cpp called `waterChangeAssistant->setTankVolume(75.0)` after initialization, overriding any saved value
- No connection to ConfigManager's authoritative tank dimensions

## Solution
Changed `WaterChangeAssistant` to **calculate tank volume from ConfigManager dimensions** instead of storing its own copy.

### Files Modified

#### `include/WaterChangeAssistant.h`
**Added:**
- Forward declaration of `ConfigManager`
- `ConfigManager* configMgr;` member variable
- `void begin(ConfigManager* config);` overload to accept ConfigManager
- `void setConfigManager(ConfigManager* config);` method
- Changed `float getTankVolume()` from inline return to method declaration

**Removed:**
- `float tankVolumeLitres;` member variable
- `void setTankVolume(float litres);` method

#### `src/WaterChangeAssistant.cpp`
**Added:**
- `#include "ConfigManager.h"` at top of file
- `configMgr(nullptr)` in constructor initialization list
- `begin(ConfigManager* config)` implementation
- `setConfigManager(ConfigManager* config)` implementation
- `getTankVolume()` implementation that calculates volume from ConfigManager dimensions

**getTankVolume() Implementation:**
```cpp
float WaterChangeAssistant::getTankVolume() {
    if (configMgr) {
        // Calculate volume from ConfigManager dimensions (L x W x H in cm)
        SystemConfig& config = configMgr->getConfig();
        float volumeCubicCm = config.tankLength * config.tankWidth * config.tankHeight;
        float volumeLitres = volumeCubicCm / 1000.0; // Convert cm³ to litres
        
        if (volumeLitres > 0) {
            return volumeLitres;
        }
    }
    
    // Fallback to default if ConfigManager not set or dimensions not configured
    Serial.println("WARNING: Tank dimensions not configured, using default 75L");
    return 75.0;
}
```

**Removed:**
- `tankVolumeLitres(75.0)` from constructor initialization
- `setTankVolume()` method implementation
- `tankVolumeLitres` from `loadSettings()` (no longer reads from NVS)
- `tankVolumeLitres` from `saveSettings()` (no longer writes to NVS)

**Changed:**
- All references to `tankVolumeLitres` replaced with `getTankVolume()` calls
- `begin()` now prints calculated volume: `getTankVolume()`
- `startWaterChange()` validation uses `getTankVolume()`
- Volume percentage calculations use `getTankVolume()`

#### `src/main.cpp`
**Changed:**
```cpp
// OLD:
waterChangeAssistant = new WaterChangeAssistant();
waterChangeAssistant->begin();
waterChangeAssistant->setTankVolume(75.0); // This was forcing 75L!
waterChangeAssistant->setSchedule(SCHEDULE_WEEKLY, 25.0);

// NEW:
waterChangeAssistant = new WaterChangeAssistant();
waterChangeAssistant->begin(configMgr); // Pass ConfigManager
waterChangeAssistant->setSchedule(SCHEDULE_WEEKLY, 25.0); // Only if not saved in NVS
```

**Removed:**
- `waterChangeAssistant->setTankVolume(75.0);` line that was overriding saved values

#### `src/WebServer.cpp`
**Changed:**
The `/api/waterchange/tank` endpoint that previously called `setTankVolume()` is now deprecated:

```cpp
// OLD: waterChangeAssistant->setTankVolume(tankVolume);

// NEW: Returns deprecation message
request->send(200, "application/json", 
    "{\"status\":\"ok\",\"message\":\"Tank volume is now configured via tank dimensions in settings\"}");
Serial.printf("WARNING: Deprecated tankVolume API called. Use tank dimensions in ConfigManager.\n");
```

Users should now set tank dimensions via the main settings page, which updates ConfigManager.

## How It Works Now

### Tank Volume Calculation Flow
1. User sets tank dimensions in web UI settings (length × width × height in cm)
2. ConfigManager stores dimensions in NVS namespace "aquarium-cfg"
3. WaterChangeAssistant calculates volume on-demand: `(L × W × H) / 1000` litres
4. Water change volumes are calculated as: `getTankVolume() × percentageVolume`

### Example
- Tank dimensions: 100cm × 50cm × 50cm = 250,000 cm³ = **250 litres**
- 25% water change: 250L × 0.25 = **62.5 litres**
- Previously would incorrectly use: 75L × 0.25 = 18.75 litres ❌

## Benefits

✅ **Single source of truth** - Tank volume calculated from ConfigManager dimensions only  
✅ **Accurate calculations** - Water change volumes based on actual configured tank size  
✅ **No hard-coded overrides** - Respects user-configured dimensions  
✅ **Reduced NVS usage** - No duplicate tankVol storage in waterchange namespace  
✅ **Backward compatible** - Falls back to 75L default if dimensions not configured  

## Migration Notes

### For Existing Users
- Old `tankVol` value in waterchange namespace is now ignored (safe to leave, won't be used)
- **Action required**: Set proper tank dimensions in Settings page
- Tank dimensions from ConfigManager will be used immediately
- No data loss - all water change history preserved

### For New Users
- Set tank dimensions in Settings page first
- Water change volumes will automatically use correct tank size
- No need to separately configure "tank volume" for water changes

## Testing Recommendations

1. **Set tank dimensions** in web UI settings (e.g., 100 × 50 × 50 cm)
2. **Verify calculation** via serial output: "Tank volume: 250.0 litres"
3. **Check water change volume**: 25% of 250L = 62.5L (not 18.75L from 75L)
4. **Test water change**: Start water change, verify volume is correct
5. **Test reboot persistence**: Restart ESP32, verify volume still calculated correctly

## API Changes

### Deprecated
- `POST /api/waterchange/tank` with `{"tankVolume": 100}` - Now returns deprecation message

### Recommended
- Use `POST /api/settings` with tank dimensions:
  ```json
  {
    "tankLength": 100,
    "tankWidth": 50,
    "tankHeight": 50
  }
  ```

## Serial Output

**On startup:**
```
Water Change Assistant initialized
Tank volume: 250.0 litres
Schedule: 7 days, 25.0% volume
```

**If dimensions not set:**
```
WARNING: Tank dimensions not configured, using default 75L
Tank volume: 75.0 litres
```

**During water change:**
```
=== Water Change Started ===
Volume: 62.5 litres (25.0%)
Temperature before: 24.5°C
pH before: 7.2
Systems paused for safety
===========================
```

## Configuration Example

**Via Web UI Settings:**
1. Navigate to Settings tab
2. Enter tank dimensions:
   - Length: 100 cm
   - Width: 50 cm  
   - Height: 50 cm
3. Click "Save Settings"
4. Tank volume = 100 × 50 × 50 / 1000 = **250 litres**

**Via API:**
```bash
curl -X POST http://192.168.1.100/api/settings \
  -H "Content-Type: application/json" \
  -d '{
    "tankLength": 100,
    "tankWidth": 50,
    "tankHeight": 50
  }'
```

## Deployment Status

✅ **Code changes complete**  
✅ **Firmware compiled successfully**  
⏳ **Ready for upload to ESP32**  
⏳ **Testing pending**  

## Next Steps

1. Upload firmware to ESP32
2. Set tank dimensions in web UI
3. Verify correct tank volume calculation via serial monitor
4. Test water change with correct volume
5. Verify persistence across reboots
