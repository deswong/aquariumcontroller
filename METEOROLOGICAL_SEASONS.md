# Meteorological Season Configuration

## Simple Month-Based Seasons

The aquarium controller uses **meteorological seasons** - simple calendar months. No complex calculations, just easy-to-understand 3-month blocks!

---

## What Are Meteorological Seasons?

Meteorological seasons divide the year into four 3-month periods based on the calendar. Simple, predictable, and easy to understand!

### Why Meteorological?
- ✅ **Simple** - Based on calendar months everyone knows
- ✅ **Predictable** - Same dates every year
- ✅ **Easy to configure** - Just select your hemisphere
- ✅ **Practical** - Aligns with weather patterns
- ✅ **No calculations** - No solar event math needed

---

## Dropdown Configuration

Just select your hemisphere - that's it!

### 🌎 Northern Hemisphere
**Select if you live in:**
- USA, Canada, Mexico
- Europe (UK, France, Germany, Spain, Italy, etc.)
- Most of Asia (China, Japan, Korea, Russia, etc.)
- Middle East, North Africa

**Seasons:**
| Season | Months | Icon |
|--------|--------|------|
| **Spring** | March, April, May | 🌸 |
| **Summer** | June, July, August | ☀️ |
| **Autumn** | September, October, November | 🍂 |
| **Winter** | December, January, February | ❄️ |

---

### 🌏 Southern Hemisphere (Default for Brisbane!)
**Select if you live in:**
- Australia
- New Zealand
- South Africa
- Most of South America (Brazil, Argentina, Chile, etc.)

**Seasons:**
| Season | Months | Icon |
|--------|--------|------|
| **Spring** | September, October, November | 🌸 |
| **Summer** | December, January, February | ☀️ |
| **Autumn** | March, April, May | 🍂 |
| **Winter** | June, July, August | ❄️ |

---

### 🌴 Tropical (Near Equator)
**Select if you live in:**
- Singapore, Malaysia, Philippines
- Northern Indonesia
- Central Africa
- Central America, Caribbean

**Note:** Tropical regions have minimal seasonal variation. The system uses a simplified month-based approximation.

---

## Current Season (October 27, 2025)

| Location | Preset | Month | Current Season |
|----------|--------|-------|----------------|
| **Brisbane, Australia** | Southern Hemisphere | October | **Spring** 🌸 |
| **Sydney, Australia** | Southern Hemisphere | October | **Spring** 🌸 |
| **New York, USA** | Northern Hemisphere | October | **Autumn** 🍂 |
| **London, UK** | Northern Hemisphere | October | **Autumn** 🍂 |

---

## Configuration Methods

### ⭐ Web Interface (Easiest!)

**Navigate to:** Settings Tab → 🌍 Season Configuration

1. **Select your hemisphere** from the dropdown:
   - Northern Hemisphere (USA, Europe, Asia)
   - Southern Hemisphere (Australia, NZ) - Default for Brisbane
   - Tropical (Near Equator)

2. **Preview** the current season display showing:
   - Season icon (🌸 🌸 ☀️ 🍂 ❄️)
   - Season name
   - Month range

3. **Click** "💾 Save Season Preset" to persist the setting

The current season updates **immediately** when you change the dropdown, and the setting is saved to NVS (non-volatile storage) on the ESP32.

**Location in Web Interface:**
```
🔧 Settings Tab
  └── WiFi & MQTT Settings
  └── ⏰ Time & NTP Settings
  └── 🌍 Season Configuration ← HERE!
      ├── Season Preset Dropdown
      ├── Current Season Display (live preview)
      └── Save Button
  └── 🐠 Tank Volume Calculator
  └── ⚙️ Hardware Pin Configuration
```

---

### Web API

```bash
# Set to Southern Hemisphere (Australia, NZ, South America, South Africa)
curl -X POST http://aquarium-ip/api/season/config \
  -H "Content-Type: application/json" \
  -d '{"preset": "southern"}'

# Or use numeric value (1 = Southern)
curl -X POST http://aquarium-ip/api/season/config \
  -H "Content-Type: application/json" \
  -d '{"preset": 1}'

# Set to Northern Hemisphere (USA, Europe, Asia)
curl -X POST http://aquarium-ip/api/season/config \
  -H "Content-Type: application/json" \
  -d '{"preset": "northern"}'

# Set to Tropical (near equator)
curl -X POST http://aquarium-ip/api/season/config \
  -H "Content-Type: application/json" \
  -d '{"preset": "tropical"}'

# Get current season configuration
curl http://aquarium-ip/api/season/config
```

### Code

```cpp
#include "ConfigManager.h"

// Set to Southern Hemisphere (Australia)
configMgr->setSeasonPreset(SeasonPreset::SOUTHERN_HEMISPHERE);

// Set to Northern Hemisphere (USA, Europe)
configMgr->setSeasonPreset(SeasonPreset::NORTHERN_HEMISPHERE);

// Set to Tropical
configMgr->setSeasonPreset(SeasonPreset::TROPICAL);

// Get current season
SystemConfig config = configMgr->getConfig();
uint8_t season = SeasonCalculator::getCurrentSeason(config.seasonPreset);
Serial.printf("Season: %s %s\n",
             SeasonCalculator::getSeasonName(season),
             SeasonCalculator::getSeasonIcon(season));
```

### HTML Dropdown (For Web Interface)

```html
<label for="seasonPreset">Your Location:</label>
<select id="seasonPreset" name="seasonPreset">
  <option value="0">Northern Hemisphere (USA, Europe, Asia)</option>
  <option value="1" selected>Southern Hemisphere (Australia, NZ)</option>
  <option value="2">Tropical (Near Equator)</option>
</select>

<script>
document.getElementById('seasonPreset').addEventListener('change', function() {
  fetch('/api/season/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({preset: parseInt(this.value)})
  })
  .then(r => r.json())
  .then(data => {
    if (data.status === 'ok') {
      alert('Season preset updated successfully!');
    }
  });
});
</script>
```

---

## Season Month Ranges

### Northern Hemisphere
```cpp
Spring: months 3, 4, 5     (Mar, Apr, May)
Summer: months 6, 7, 8     (Jun, Jul, Aug)
Autumn: months 9, 10, 11   (Sep, Oct, Nov)
Winter: months 12, 1, 2    (Dec, Jan, Feb)
```

### Southern Hemisphere
```cpp
Spring: months 9, 10, 11   (Sep, Oct, Nov)
Summer: months 12, 1, 2    (Dec, Jan, Feb)
Autumn: months 3, 4, 5     (Mar, Apr, May)
Winter: months 6, 7, 8     (Jun, Jul, Aug)
```

---

## Implementation Details

### Class: SeasonCalculator

**Header:** `include/SeasonCalculator.h`  
**Source:** `src/SeasonCalculator.cpp`

**Method:**
```cpp
uint8_t SeasonCalculator::getCurrentSeason(SeasonPreset preset, time_t now);
```

**Logic:**
1. Get current month (1-12)
2. Check month against preset ranges
3. Return season number (0-3)

**That's it!** No complex solar calculations, no leap years, no day-of-year math. Just simple month checks.

---

## Benefits

### ✅ Simple
- No latitude/longitude needed
- No complex knowledge required
- Just pick your hemisphere

### ✅ Predictable
- Same dates every year
- No variation from year to year
- Easy to remember

### ✅ Practical
- Aligns with weather patterns
- Matches what meteorologists use
- Familiar to everyone

### ✅ Efficient
- **Tiny code** - Just month comparisons
- **Fast** - Instant calculation
- **Small storage** - 1 byte

---

## Seasonal PID Adaptation

The ML system uses these seasons to adapt PID control:

### Brisbane Example (Southern Hemisphere)

**October (Spring) - Current Month:**
- Preset: `SOUTHERN_HEMISPHERE`
- Month: 10 (October)
- Season: **Spring** 🌸 (months 9-11)
- PID Multiplier: Moderate warming (kp=1.05, ki=1.1, kd=1.05)

**December (Summer):**
- Month: 12
- Season: **Summer** ☀️ (months 12, 1-2)
- PID Multiplier: Gentler control (kp=0.8, ki=0.7, kd=1.2)

**June (Winter):**
- Month: 6
- Season: **Winter** ❄️ (months 6-8)
- PID Multiplier: More aggressive heating (kp=1.2, ki=1.3, kd=1.1)

---

## Default Configuration

**Brisbane, Australia:**
```cpp
seasonPreset = SeasonPreset::SOUTHERN_HEMISPHERE;
```

Already configured! No changes needed for Brisbane users.

---

## Resource Usage

| Metric | Value |
|--------|-------|
| **Flash** | 1,168,973 bytes (31.9%) |
| **RAM** | 51,412 bytes (15.7%) |
| **Config Storage** | 1 byte |
| **Calculation Time** | < 0.1 ms (just a few if statements) |

**Incredibly efficient!** Simple month-based logic.

---

## API Endpoints

### Set Season Preset
```http
POST /api/season/config

Request:
{
  "preset": "southern"  // or "northern", "tropical", or numeric 0-2
}

Response:
{
  "status": "ok"
}
```

### Get Current Season
```http
GET /api/pattern/seasonal

Response:
{
  "season": "spring",
  "avgAmbient": 22.5,
  "avgWater": 25.0,
  "avgPH": 6.8,
  "avgTDS": 245,
  "daysCollected": 14
}
```

---

## Files

### Core Files
- `include/SeasonCalculator.h` - Header with SeasonPreset enum and SeasonCalculator class
- `src/SeasonCalculator.cpp` - Simple month-based implementation (~110 lines)
- `include/ConfigManager.h` - Added seasonPreset field
- `src/ConfigManager.cpp` - Load/save preset
- `src/SystemTasks.cpp` - Uses SeasonCalculator
- `src/WebServer.cpp` - API endpoint /api/season/config

---

## Quick Reference

**For Brisbane (October 2025):**
- ✅ Preset: `SOUTHERN_HEMISPHERE`
- ✅ Current Month: October (10)
- ✅ Current Season: **Spring** 🌸 (Sep-Nov)
- ✅ Next Season: Summer ☀️ (starts December 1)

**To Configure:**
```bash
curl -X POST http://aquarium-ip/api/season/config \
  -d '{"preset":"southern"}' \
  -H "Content-Type: application/json"
```

**Done!** That's all you need. 🎉

---

## Summary

### Meteorological Seasons = Simple!

**No more:**
- ❌ Finding latitude/longitude coordinates
- ❌ Solar equinox/solstice calculations
- ❌ Day-of-year math
- ❌ Leap year corrections
- ❌ Complex formulas

**Just:**
- ✅ Select your hemisphere
- ✅ System uses calendar months
- ✅ Done!

**Result:** Simple, practical, easy-to-configure season detection that anyone can understand. Perfect for basic users!
