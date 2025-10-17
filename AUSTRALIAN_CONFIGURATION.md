# Australian Configuration Guide

## Overview

This aquarium controller is configured with Australian standards, measurements, and typical water parameters.

---

## Default Settings (Australian)

### Time Zone Configuration
- **NTP Server:** `au.pool.ntp.org` (Australian NTP pool)
- **Time Zone:** AEST (Australian Eastern Standard Time)
- **GMT Offset:** UTC+10 (36000 seconds)
- **Daylight Saving:** +1 hour (3600 seconds) when active

**Note:** Adjust for your state:
- **AEST (NSW, VIC, TAS, QLD\*):** UTC+10
- **ACST (SA, NT\*):** UTC+9.5 (34200 seconds)
- **AWST (WA):** UTC+8 (28800 seconds)

\*Queensland and Northern Territory do NOT observe daylight saving time - set `daylightOffsetSec = 0`

### Temperature Settings
- **Default Target:** 25°C (suitable for tropical fish)
- **Safety Maximum:** 30°C (Australian summer-safe limit)
- **Units:** Always Celsius (°C)

**Typical Australian Aquarium Temperatures:**
- **Tropical (Tetras, Guppies, Barbs):** 24-26°C
- **Goldfish (Cold water):** 18-22°C
- **Discus (Warm water):** 28-30°C
- **Planted tanks:** 24-26°C

### pH Settings
- **Default Target:** 6.8 (slightly acidic for planted tanks)
- **Safety Minimum:** 6.0

**Australian Water pH Ranges:**
- **Sydney tap water:** 7.5-8.5
- **Melbourne tap water:** 7.0-8.0
- **Brisbane tap water:** 7.0-8.5
- **Adelaide tap water:** 7.5-8.5
- **Perth tap water:** 7.0-8.0

**Aquarium pH Targets:**
- **Community tanks:** 6.5-7.5
- **African cichlids:** 7.8-8.6
- **Discus/Tetras:** 6.0-7.0
- **Planted tanks:** 6.5-7.2

### TDS (Total Dissolved Solids)
- **Default Threshold:** 400 ppm
- **Units:** Parts per million (ppm) or mg/L

**Australian Tap Water TDS:**
- **Sydney:** 50-150 ppm (soft water)
- **Melbourne:** 50-100 ppm (very soft water)
- **Brisbane:** 100-200 ppm (moderate)
- **Adelaide:** 150-300 ppm (hard water)
- **Perth:** 100-200 ppm (moderate)

**Aquarium TDS Targets:**
- **Planted tanks:** 150-300 ppm
- **Community tanks:** 200-400 ppm
- **Before water change:** <500 ppm
- **Discus/Soft water fish:** 100-200 ppm

### Volume Measurements
All liquid measurements use **millilitres (mL)** and **litres (L)**:

- **Dosing pump:** mL per dose
- **Daily limits:** mL per day
- **Water changes:** % or litres
- **Aquarium capacity:** Litres

**Common Australian Aquarium Sizes:**
- **Nano:** 20-40 L
- **Small:** 60-100 L
- **Medium:** 150-200 L
- **Large:** 300-400 L
- **Very Large:** 500+ L

---

## Electrical Safety (Australian Standards)

### Mains Voltage
- **Standard:** 240V AC, 50Hz
- **Plug Type:** Type I (AS/NZS 3112)
- **Safety Device:** RCD (Residual Current Device) mandatory

### Safety Requirements
1. **RCD Protection:**
   - All aquarium equipment MUST be on RCD-protected circuit
   - RCD should be 30mA sensitivity
   - Test RCD monthly (press test button)

2. **Double Insulation:**
   - Prefer Class II (double-insulated) appliances
   - Look for ⧈ symbol on equipment

3. **IP Rating:**
   - Equipment near water should be IP65+ rated
   - Controller enclosure: IP54 minimum

4. **Earthing:**
   - All metal components must be earthed
   - Use 3-pin plugs (never remove earth pin!)
   - Earth bonding for metal tank stands

5. **Distance from Water:**
   - Keep all 240V equipment >30cm from water
   - No power boards directly above aquarium
   - Use drip loops on all cables

---

## Australian Product Recommendations

### Temperature Sensors (DS18B20)
- **Temperature Range:** -10°C to +85°C
- **Accuracy:** ±0.5°C (0-50°C range)
- **Suitable for:** All Australian aquariums

### pH Sensors
- **Calibration:** Use buffer solutions pH 4.0, 7.0, 10.0
- **Lifespan:** Replace yearly in Australian climate
- **Storage:** Keep probe tip moist between use

### TDS Sensors
- **Calibration:** 342 ppm or 1413 µS/cm standard
- **Maintenance:** Clean probe weekly (Australian algae growth)
- **Suitable for:** Freshwater aquariums

### Heaters (240V)
- **Sizing:** 1 watt per litre (e.g., 200L tank = 200W heater)
- **Australian brands:** Aqua One, Eheim, Jager
- **Important:** Controller switches heater on/off - use reliable brand

### CO2 Equipment
- **Bottle Size:** 2.6kg or 6kg (common Australian sizes)
- **Regulator:** Must be compatible with Australian cylinders
- **Solenoid:** 240V AC (controller switches solenoid)
- **Bubble Counter:** 1-3 bubbles/second typical

---

## Wi-Fi Configuration

### Australian ISPs
The controller works with all Australian internet providers:
- Telstra
- Optus  
- TPG
- Aussie Broadband
- etc.

### Wi-Fi Settings
- **SSID:** Your home network name
- **Password:** Your Wi-Fi password
- **Band:** 2.4GHz (ESP32 doesn't support 5GHz)

**Note:** Some Australian modems use band steering - ensure 2.4GHz band is enabled separately.

---

## MQTT Configuration

### Australian MQTT Brokers (Optional)
If using cloud MQTT:
- **CloudMQTT:** Australian servers available
- **AWS IoT:** ap-southeast-2 (Sydney region)
- **Azure IoT:** Australia East (NSW)

### Local MQTT Broker
For privacy, run local broker (recommended):
- **Mosquitto** on Raspberry Pi
- **Home Assistant** built-in broker
- **Node-RED** with Aedes

---

## Typical Australian Setup Examples

### Sydney Planted Tank (200L)
```cpp
tempTarget = 25.0;          // 25°C
tempSafetyMax = 30.0;       // Sydney summer: 45°C ambient
phTarget = 6.8;             // Planted tank
phSafetyMin = 6.0;
gmtOffsetSec = 36000;       // UTC+10
daylightOffsetSec = 3600;   // AEDT in summer
```

### Brisbane Tropical Tank (150L)
```cpp
tempTarget = 26.0;          // 26°C (warmer climate)
tempSafetyMax = 32.0;       // Brisbane summer: 35°C ambient
phTarget = 7.0;             // Community fish
phSafetyMin = 6.5;
gmtOffsetSec = 36000;       // UTC+10
daylightOffsetSec = 0;      // QLD no daylight saving
```

### Adelaide Discus Tank (300L)
```cpp
tempTarget = 29.0;          // 29°C (warm for discus)
tempSafetyMax = 32.0;       // Adelaide summer: 40°C ambient
phTarget = 6.5;             // Acidic for discus
phSafetyMin = 6.0;
gmtOffsetSec = 34200;       // UTC+9.5 (ACST)
daylightOffsetSec = 3600;   // ACDT in summer
```

### Perth Reef Tank (100L)
```cpp
tempTarget = 25.0;          // 25°C (coral optimal)
tempSafetyMax = 28.0;       // Reef: strict temp control
phTarget = 8.2;             // Alkaline for reef
phSafetyMin = 8.0;
gmtOffsetSec = 28800;       // UTC+8 (AWST)
daylightOffsetSec = 0;      // WA no daylight saving
```

---

## Water Change Guidelines (Australian Conditions)

### Frequency
- **Summer:** More frequent due to higher evaporation
- **Winter:** Less frequent (cooler temps, less evaporation)
- **Typical:** 20-30% weekly

### Australian Tap Water Treatment
1. **Chloramine Removal:** Most Australian cities use chloramine (not just chlorine)
   - Use water conditioner (e.g., Seachem Prime)
   - Chloramine doesn't evaporate - must be chemically removed

2. **Temperature Matching:**
   - Hot summer tap water: Let cool before adding
   - Cold winter tap water: Use aquarium water heater or mix with hot tap

3. **pH Adjustment:**
   - Australian tap water is typically alkaline (pH 7.5-8.5)
   - Add slowly to avoid shocking fish
   - Consider RO/DI system for sensitive species

### TDS After Water Changes
- **New water TDS:** Australian tap typically 50-300 ppm
- **Old water TDS:** May be 300-500 ppm
- **After 30% change:** TDS drops by ~30%

---

## Seasonal Adjustments (Australian Climate)

### Summer (Dec-Feb)
- **Ambient temp:** 30-45°C (varies by region)
- **Risk:** Overheating
- **Actions:**
  - Increase monitoring frequency
  - Use cooling fans if needed
  - Check heater isn't stuck ON
  - More frequent water changes (evaporation)

### Autumn (Mar-May)
- **Ambient temp:** 15-25°C
- **Stable:** Best time for aquarium maintenance
- **Actions:**
  - Standard monitoring
  - Good time for equipment upgrades

### Winter (Jun-Aug)
- **Ambient temp:** 5-15°C (varies by region)
- **Risk:** Heater overwork
- **Actions:**
  - Check heater is adequate
  - Monitor for heater failure
  - Insulate tank if in cold room
  - Less water changes (evaporation low)

### Spring (Sep-Nov)
- **Ambient temp:** 15-30°C
- **Variable:** Temperature swings
- **Actions:**
  - Watch for day/night temp variations
  - Adjust targets for breeding season
  - Prepare for summer heat

---

## Australian Aquarium Shops

### Equipment Availability
- **Jaycar Electronics:** ESP32 boards, sensors, components
- **Bunnings:** Electrical enclosures, cable, tools
- **Local aquarium shops:** Heaters, CO2, test kits
- **Online:** eBay Australia, Amazon AU, AliExpress

### Local Support
- **Forums:** Aquarium Society (each state has one)
- **Facebook Groups:** Australian Aquarium Hobbyists
- **Reddit:** r/AustralianAquariums

---

## Legal & Standards Compliance

### Electrical Installation
- **AS/NZS 3000:2018** - Wiring rules
- **DIY Allowed:** Low voltage (<50V DC) - YES
- **DIY Allowed:** 240V AC switching - YES (plug-in devices)
- **Licensed Required:** 240V AC wiring in walls - YES

### Water Electrical Safety
- **AS/NZS 3100** - Appliances for wet areas
- **IP Rating:** IP65+ for splashproof
- **RCD:** Mandatory for all wet area circuits

### Radio Compliance
- **ESP32 Wi-Fi:** 2.4GHz ISM band (license-free in Australia)
- **ACMA Approved:** ESP32 modules typically pre-certified
- **Legal:** Commercial products need RCM mark (DIY exempt)

---

## Troubleshooting Australian-Specific Issues

### Wi-Fi Not Connecting
- **Problem:** NBN modem blocks 2.4GHz
- **Solution:** Enable 2.4GHz band separately (disable band steering)

### Temperature Too High in Summer
- **Problem:** Ambient 40°C+ causing heater to run
- **Solution:** 
  - Use cooling fan
  - Move tank away from windows
  - Reduce lighting hours

### pH Always High
- **Problem:** Australian tap water alkaline
- **Solution:**
  - Use peat filtration
  - Add driftwood (natural tannins)
  - Consider RO/DI water system

### TDS Rising Too Fast
- **Problem:** Hard water + fish waste
- **Solution:**
  - Increase water change frequency
  - Reduce feeding
  - Add plants (consume nitrates)

---

## Configuration via Web Interface

Access at: `http://<ESP32_IP_ADDRESS>`

### Australian Settings to Configure:
1. **Time Zone:**
   - NTP Server: `au.pool.ntp.org`
   - GMT Offset: Your state (see above)
   - Daylight Saving: Yes (except QLD, NT, WA)

2. **Temperature:**
   - Target: 24-26°C (tropical)
   - Safety Max: 30-32°C (summer protection)

3. **pH:**
   - Target: 6.8-7.2 (most Australian fish)
   - Safety Min: 6.0

4. **Wi-Fi:**
   - SSID: Your Australian ISP modem
   - Password: Your Wi-Fi password

---

## Summary

✅ **Units:** Celsius (°C), millilitres (mL), litres (L), ppm  
✅ **Voltage:** 240V AC, 50Hz  
✅ **Safety:** RCD mandatory, IP65+ enclosures  
✅ **Time:** AEST/ACST/AWST + daylight saving  
✅ **Water:** Adjust for local tap water (soft in Sydney/Melbourne, harder in Adelaide)  
✅ **Climate:** Account for hot Australian summers  
✅ **Standards:** AS/NZS 3000, AS/NZS 3100 compliant  

---

## Quick Reference

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Voltage** | 240V AC | Australian standard |
| **Frequency** | 50Hz | Mains frequency |
| **Temperature Unit** | °C | Celsius only |
| **Volume Unit** | mL / L | Millilitres / Litres |
| **TDS Unit** | ppm | Parts per million |
| **Time Zone** | UTC+8 to +11 | Varies by state |
| **NTP Server** | au.pool.ntp.org | Australian pool |
| **Plug Type** | Type I (AS 3112) | 3-pin Australian |
| **Safety Device** | RCD 30mA | Mandatory |

---

**G'day and happy fishkeeping! 🐠🇦🇺**
