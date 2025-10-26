#ifndef SEASON_CALCULATOR_H
#define SEASON_CALCULATOR_H

#include <Arduino.h>
#include <time.h>

/**
 * @brief Meteorological Season Calculator
 * 
 * Uses simple calendar months for season calculation (meteorological seasons).
 * Just pick your hemisphere from a dropdown - that's it!
 * 
 * Season numbering (consistent for ML training):
 * - 0 = Spring
 * - 1 = Summer
 * - 2 = Autumn
 * - 3 = Winter
 */
enum class SeasonPreset : uint8_t {
    NORTHERN_HEMISPHERE = 0,  // USA, Europe, Asia (most countries)
    SOUTHERN_HEMISPHERE = 1,  // Australia, New Zealand, South America, Southern Africa
    TROPICAL = 2,             // Near equator - minimal seasonal variation
    CUSTOM = 3                // Custom month ranges (advanced users)
};

/**
 * @brief Calculates meteorological seasons based on calendar months
 * 
 * Meteorological seasons align with calendar months:
 * - Northern: Spring (Mar-May), Summer (Jun-Aug), Autumn (Sep-Nov), Winter (Dec-Feb)
 * - Southern: Spring (Sep-Nov), Summer (Dec-Feb), Autumn (Mar-May), Winter (Jun-Aug)
 * - Tropical: Simplified month-based approximation
 */
class SeasonCalculator {
public:
    /**
     * @brief Calculate current meteorological season
     * 
     * Uses simple calendar months:
     * - Northern: Spring (Mar-May), Summer (Jun-Aug), Autumn (Sep-Nov), Winter (Dec-Feb)
     * - Southern: Spring (Sep-Nov), Summer (Dec-Feb), Autumn (Mar-May), Winter (Jun-Aug)
     * 
     * @param preset Hemisphere preset (Northern, Southern, Tropical)
     * @param now Current time (defaults to current system time if 0)
     * @return uint8_t Season number (0=Spring, 1=Summer, 2=Autumn, 3=Winter)
     */
    static uint8_t getCurrentSeason(SeasonPreset preset, time_t now = 0);
    
    /**
     * @brief Get season name as string
     * 
     * @param season Season number (0-3)
     * @return const char* Season name ("Spring", "Summer", "Autumn", or "Winter")
     */
    static const char* getSeasonName(uint8_t season);
    
    /**
     * @brief Get season icon for web interface
     * 
     * @param season Season number (0-3)
     * @return const char* Unicode emoji for season
     */
    static const char* getSeasonIcon(uint8_t season);
    
    /**
     * @brief Get preset name for display
     * 
     * @param preset SeasonPreset enum value
     * @return const char* Preset name ("Northern Hemisphere", "Southern Hemisphere", etc.)
     */
    static const char* getPresetName(SeasonPreset preset);
};

#endif // SEASON_CALCULATOR_H
