# Water Change Schedule Information Fix

## Problem
The scheduled volume displayed in the Water Change tab was not reflecting the actual saved settings. The display showed:
1. **Hard-coded 25%** volume percentage
2. **Calculated volume** based on 25% assumption (e.g., 250L × 25% = 62.5L)
3. Did not update when user changed schedule settings

This meant users couldn't see their actual scheduled volume percentage or the correct litres to change.

## Root Cause
The web UI was **calculating** the scheduled volume instead of **fetching** it from the backend:

**Before:**
```javascript
// Hard-coded 25% assumption
const scheduledVol = (status.tankVolume * 0.25).toFixed(1);
document.getElementById('wc-scheduled-volume').textContent = scheduledVol;
```

**Issues:**
1. Always showed 25%, even if user set 10%, 30%, or 50%
2. Volume percent input field not updated when loading page
3. Schedule settings saved to NVS but not displayed correctly
4. Confusing for users - settings saved but display showed wrong values

## Solution
Enhanced the API to return the actual saved schedule settings and updated the UI to display them correctly.

### Changes Made

#### 1. Added Getter Method
**`include/WaterChangeAssistant.h`**
```cpp
float getScheduledVolumePercent() { return scheduledVolumePercent; }
```

#### 2. Enhanced API Response
**`src/WebServer.cpp` - `/api/waterchange/status` endpoint**

Added two new fields to the status response:
```cpp
doc["scheduledVolumePercent"] = waterChangeAssistant->getScheduledVolumePercent();
doc["scheduledVolume"] = waterChangeAssistant->getScheduledChangeVolume();
```

**API Response Now Includes:**
```json
{
  "tankVolume": 250.0,
  "scheduledVolumePercent": 30.0,
  "scheduledVolume": 75.0,
  "schedule": 7,
  ...
}
```

#### 3. Updated Web UI
**`data/index.html` - `updateWaterChangeStatus()` function**

**Changed:**
```javascript
// OLD: Hard-coded calculation
const scheduledVol = (status.tankVolume * 0.25).toFixed(1);

// NEW: Use actual value from API
document.getElementById('wc-scheduled-volume').textContent = status.scheduledVolume.toFixed(1);
```

**Also Added:**
```javascript
// Update the volume percent input field to show saved value
document.getElementById('wc-volume-percent').value = status.scheduledVolumePercent;
```

## How It Works Now

### Data Flow
1. **User sets schedule**: "Weekly, 30% volume"
2. **API saves to NVS**: `setSchedule(SCHEDULE_WEEKLY, 30.0)` → Saved to NVS
3. **On page load/refresh**: 
   - Fetch `/api/waterchange/status`
   - Returns `scheduledVolumePercent: 30.0` and `scheduledVolume: 75.0`
   - UI displays: "30%" and "75.0 litres"
4. **Settings persist**: Across reboots, tab changes, page refreshes

### Example Scenarios

#### Scenario 1: 250L Tank, 30% Weekly
**Settings:**
- Tank: 250L
- Schedule: Weekly
- Volume: 30%

**Display Shows:**
- Tank Volume: **250.0 litres** ✓
- Scheduled Volume: **75.0 litres** ✓ (250L × 30%)
- Volume Percent Input: **30** ✓

#### Scenario 2: 100L Tank, 50% Bi-weekly
**Settings:**
- Tank: 100L
- Schedule: Bi-weekly
- Volume: 50%

**Display Shows:**
- Tank Volume: **100.0 litres** ✓
- Scheduled Volume: **50.0 litres** ✓ (100L × 50%)
- Volume Percent Input: **50** ✓

#### Scenario 3: After Changing Settings
**Action:** User changes volume from 25% to 40%
1. Click "Save Schedule"
2. Settings saved to NVS
3. Status refreshes automatically
4. Display updates: **40%** and calculated litres ✓

## Benefits

✅ **Accurate display** - Shows actual saved settings, not assumptions  
✅ **Reflects reality** - What you see is what's saved  
✅ **Persists correctly** - Survives reboots and tab changes  
✅ **Better UX** - Input fields show current values when page loads  
✅ **No confusion** - Settings and display always in sync  
✅ **Flexible scheduling** - Any percentage works correctly (10-50%)  

## API Enhancement

### GET /api/waterchange/status

**New Fields Added:**
- `scheduledVolumePercent` (float) - The saved percentage (e.g., 30.0)
- `scheduledVolume` (float) - Calculated litres (tankVolume × percent / 100)

**Complete Response:**
```json
{
  "inProgress": false,
  "phase": 0,
  "phaseDescription": "Idle",
  "phaseElapsed": 0,
  "systemsPaused": false,
  "currentVolume": 0,
  "tankVolume": 250.0,
  "schedule": 7,
  "scheduledVolumePercent": 30.0,
  "scheduledVolume": 75.0,
  "daysSinceLastChange": 3,
  "daysUntilNextChange": 4,
  "isOverdue": false
}
```

## Testing Recommendations

1. **Set custom schedule**: Change volume percent to non-default value (e.g., 35%)
2. **Save and verify**: Check display shows "35%" and calculated litres
3. **Refresh page**: Verify values persist after page reload
4. **Reboot ESP32**: Verify settings survive reboot
5. **Change tank dimensions**: Verify scheduled volume recalculates (e.g., 250L → 100L updates from 87.5L → 35.0L)
6. **Multiple percentage values**: Test 10%, 25%, 40%, 50%
7. **Check history**: Verify completion dialog still shows correct scheduled volume

## Before vs After

### Before Fix
```
┌─────────────────────────────┐
│ Tank Volume: 250.0 litres   │
│ Scheduled Volume: 62.5 L    │ ← Always 25%! Wrong!
│                             │
│ Volume Percentage: [30]     │ ← User's actual setting
│ [Save Schedule]             │
└─────────────────────────────┘
Display: 62.5L (25%)
Actual Setting: 30%
Result: Confusing! ❌
```

### After Fix
```
┌─────────────────────────────┐
│ Tank Volume: 250.0 litres   │
│ Scheduled Volume: 75.0 L    │ ← Correct 30%!
│                             │
│ Volume Percentage: [30]     │ ← Matches display
│ [Save Schedule]             │
└─────────────────────────────┘
Display: 75.0L (30%)
Actual Setting: 30%
Result: Accurate! ✓
```

## Technical Notes

### Volume Calculation
```cpp
// WaterChangeAssistant::getScheduledChangeVolume()
float WaterChangeAssistant::getScheduledChangeVolume() {
    return getTankVolume() * (scheduledVolumePercent / 100.0);
}
```

### Persistence
The `scheduledVolumePercent` is:
- Saved in `WaterChangeAssistant::saveSettings()`
- Loaded in `WaterChangeAssistant::loadSettings()`
- Stored in NVS namespace: `"waterchange"`, key: `"volumePct"`
- Retrieved via getter: `getScheduledVolumePercent()`

### Dynamic Updates
The volume automatically recalculates when:
- Tank dimensions change (ConfigManager updates)
- Volume percentage changes (schedule settings update)
- Both values are always in sync

## Files Modified
- `include/WaterChangeAssistant.h` - Added getter method
- `src/WebServer.cpp` - Enhanced status API response
- `data/index.html` - Updated UI to use API values

## Deployment Status
✅ **Code changes complete**  
✅ **HTML regenerated** (153KB → 22KB compressed)  
✅ **Firmware compiled successfully** (61.9% flash usage)  
⏳ **Ready for upload to ESP32**  

## Next Steps
1. Upload firmware to ESP32
2. Test schedule settings with various percentages
3. Verify display matches saved settings
4. Test persistence across reboots
5. Verify volume recalculates with dimension changes
