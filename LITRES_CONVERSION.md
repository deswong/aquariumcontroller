# Water Change Assistant - Litres Conversion

## Changes Made

All water volume measurements in the Water Change Assistant have been converted from **gallons** to **litres**.

### Files Modified

1. **include/WaterChangeAssistant.h**
   - Changed `tankVolumeGallons` → `tankVolumeLitres`
   - Updated function signatures:
     - `setTankVolume(float litres)`
     - `getScheduledChangeVolume()` returns litres
     - `startWaterChange(float volumeLitres = 0)`

2. **src/WaterChangeAssistant.cpp**
   - Updated default tank volume: 75.0 litres (was 20.0 gallons)
   - Tank volume range: 20-4000 litres (was 5-1000 gallons)
   - All Serial output messages now display "litres" or "L"
   - All calculations use `tankVolumeLitres`

3. **src/main.cpp**
   - Default tank volume: 75.0 litres (equivalent to ~20 gallons)

4. **data/index.html**
   - Water Change tab now displays "litres" instead of "gallons"
   - Updated 4 locations:
     - Tank Volume display
     - Scheduled Volume display
     - Volume This Month stat
     - Average Volume stat

5. **include/WebInterface.h**
   - Regenerated with compressed HTML (18,816 bytes)

## Conversion Reference

- **1 US gallon = 3.785 litres**
- **20 gallons ≈ 75 litres**

### Common Tank Sizes

| Gallons | Litres | Typical Use |
|---------|--------|-------------|
| 10      | 38     | Small nano tank |
| 20      | 75     | Standard small tank |
| 40      | 150    | Medium tank |
| 55      | 208    | Large community tank |
| 75      | 284    | Large display tank |
| 100     | 379    | Extra large tank |

## Default Settings

- **Default Tank Volume**: 75 litres
- **Tank Volume Range**: 20-4000 litres
- **Default Schedule**: 25% weekly (18.75 litres for 75L tank)

## NVS Storage

Water change history is stored in NVS with volumes in litres. The key names remain the same:
- `tankVol` - Tank volume in litres
- `vol_X` - Water change volume records in litres

## Serial Output Examples

```
Water Change Assistant initialized
Tank volume: 75.0 litres
Schedule: 7 days, 25.0% volume

=== Water Change Started ===
Volume: 18.8 litres (25.0%)
Temperature before: 25.0°C
pH before: 6.80
Systems paused for safety
===========================

=== Water Change Complete ===
Volume changed: 18.8 litres
Temperature: 25.0°C → 25.0°C (Δ0.00°C)
pH: 6.80 → 6.80 (Δ0.00)
Duration: 5 minutes
Systems resumed
============================
```

## Web Interface

The web interface now displays all volumes in litres:
- Tank Volume: XX litres
- Scheduled Volume: XX litres  
- Volume This Month: XX litres
- Average Volume: XX litres

## Verification

✅ **Compilation**: Successful (1,206,041 bytes, 61.3% flash)  
✅ **Upload**: Successful via COM3  
✅ **HTML Size**: 18,816 bytes compressed (14% of original)  
✅ **Default Values**: 75L tank with 25% weekly changes (18.75L)

## Notes

- The struct `WaterChangeRecord` already used "Liters" in the comment, so no change needed there
- All safety limits remain the same (±2°C, ±0.5 pH, 50% max volume)
- Water change predictor calculations are volume percentage-based, so no changes needed
- MQTT alerts now show "L" instead of "gal"

---

**Date**: October 19, 2025  
**Version**: v2.0 - Metric conversion complete
