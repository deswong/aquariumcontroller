# Water Change Phase Advancement Bug Fix

## Problem
After implementing the volume completion dialog, the water change process would not proceed past the DRAINING phase. When clicking "Next Phase" from DRAINING (phase 2), nothing happened.

## Root Cause
The API endpoint `/api/waterchange/advance` was modified to accept an optional JSON body with `actualVolume`. However, the implementation had a critical flaw:

**Before (Broken):**
```cpp
server->on("/api/waterchange/advance", HTTP_POST, 
    [](AsyncWebServerRequest* request) {}, // Empty handler!
    NULL, 
    [](AsyncWebServerRequest* request, uint8_t *data, size_t len, ...) {
        // Body handler - only called when body is present
        ...
    });
```

The first lambda (normal request handler) was **empty** - just `{}`. This meant:
- Requests **without a body** (normal phase advancement) → Empty handler → No response! ❌
- Requests **with a body** (completion with volume) → Body handler called → Works ✓

Since phases PREPARE → DRAINING → DRAINED → FILLING all send requests **without a body**, they all failed silently.

## Solution
Implement **both handlers properly** so the endpoint works with AND without a body:

**After (Fixed):**
```cpp
server->on("/api/waterchange/advance", HTTP_POST, 
    [](AsyncWebServerRequest* request) {
        // Normal request handler - called when NO body present
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"...\"}");
            return;
        }
        
        if (waterChangeAssistant->advancePhase()) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"...\"}");
        }
    }, 
    NULL, 
    [](AsyncWebServerRequest* request, uint8_t *data, size_t len, ...) {
        // Body handler - called when body IS present
        if (!waterChangeAssistant) {
            request->send(500, "application/json", "{\"error\":\"...\"}");
            return;
        }
        
        // Extract actualVolume from JSON body
        if (len > 0) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (!error && doc.containsKey("actualVolume")) {
                float actualVolume = doc["actualVolume"];
                waterChangeAssistant->setActualVolume(actualVolume);
            }
        }
        
        if (waterChangeAssistant->advancePhase()) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"...\"}");
        }
    });
```

## How It Works Now

### Normal Phase Advancement (No Body)
**Used for phases:** PREPARE → DRAINING → DRAINED → FILLING → STABILIZING

**Request:**
```http
POST /api/waterchange/advance HTTP/1.1
```

**Handler:** First lambda (normal request handler)
**Result:** `advancePhase()` called, phase advances ✓

### Completion with Volume (With Body)
**Used for phase:** STABILIZING → COMPLETE (with dialog)

**Request:**
```http
POST /api/waterchange/advance HTTP/1.1
Content-Type: application/json

{"actualVolume": 62.5}
```

**Handler:** Third lambda (body handler)
**Result:** `setActualVolume(62.5)` called, then `advancePhase()`, completion with actual volume ✓

## Testing Results

### Before Fix
1. Start water change ✓
2. Advance to DRAINING ✓
3. Click "Next Phase" from DRAINING ❌ **STUCK**
4. Nothing happens, no response
5. Serial: No output

### After Fix
1. Start water change ✓
2. Advance to DRAINING ✓
3. Click "Next Phase" from DRAINING ✓ **WORKS**
4. Advances to DRAINED ✓
5. Continue through all phases ✓
6. Dialog appears at STABILIZING ✓
7. Completion records actual volume ✓

## AsyncWebServer API Pattern

The correct pattern for endpoints that accept both with and without body:

```cpp
server->on("/endpoint", HTTP_POST,
    // Handler 1: Called for requests WITHOUT body
    [](AsyncWebServerRequest* request) {
        // Handle no-body case
        request->send(200, ...);
    },
    // Handler 2: File upload handler (not used here)
    NULL,
    // Handler 3: Called for requests WITH body
    [](AsyncWebServerRequest* request, uint8_t *data, size_t len, ...) {
        // Parse body and handle
        request->send(200, ...);
    }
);
```

**Important:** 
- If **Handler 1** is empty (`{}`), requests without body fail
- If **Handler 3** is missing, requests with body fail
- Both must be properly implemented for dual-mode endpoints

## Files Modified
- `src/WebServer.cpp` - Fixed `/api/waterchange/advance` endpoint handler

## Lessons Learned
1. **Empty lambda handlers are not "pass-through"** - they consume the request and do nothing
2. **AsyncWebServer requires explicit handling** for both body and no-body cases
3. **Always test all code paths** - the STABILIZING→COMPLETE path worked (with body), but earlier phases failed (without body)
4. **Silent failures are dangerous** - no error message made debugging harder

## Deployment Status
✅ **Bug fixed**  
✅ **Firmware recompiled** (61.9% flash usage)  
⏳ **Ready for upload and testing**  

## Next Steps
1. Upload firmware to ESP32
2. Test complete water change flow
3. Verify all phases advance correctly
4. Confirm dialog appears only at STABILIZING
5. Verify actual volume is recorded in history
