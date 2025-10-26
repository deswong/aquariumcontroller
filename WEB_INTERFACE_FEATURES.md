# Web Interface New Features

## Four Major Features

### 1. 🕐 Real-Time Clock Display
- **Location**: Top-right corner of web page
- **Update Frequency**: Every 1 second
- **Format**: "Day, DD Mon YYYY HH:MM:SS" (Australian format)
- **Example**: "Sat, 19 Oct 2025 14:37:45"

### 2. 🌍 Season Configuration (NEW!)
- **Location**: Settings tab → Season Configuration section
- **Purpose**: Configure meteorological season for PID adaptation and pattern learning
- **Features**:
  - Dropdown to select hemisphere preset:
    - Northern Hemisphere (USA, Europe, Asia)
    - Southern Hemisphere (Australia, NZ) - **Default for Brisbane**
    - Tropical (Near Equator)
  - **Live preview** showing current season with icon
  - Season name and month range displayed
  - Instant visual feedback when changing preset
  - Saves to NVS (non-volatile storage)

**How to Use**:
1. Go to Settings tab
2. Scroll to "🌍 Season Configuration" section
3. Select your hemisphere from dropdown
4. Preview shows current season immediately (🌸 Spring, ☀️ Summer, 🍂 Autumn, ❄️ Winter)
5. Click "💾 Save Season Preset" to persist setting

**Example for Brisbane, Australia**:
- Preset: "Southern Hemisphere" (selected by default)
- Current Season (October 2025): Spring 🌸
- Month Range: September - November
- Used for: Seasonal PID multipliers and pattern learning adaptation

**Benefits**:
- Simple configuration - no complex latitude/longitude needed
- Accurate seasonal PID optimization based on your location
- Visual feedback with season icons
- Supports Northern, Southern, and Tropical regions
- See [METEOROLOGICAL_SEASONS.md](METEOROLOGICAL_SEASONS.md) for full details

### 3. 🐠 Tank Volume Calculator  
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

### 4. 🔧 Settings Auto-Restore
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

### New Endpoint: `/api/season/config`
- **Method**: GET
- **Purpose**: Retrieve current season preset configuration
- **Returns**: `{"preset": 0-2, "presetName": "...", "season": 0-3, "seasonName": "..."}`
- **Used by**: Settings tab to populate season dropdown

### New Endpoint: `/api/season/config` (POST)
- **Method**: POST
- **Purpose**: Update season preset
- **Body**: `{"preset": 0-2}` or `{"preset": "northern"/"southern"/"tropical"}`
- **Returns**: `{"status": "ok", "preset": 0-2}`
- **Used by**: Settings tab season preset save button

### Endpoint: `/api/config`
- **Method**: GET
- **Purpose**: Retrieve current configuration
- **Returns**: JSON with all config parameters
- **Used by**: Settings tab to populate fields

### Endpoint: `/api/waterchange/config`
- **Method**: POST
- **Purpose**: Update water change settings
- **Body**: `{"tankVolume": <litres>}`
- **Used by**: Tank calculator to apply volume

## Technical Details

### Season Configuration Implementation
```javascript
async function loadSeasonPreset() {
    const response = await fetch('/api/season/config');
    if (response.ok) {
        const data = await response.json();
        document.getElementById('season-preset').value = data.preset;
        updateCurrentSeasonDisplay();
    }
}

async function saveSeasonPreset() {
    const preset = parseInt(document.getElementById('season-preset').value);
    const response = await fetch('/api/season/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ preset: preset })
    });
    if (response.ok) {
        alert('✅ Season preset saved successfully!');
        updateCurrentSeasonDisplay();
    }
}

function updateCurrentSeasonDisplay() {
    const preset = parseInt(document.getElementById('season-preset').value);
    const month = new Date().getMonth() + 1; // 1-12
    
    // Calculate season based on preset and month
    // Northern: Spring(3-5), Summer(6-8), Autumn(9-11), Winter(12,1-2)
    // Southern: Spring(9-11), Summer(12,1-2), Autumn(3-5), Winter(6-8)
    // Tropical: Wet(12-5), Dry(6-11)
    
    // Update icon, name, and month range in UI
}

// Auto-load on page load
loadSeasonPreset();

// Update display when dropdown changes
document.getElementById('season-preset').addEventListener('change', updateCurrentSeasonDisplay);
```

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
3. **Seasonal Optimization**: PID adapts automatically to your hemisphere's seasons
4. **Location-Aware**: Simple dropdown configuration for worldwide use
5. **Time Reference**: Always know what time it is
6. **Easier Configuration**: Visual feedback and calculations
7. **Prevents Errors**: Can see current values before changing

## Troubleshooting

### Clock not updating:
- Check browser JavaScript console for errors
- Refresh page
- Clock uses browser's local time

### Season not displaying correctly:
- Verify correct hemisphere preset selected
- Check browser date/time is correct (used for season calculation)
- Refresh page after saving preset
- Check `/api/season/config` endpoint is accessible

### Tank calculator not working:
- Ensure all three dimensions are entered
- Values must be positive numbers
- Check browser console for errors

### Settings not loading:
- Check `/api/config` endpoint is accessible
- Verify ESP32 is connected
- Check browser network tab for 200 OK response

---

**Version**: v2.2  
**Date**: October 27, 2025  
**Features**: Season configuration, Tank calculator, Settings restore, Real-time clock
