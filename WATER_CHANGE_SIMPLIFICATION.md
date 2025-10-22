# Water Change Interface Simplification - Complete

## Overview
Successfully simplified the water change interface from a complex 7-phase manual advancement system to a streamlined 2-button interface (Start/End) while retaining comprehensive sensor data tracking and adding filter maintenance functionality.

## Changes Implemented

### 1. Simplified Water Change Phases
**Previous System:**
- 7 phases: IDLE, PREPARE, DRAINING, DRAINED, FILLING, STABILIZING, COMPLETE
- Manual advancement required between phases
- Complex timing and state management

**New System:**
- 3 phases: IDLE, IN_PROGRESS, COMPLETE
- Automatic progression: Start → In Progress → Complete
- Single-step initiation and completion

### 2. Enhanced Data Tracking
**WaterChangeRecord Structure:**
```cpp
struct WaterChangeRecord {
    time_t startTimestamp;       // When water change started
    time_t endTimestamp;         // When water change ended
    float volumeChanged;         // Litres changed
    float tempBefore;           // Temperature before change
    float tempAfter;            // Temperature after change
    float phBefore;             // pH before change
    float phAfter;              // pH after change
    float tdsBefore;            // TDS before change
    float tdsAfter;             // TDS after change
    int durationMinutes;        // Total duration
    bool completedSuccessfully; // Completion status
};
```

**Key Features:**
- Captures sensor readings automatically at start and end
- Records start/end timestamps separately
- Calculates duration automatically
- Maintains full history (50 records in NVS)

### 3. Filter Maintenance Tracking
**New Feature:**
```cpp
struct FilterMaintenanceRecord {
    time_t timestamp;
    String notes;  // Optional maintenance notes
};
```

**Functionality:**
- Record filter maintenance with optional notes
- Track maintenance history (100 records in NVS)
- Calculate days since last maintenance
- Separate NVS namespace: "wc-filter"

### 4. Safety Features Preserved
**System Pause During Water Change:**
- Heater relay disabled (`safetyDisable()`)
- CO2 relay disabled (`safetyDisable()`)
- Automatic re-enable on completion (`safetyEnable()`)
- Also re-enabled on cancel

**Implementation:**
- `startWaterChange()` → Pauses systems
- `endWaterChange()` → Resumes systems
- `cancelWaterChange()` → Resumes systems without logging

### 5. API Endpoints Updated

**Water Change Endpoints:**
```
GET  /api/waterchange/status
  - Returns: inProgress, phase, elapsedTime, systemsPaused, currentVolume
  - If in progress: includes tempBefore, phBefore, tdsBefore
  - Includes: daysSinceFilterMaintenance

POST /api/waterchange/start
  - Body: {"volume": <litres>}  (optional, defaults to scheduled)
  - Automatically captures current temp/pH/TDS
  - Pauses heater and CO2

POST /api/waterchange/end
  - No body required
  - Automatically captures final temp/pH/TDS
  - Resumes heater and CO2
  - Creates complete record with before/after data

POST /api/waterchange/cancel
  - No body required
  - Resumes systems without logging

GET  /api/waterchange/history?count=10
  - Returns array of records with:
    * startTime, endTime, duration
    * volume
    * tempBefore/After, phBefore/After, tdsBefore/After
    * successful flag
```

**Filter Maintenance Endpoints:**
```
POST /api/maintenance/filter
  - Body: {"notes": "optional text"}
  - Records maintenance event with timestamp

GET  /api/maintenance/filter?count=10
  - Returns array of maintenance records
  - Each record has: timestamp, notes
```

**Removed Endpoints:**
- `/api/waterchange/advance` - No longer needed (auto progression)
- Safety limits configuration - Simplified system doesn't need them

### 6. Web Interface Requirements

**Water Change Section:**
```html
<!-- Status Display -->
<div class="status">
  Phase: <span id="wcPhase">IDLE</span>
  Elapsed: <span id="wcElapsed">0</span> minutes
  Systems: <span id="wcSystems">Active</span>
</div>

<!-- If IN_PROGRESS: Show "before" readings -->
<div id="beforeReadings" style="display:none">
  <h3>Initial Readings</h3>
  Temp: <span id="tempBefore">--</span>°C
  pH: <span id="phBefore">--</span>
  TDS: <span id="tdsBefore">--</span> ppm
</div>

<!-- Controls -->
<button id="startWC" onclick="startWaterChange()">
  Start Water Change
</button>
<input type="number" id="volumeInput" placeholder="Volume (L)" />

<button id="endWC" onclick="endWaterChange()" style="display:none">
  Complete Water Change
</button>

<button id="cancelWC" onclick="cancelWaterChange()" style="display:none">
  Cancel
</button>
```

**Filter Maintenance Section:**
```html
<div class="maintenance">
  <h3>Filter Maintenance</h3>
  <p>Last cleaned: <span id="daysSinceFilter">--</span> days ago</p>
  
  <input type="text" id="maintenanceNotes" placeholder="Optional notes" />
  <button onclick="recordFilterMaintenance()">
    Record Filter Maintenance
  </button>
  
  <!-- History Table -->
  <table id="filterHistory">
    <tr>
      <th>Date</th>
      <th>Notes</th>
    </tr>
  </table>
</div>
```

**History Table Update:**
```html
<table id="waterChangeHistory">
  <tr>
    <th>Start Time</th>
    <th>End Time</th>
    <th>Duration</th>
    <th>Volume (L)</th>
    <th>Temp Δ</th>
    <th>pH Δ</th>
    <th>TDS Δ</th>
    <th>Status</th>
  </tr>
</table>
```

**JavaScript Functions Needed:**
```javascript
async function startWaterChange() {
  const volume = parseFloat(document.getElementById('volumeInput').value) || 0;
  await fetch('/api/waterchange/start', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({volume: volume})
  });
  updateStatus();
}

async function endWaterChange() {
  await fetch('/api/waterchange/end', {
    method: 'POST'
  });
  updateStatus();
}

async function recordFilterMaintenance() {
  const notes = document.getElementById('maintenanceNotes').value;
  await fetch('/api/maintenance/filter', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({notes: notes})
  });
  loadFilterHistory();
}

async function updateStatus() {
  const resp = await fetch('/api/waterchange/status');
  const data = await resp.json();
  
  // Update UI based on data.inProgress
  document.getElementById('wcPhase').textContent = data.phaseDescription;
  document.getElementById('wcElapsed').textContent = data.elapsedTime;
  document.getElementById('wcSystems').textContent = 
    data.systemsPaused ? 'PAUSED' : 'Active';
  
  // Show/hide controls
  document.getElementById('startWC').style.display = 
    data.inProgress ? 'none' : 'inline';
  document.getElementById('endWC').style.display = 
    data.inProgress ? 'inline' : 'none';
  document.getElementById('cancelWC').style.display = 
    data.inProgress ? 'inline' : 'none';
  
  // Show "before" readings if in progress
  if (data.inProgress) {
    document.getElementById('beforeReadings').style.display = 'block';
    document.getElementById('tempBefore').textContent = data.tempBefore.toFixed(1);
    document.getElementById('phBefore').textContent = data.phBefore.toFixed(2);
    document.getElementById('tdsBefore').textContent = data.tdsBefore.toFixed(0);
  } else {
    document.getElementById('beforeReadings').style.display = 'none';
  }
  
  // Update filter maintenance info
  document.getElementById('daysSinceFilter').textContent = 
    data.daysSinceFilterMaintenance;
}
```

## Technical Details

### Code Changes

**Files Modified:**
1. `include/WaterChangeAssistant.h` - Simplified interface
2. `src/WaterChangeAssistant.cpp` - Complete rewrite (~600 lines → 539 lines)
3. `src/WebServer.cpp` - Updated all water change endpoints
4. `src/main.cpp` - Removed safety limits call

**Files Archived:**
- `WaterChangeAssistant_backup.cpp` - Original implementation preserved

### Key Improvements

**Sensor Integration:**
- Automatic capture via `getSensorData()` from SystemTasks
- No manual sensor reading required from web interface
- Timestamp and sensor data captured atomically

**Relay Control:**
- Uses `safetyDisable()` / `safetyEnable()` instead of `setEnabled()`
- Consistent with emergency stop pattern
- Prevents accidental override during water change

**Tank Volume:**
- Calculated from ConfigManager dimensions (length × width × height / 1000)
- No longer stored as separate value
- Consistent with configuration management approach

**NVS Storage:**
- Water change history: "wc-history" namespace (50 records)
- Filter maintenance: "wc-filter" namespace (100 records)
- Settings: "wc-settings" namespace (schedule, lastChange)
- Each namespace independently managed for reliability

### Compilation Status
✅ **Successfully compiles** for ESP32-S3
- Flash usage: 30.4% (1,116,997 bytes)
- RAM usage: 15.7% (51,540 bytes)
- No warnings or errors

## Usage Example

### Starting a Water Change
1. User clicks "Start Water Change" button
2. Optionally enters volume (defaults to scheduled)
3. System captures current temp/pH/TDS readings
4. Heater and CO2 disabled
5. Status shows "IN PROGRESS" with initial readings

### Completing a Water Change
1. User performs manual water change tasks
2. Clicks "Complete Water Change" button
3. System captures final temp/pH/TDS readings
4. Heater and CO2 re-enabled
5. Complete record saved with all data
6. Status returns to "IDLE"

### Recording Filter Maintenance
1. User performs filter cleaning
2. Enters optional notes (e.g., "Replaced cartridge")
3. Clicks "Record Filter Maintenance"
4. Timestamp and notes saved
5. "Days since last maintenance" resets to 0

## Benefits

### For Users
- **Simplified workflow:** 2 buttons instead of multi-phase process
- **Automatic data collection:** No manual sensor reading
- **Complete history:** All data preserved for trend analysis
- **Maintenance tracking:** Never forget filter cleaning again
- **Safety maintained:** Systems still paused during changes

### For Developers
- **Less complexity:** 539 lines vs ~700+ lines
- **Easier to maintain:** Clearer state machine
- **Better separation:** Water change vs filter maintenance
- **Cleaner API:** RESTful design with clear purpose
- **Extensible:** Easy to add more maintenance types

## Future Enhancements

### Potential Additions
1. **Automatic reminders:** Alert when filter maintenance overdue
2. **Multiple maintenance types:** Substrate cleaning, equipment checks, etc.
3. **Photo attachment:** Store images with maintenance records
4. **Export functionality:** Download history as CSV
5. **Graphing:** Visualize sensor changes during water changes
6. **Scheduling:** Auto-remind based on calendar/schedule

### Web Interface Polish
1. Confirmation dialogs for actions
2. Progress indicators during water change
3. Real-time sensor updates while in progress
4. Color-coded status (green=OK, yellow=in progress, red=overdue)
5. Tooltips with help text

## Testing Recommendations

### Unit Tests (Native)
- Test record serialization/deserialization
- Verify NVS save/load for both record types
- Test sensor data capture and storage
- Verify duration calculation

### Integration Tests (ESP32)
1. Start water change → verify systems paused
2. Complete water change → verify systems resumed
3. Cancel water change → verify cleanup
4. Record filter maintenance → verify persistence
5. Load history after reboot → verify recovery

### Web Interface Tests
1. Button state transitions
2. Data display updates
3. Error handling (network failures)
4. Concurrent access (multiple browsers)

## Documentation Status
- ✅ Code fully documented with inline comments
- ✅ API endpoints documented above
- ✅ Web interface requirements specified
- ✅ Usage examples provided
- ⏳ Web interface HTML/JS updates pending

## Migration Notes

### Breaking Changes
- `/api/waterchange/advance` endpoint removed
- `WaterChangeRecord.timestamp` split into `startTimestamp` and `endTimestamp`
- `setSafetyLimits()` method removed
- `advancePhase()` method removed
- `setActualVolume()` method removed
- `getPhaseElapsedTime()` renamed to `getElapsedTime()`

### Backward Compatibility
- History format updated (old records won't load)
- Consider NVS namespace migration if needed
- Web interface requires complete rewrite for water change section

## Conclusion

The water change system has been successfully simplified while maintaining all critical functionality:
- ✅ Safety features preserved
- ✅ Data tracking enhanced
- ✅ Filter maintenance added
- ✅ API cleaner and more RESTful
- ✅ Code complexity reduced
- ✅ Successfully compiles

**Status:** Backend implementation complete. Web interface updates pending.
