#ifndef ESP32_RANDOM_H
#define ESP32_RANDOM_H

#include <Arduino.h>
#include <esp_system.h>

/**
 * ESP32 Hardware Random Number Generator (TRNG)
 * 
 * Uses ESP32's true random number generator which generates entropy from:
 * - RF noise from WiFi/Bluetooth hardware
 * - Internal clock jitter
 * - Thermal noise
 * 
 * Benefits over software PRNG:
 * - True randomness (not pseudo-random)
 * - Cryptographically secure
 * - No seeding required
 * - Better distribution
 * - No computation overhead
 */
class ESP32_Random {
public:
    /**
     * Get a random 32-bit unsigned integer
     * @return Random uint32_t value
     */
    static uint32_t random32() {
        return esp_random();
    }
    
    /**
     * Get a random 64-bit unsigned integer
     * @return Random uint64_t value
     */
    static uint64_t random64() {
        uint64_t high = esp_random();
        uint64_t low = esp_random();
        return (high << 32) | low;
    }
    
    /**
     * Get a random value in range [0, max)
     * @param max Upper bound (exclusive)
     * @return Random value in range [0, max)
     */
    static uint32_t randomRange(uint32_t max) {
        if (max == 0) return 0;
        return esp_random() % max;
    }
    
    /**
     * Get a random value in range [min, max)
     * @param min Lower bound (inclusive)
     * @param max Upper bound (exclusive)
     * @return Random value in range [min, max)
     */
    static uint32_t randomRange(uint32_t min, uint32_t max) {
        if (min >= max) return min;
        return min + (esp_random() % (max - min));
    }
    
    /**
     * Get a random float in range [0.0, 1.0)
     * @return Random float value
     */
    static float randomFloat() {
        return (float)esp_random() / (float)UINT32_MAX;
    }
    
    /**
     * Get a random float in range [min, max)
     * @param min Lower bound (inclusive)
     * @param max Upper bound (exclusive)
     * @return Random float value in range
     */
    static float randomFloat(float min, float max) {
        return min + randomFloat() * (max - min);
    }
    
    /**
     * Fill a buffer with random bytes
     * @param buffer Pointer to buffer to fill
     * @param length Number of bytes to generate
     */
    static void randomBytes(uint8_t* buffer, size_t length) {
        esp_fill_random(buffer, length);
    }
    
    /**
     * Generate a random hexadecimal string (useful for IDs, tokens, etc.)
     * @param buffer Output buffer (must be at least length*2 + 1 bytes)
     * @param length Number of random bytes (output will be length*2 hex chars)
     */
    static void randomHexString(char* buffer, size_t length) {
        uint8_t* bytes = new uint8_t[length];
        esp_fill_random(bytes, length);
        
        for (size_t i = 0; i < length; i++) {
            sprintf(&buffer[i * 2], "%02x", bytes[i]);
        }
        buffer[length * 2] = '\0';
        
        delete[] bytes;
    }
    
    /**
     * Generate a unique device ID based on MAC address and random bytes
     * Format: "aquarium-XXXXXX" where X is hex from MAC + random
     * @param buffer Output buffer (must be at least 20 bytes)
     */
    static void generateDeviceID(char* buffer, size_t bufferSize) {
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        
        // Use last 3 bytes of MAC + 1 random byte for uniqueness
        uint8_t random = esp_random() & 0xFF;
        
        snprintf(buffer, bufferSize, "aquarium-%02x%02x%02x%02x",
                 mac[3], mac[4], mac[5], random);
    }
    
    /**
     * Generate a UUID-like string (32 hex chars)
     * Format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     * @param buffer Output buffer (must be at least 37 bytes)
     */
    static void generateUUID(char* buffer) {
        uint8_t uuid[16];
        esp_fill_random(uuid, 16);
        
        // UUID v4 format
        uuid[6] = (uuid[6] & 0x0F) | 0x40; // Version 4
        uuid[8] = (uuid[8] & 0x3F) | 0x80; // Variant 1
        
        snprintf(buffer, 37, 
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 uuid[0], uuid[1], uuid[2], uuid[3],
                 uuid[4], uuid[5], uuid[6], uuid[7],
                 uuid[8], uuid[9], uuid[10], uuid[11],
                 uuid[12], uuid[13], uuid[14], uuid[15]);
    }
    
    /**
     * Generate a shorter unique ID (8 hex characters)
     * @param buffer Output buffer (must be at least 9 bytes)
     */
    static void generateShortID(char* buffer) {
        uint32_t id = esp_random();
        snprintf(buffer, 9, "%08x", id);
    }
};

#endif
