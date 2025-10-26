#include "SeasonCalculator.h"

uint8_t SeasonCalculator::getCurrentSeason(SeasonPreset preset, time_t now) {
    // Use current time if not specified
    if (now == 0) {
        now = time(nullptr);
    }
    
    struct tm* timeinfo = localtime(&now);
    int month = timeinfo->tm_mon + 1; // tm_mon is 0-based (0-11), we want 1-12
    
    switch (preset) {
        case SeasonPreset::NORTHERN_HEMISPHERE: {
            // Northern Hemisphere Meteorological Seasons (based on calendar months)
            // Spring: March, April, May (3-5)
            // Summer: June, July, August (6-8)
            // Autumn: September, October, November (9-11)
            // Winter: December, January, February (12, 1-2)
            
            if (month >= 3 && month <= 5) {
                return 0; // Spring
            } else if (month >= 6 && month <= 8) {
                return 1; // Summer
            } else if (month >= 9 && month <= 11) {
                return 2; // Autumn
            } else {
                return 3; // Winter (Dec, Jan, Feb)
            }
        }
        
        case SeasonPreset::SOUTHERN_HEMISPHERE: {
            // Southern Hemisphere Meteorological Seasons (reversed months)
            // Spring: September, October, November (9-11)
            // Summer: December, January, February (12, 1-2)
            // Autumn: March, April, May (3-5)
            // Winter: June, July, August (6-8)
            
            if (month >= 9 && month <= 11) {
                return 0; // Spring
            } else if (month == 12 || month <= 2) {
                return 1; // Summer
            } else if (month >= 3 && month <= 5) {
                return 2; // Autumn
            } else {
                return 3; // Winter (Jun-Aug)
            }
        }
        
        case SeasonPreset::TROPICAL: {
            // Tropical regions: Simplified month-based approximation
            // Many tropical regions have minimal seasonal variation
            // We'll map to traditional seasons for consistency with ML system
            
            // Approximate mapping for tropical regions:
            // Dec-Feb: "Summer" (hot, often wet in tropics)
            // Mar-May: "Autumn" (transition)
            // Jun-Aug: "Winter" (cooler/drier in many tropics)
            // Sep-Nov: "Spring" (transition)
            
            if (month >= 12 || month <= 2) {
                return 1; // Summer
            } else if (month >= 3 && month <= 5) {
                return 2; // Autumn
            } else if (month >= 6 && month <= 8) {
                return 3; // Winter
            } else {
                return 0; // Spring
            }
        }
        
        case SeasonPreset::CUSTOM:
        default:
            // Custom - fall back to Northern Hemisphere for now
            // TODO: Allow users to define custom season month ranges
            return getCurrentSeason(SeasonPreset::NORTHERN_HEMISPHERE, now);
    }
}

const char* SeasonCalculator::getSeasonName(uint8_t season) {
    switch (season) {
        case 0: return "Spring";
        case 1: return "Summer";
        case 2: return "Autumn";
        case 3: return "Winter";
        default: return "Unknown";
    }
}

const char* SeasonCalculator::getSeasonIcon(uint8_t season) {
    switch (season) {
        case 0: return "🌸"; // Spring - cherry blossom
        case 1: return "☀️"; // Summer - sun
        case 2: return "🍂"; // Autumn - fallen leaf
        case 3: return "❄️"; // Winter - snowflake
        default: return "❓"; // Unknown
    }
}

const char* SeasonCalculator::getPresetName(SeasonPreset preset) {
    switch (preset) {
        case SeasonPreset::NORTHERN_HEMISPHERE: return "Northern Hemisphere";
        case SeasonPreset::SOUTHERN_HEMISPHERE: return "Southern Hemisphere";
        case SeasonPreset::TROPICAL: return "Tropical (Near Equator)";
        case SeasonPreset::CUSTOM: return "Custom";
        default: return "Unknown";
    }
}
