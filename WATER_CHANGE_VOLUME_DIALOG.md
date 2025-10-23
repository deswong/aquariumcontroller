# ⚠️ OUTDATED - Water Change Volume Recording

> **Notice:** This document describes an older multi-phase water change system. The system has been **significantly simplified** to a 2-button interface where volume is entered at completion.
> 
> **See:** [WATER_CHANGE_SIMPLIFICATION.md](WATER_CHANGE_SIMPLIFICATION.md) for current implementation.

## Current Implementation (Updated October 2025)

The water change system now uses a simplified flow:

1. **Start:** User clicks "Start Water Change" (no volume entered)
2. **In Progress:** Systems pause, user performs water change
3. **Complete:** User enters **actual volume changed** and clicks "Complete"
4. **Volume is recorded at END** of water change, not at start

### Why This Changed
- More accurate: Records what actually happened, not what was planned
- Realistic data: User knows exact volume after completing the change
- Simpler UX: No multi-phase advancement, just Start → Complete
- Flexible: Can change more or less than scheduled amount

## Historical Context (Archived)

### Old Multi-Phase System (Removed)
The previous implementation had 7 phases (PREPARE → DRAINING → DRAINED → FILLING → STABILIZING → COMPLETE) with manual advancement between each phase. This was overly complex for a simple water change operation.

## Implementation Details

### Files Modified

#### `include/WaterChangeAssistant.h`
**Added:**
- `void setActualVolume(float volumeLitres);` - Method to update the actual volume during water change

#### `src/WaterChangeAssistant.cpp`
**Added:**
```cpp
void WaterChangeAssistant::setActualVolume(float volumeLitres) {
    if (currentPhase != PHASE_IDLE && currentPhase != PHASE_COMPLETE) {
        currentChangeVolume = volumeLitres;
        Serial.printf("Actual water change volume updated to: %.1f litres\n", volumeLitres);
    } else {
        Serial.println("WARNING: Cannot set volume - no water change in progress");
    }
}
```

#### `src/WebServer.cpp`
**Modified:** `/api/waterchange/advance` endpoint
- Now accepts optional JSON body with `actualVolume` field
- If volume provided, calls `waterChangeAssistant->setActualVolume(actualVolume)` before advancing phase
- Compatible with both old (no body) and new (with body) API calls

**API Signature:**
```
POST /api/waterchange/advance
Content-Type: application/json

{
  "actualVolume": 62.5
}
```

#### `data/index.html`
**Added Functions:**

1. **Modified `advancePhase()`**
   - Checks if current phase is STABILIZING (phase 5)
   - If yes: Shows volume dialog
   - If no: Proceeds with normal phase advancement

2. **`showVolumeCompletionDialog()`**
   - Creates beautiful modal dialog with gradient background
   - Input field pre-filled with scheduled volume
   - Styled with modern Material Design aesthetics
   - Supports keyboard input (Enter key submits)

3. **`confirmVolumeAndComplete()`**
   - Validates volume input
   - Sends POST request with `actualVolume` in body
   - Shows success message with actual volume
   - Updates water change status

4. **`cancelVolumeDialog()`**
   - Closes dialog without completing
   - Keeps water change in STABILIZING phase

### Dialog Design

**Appearance:**
- Dark overlay (70% opacity black)
- Centered dialog box with blue gradient background
- White text with good contrast
- Large, clear input field
- Two-button layout (Cancel / Complete)
- Modern rounded corners and shadows

**Behavior:**
- Auto-focuses input field
- Enter key submits form
- Escape key not bound (user must click Cancel)
- Validates input before submission
- Shows error if invalid volume entered

## Example Usage

### Scenario 1: Accept Scheduled Volume
1. Tank: 250L, Scheduled: 25% = 62.5L
2. Dialog appears with "62.5" pre-filled
3. User clicks "✓ Complete"
4. History records: 62.5L changed ✓

### Scenario 2: Adjust to Actual Volume
1. Tank: 250L, Scheduled: 25% = 62.5L
2. Dialog appears with "62.5" pre-filled
3. User changes to "50.0" (actually changed less)
4. User clicks "✓ Complete"
5. History records: 50.0L changed ✓

### Scenario 3: More Than Scheduled
1. Tank: 250L, Scheduled: 25% = 62.5L
2. Dialog appears with "62.5" pre-filled
3. User changes to "80.0" (changed more than planned)
4. User clicks "✓ Complete"
5. History records: 80.0L changed ✓

## Technical Notes

### Phase Detection
- Uses `lastWaterChangePhase` variable to track current phase
- Phase 5 = STABILIZING (last phase before completion)
- Phase 6 = COMPLETE (water change done)
- Phase 0 = IDLE (no water change in progress)

### Volume Calculation
```javascript
// Get scheduled volume from display
const scheduledVolText = document.getElementById('wc-scheduled-volume').textContent;
const scheduledVol = parseFloat(scheduledVolText) || 25.0;
```

### API Communication
```javascript
// Send volume with phase advancement
const response = await fetch('/api/waterchange/advance', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({actualVolume: actualVolume})
});
```

### Backend Processing
```cpp
// WebServer.cpp - Extract volume from JSON
if (len > 0) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (!error && doc.containsKey("actualVolume")) {
        float actualVolume = doc["actualVolume"];
        waterChangeAssistant->setActualVolume(actualVolume);
    }
}
```

## Benefits

✅ **Accurate record keeping** - Records actual volume changed, not just scheduled  
✅ **User flexibility** - Can adjust volume on completion  
✅ **Better analytics** - Historical data reflects reality  
✅ **Non-intrusive** - Only appears at completion, not during water change  
✅ **Sensible default** - Pre-fills with scheduled volume for quick acceptance  
✅ **Professional UI** - Beautiful dialog matches overall design aesthetic  
✅ **Backward compatible** - Old API calls (without volume) still work  

## Testing Recommendations

1. **Start a water change** via web UI
2. **Advance through phases** using "Next Phase" button
3. **Watch for dialog** when clicking "Next Phase" from STABILIZING
4. **Test default volume**: Click "✓ Complete" without changing value
5. **Test custom volume**: Change value to different amount, then complete
6. **Test cancellation**: Click "Cancel" and verify water change stays in STABILIZING
7. **Test keyboard**: Use Enter key to submit, ensure it works
8. **Check history**: Verify correct volume appears in Recent Water Changes table
9. **Check serial output**: Should show "Actual water change volume updated to: XX litres"

## Serial Output

**When volume is set:**
```
Actual water change volume updated to: 62.5 litres
Water change completed successfully
Saved water change record
```

## Web UI Screenshots (Description)

### Dialog Appearance
- **Position**: Centered overlay on entire screen
- **Background**: Dark gradient (dark blue to lighter blue)
- **Size**: Max 400px wide, 90% width on mobile
- **Title**: "💧 Complete Water Change" in white, 24px
- **Description**: Friendly prompt text in light gray
- **Input**: Large white input box with blue border, 18px font
- **Buttons**: 
  - Cancel: Gray background, white text
  - Complete: Purple gradient with glow effect
- **Shadow**: Elevated with soft shadow for depth

## Future Enhancements

Potential improvements for future versions:

1. **History tracking**: Compare scheduled vs actual volumes over time
2. **Deviation alerts**: Warn if actual volume significantly differs from scheduled
3. **Quick presets**: Buttons for common volumes (50%, 75%, 100% of scheduled)
4. **Manual entry**: Allow starting water change with custom volume from the start
5. **Volume recommendations**: Suggest volume based on parameters (TDS, etc.)
6. **Metric/Imperial toggle**: Support gallons as well as litres

## Deployment Status

✅ **Code changes complete**  
✅ **HTML regenerated** (153KB → 22KB compressed)  
✅ **Firmware compiled successfully** (61.9% flash usage)  
⏳ **Ready for upload to ESP32**  
⏳ **Testing pending**  

## API Documentation

### Endpoint: POST /api/waterchange/advance

**Purpose:** Advance to next water change phase, optionally with actual volume

**Request (without volume):**
```http
POST /api/waterchange/advance HTTP/1.1
```

**Request (with volume):**
```http
POST /api/waterchange/advance HTTP/1.1
Content-Type: application/json

{
  "actualVolume": 62.5
}
```

**Response (success):**
```json
{
  "status": "ok"
}
```

**Response (error):**
```json
{
  "error": "Failed to advance phase"
}
```

## Next Steps

1. Upload firmware to ESP32
2. Test water change flow through all phases
3. Verify dialog appears at STABILIZING phase
4. Test volume input and completion
5. Check history for correct volume recording
6. Verify scheduled vs actual volume tracking
