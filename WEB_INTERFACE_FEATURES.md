# Web Interface New Features

## Three New Features Added

### 1. 🕐 Real-Time Clock Display
- **Location**: Top-right corner of web page
- **Update Frequency**: Every 1 second
- **Format**: "Day, DD Mon YYYY HH:MM:SS" (Australian format)
- **Example**: "Sat, 19 Oct 2025 14:37:45"

### 2. 🐠 Tank Volume Calculator  
- **Location**: Settings tab
- **Purpose**: Calculate water volume from tank dimensions
- **Features**:
  - Input fields: Length (cm), Width (cm), Height (cm)
  - Calculates gross volume in litres
  - Calculates actual water volume (90% accounting for substrate/decorations)
  - One-click apply to Water Change Assistant
  
**How to Use**:
1. Go to Settings tab
2. Find "Tank Volume Calculator" section
3. Enter your tank dimensions in centimeters
4. Click "📐 Calculate Volume"
5. Review the calculated volume and actual volume (90%)
6. Click "✅ Apply to Water Change Assistant" to save

**Example**:
- Tank: 60cm × 30cm × 35cm
- Gross Volume: 63.0 litres
- Actual Volume: ~56.7 litres (accounting for substrate)

### 3. 🔧 Settings Auto-Restore
- **Location**: Settings tab
- **Purpose**: Show current MQTT and NTP settings when opening tab
- **Auto-populated Fields**:
  - MQTT Server
  - MQTT Username
  - MQTT Topic Prefix
  - MQTT Port
  - NTP Server
  - GMT Offset
  - DST Offset

**Benefits**:
- No need to remember current settings
- Can see what's configured before making changes
- Prevents accidental overwrites of blank fields

## API Endpoints

### New Endpoint: `/api/config`
- **Method**: GET
- **Purpose**: Retrieve current configuration
- **Returns**: JSON with all config parameters
- **Used by**: Settings tab to populate fields

### Updated Endpoint: `/api/waterchange/config`
- **Method**: POST
- **Purpose**: Update water change settings
- **Body**: `{"tankVolume": <litres>}`
- **Used by**: Tank calculator to apply volume

## Technical Details

### Clock Implementation
```javascript
function updateDateTime() {
    const now = new Date();
    const options = {
        weekday: 'short',
        year: 'numeric',
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false
    };
    document.getElementById('current-datetime').textContent = 
        now.toLocaleString('en-AU', options);
}

setInterval(updateDateTime, 1000);
```

### Tank Calculator Formula
```javascript
// Calculate volume in litres (cm³ / 1000)
const volumeLitres = (length * width * height) / 1000;

// Actual water volume (90% for substrate/decorations)
const actualLitres = volumeLitres * 0.9;
```

### Settings Restore
```javascript
async function loadCurrentSettings() {
    const response = await fetch('/api/config');
    if (response.ok) {
        const config = await response.json();
        // Populate form fields
        document.getElementById('mqtt-server').value = config.mqttServer;
        document.getElementById('mqtt-user').value = config.mqttUser;
        // ... etc
    }
}
```

## File Changes

### Modified Files:
1. **data/index.html**
   - Added clock display in header
   - Added tank calculator section in settings tab
   - Added `loadCurrentSettings()` function
   - Added `calculateTankVolume()` function
   - Added `applyTankVolume()` function
   - Added `updateDateTime()` function
   - Modified `showTab()` to call `loadCurrentSettings()` for settings tab

2. **include/WebInterface.h**
   - Regenerated with new HTML (20,034 bytes compressed)

3. **src/WebServer.cpp**
   - Added `/api/config` endpoint

## Firmware Stats

- **Firmware Size**: 1,207,481 bytes (61.4% flash)
- **RAM Usage**: 55,444 bytes (16.9%)
- **HTML Size**: 20,034 bytes compressed (14% of 138,948 bytes original)

## User Workflow Example

### Setting Up Tank Volume:
1. Open web interface at http://192.168.1.128
2. Click "🔧 Settings" tab
3. Scroll to "🐠 Tank Volume Calculator"
4. Enter dimensions:
   - Length: 60 cm
   - Width: 30 cm
   - Height: 35 cm
5. Click "📐 Calculate Volume"
6. See result: "63.0 litres" (actual: ~56.7 litres)
7. Click "✅ Apply to Water Change Assistant"
8. Success message appears
9. Go to "💧 Water Change" tab to verify

### Viewing Current Settings:
1. Click "🔧 Settings" tab
2. MQTT server and username automatically populate
3. Make any changes needed
4. Click "💾 Save All Settings"

### Checking Time:
- Just look at top-right corner
- Updates every second
- Shows current browser time

## Benefits

1. **Better UX**: No need to remember current MQTT settings
2. **Accurate Tank Setup**: Calculator ensures correct volume for water changes
3. **Time Reference**: Always know what time it is
4. **Easier Configuration**: Visual feedback and calculations
5. **Prevents Errors**: Can see current values before changing

## Troubleshooting

### Clock not updating:
- Check browser JavaScript console for errors
- Refresh page
- Clock uses browser's local time

### Tank calculator not working:
- Ensure all three dimensions are entered
- Values must be positive numbers
- Check browser console for errors

### Settings not loading:
- Check `/api/config` endpoint is accessible
- Verify ESP32 is connected
- Check browser network tab for 200 OK response

---

**Version**: v2.1  
**Date**: October 19, 2025  
**Features**: Tank calculator, Settings restore, Real-time clock
