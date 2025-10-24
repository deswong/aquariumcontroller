#ifndef ESP32_TIMER_H
#define ESP32_TIMER_H

#include <Arduino.h>
#include <esp_timer.h>

/**
 * ESP32 Hardware Timer Helper
 * 
 * Uses ESP32's high-resolution hardware timers for precise timing operations.
 * ESP32-S3 has 4 hardware timer groups with microsecond precision.
 * 
 * Benefits over software timing (millis/micros):
 * - Microsecond precision (vs millisecond)
 * - Hardware interrupts (not affected by task scheduling)
 * - More deterministic timing
 * - Lower jitter (<1µs vs 1-10ms)
 * - No polling required
 * 
 * Use cases:
 * - Relay PWM control (time-proportional)
 * - Precise sensor sampling
 * - Accurate timing measurements
 * - Periodic callbacks with low jitter
 */
class ESP32_Timer {
public:
    /**
     * Callback function type
     * @param arg User argument passed to callback
     */
    typedef void (*TimerCallback)(void* arg);

private:
    esp_timer_handle_t timer;
    TimerCallback callback;
    void* userArg;
    bool running;
    uint64_t periodUs;
    bool oneShot;
    const char* name;
    
    // Static callback wrapper for esp_timer
    static void timerCallbackWrapper(void* arg) {
        ESP32_Timer* instance = static_cast<ESP32_Timer*>(arg);
        if (instance && instance->callback) {
            instance->callback(instance->userArg);
        }
    }

public:
    /**
     * Constructor
     * @param timerName Name for debugging (optional)
     */
    ESP32_Timer(const char* timerName = "ESP32Timer") 
        : timer(nullptr), callback(nullptr), userArg(nullptr), 
          running(false), periodUs(0), oneShot(false), name(timerName) {
    }
    
    ~ESP32_Timer() {
        stop();
        if (timer) {
            esp_timer_delete(timer);
        }
    }
    
    /**
     * Initialize timer with callback
     * @param cb Callback function to call on timer expiry
     * @param arg User argument passed to callback
     * @param periodic true for periodic timer, false for one-shot
     * @return true if successful
     */
    bool begin(TimerCallback cb, void* arg = nullptr, bool periodic = true) {
        if (timer != nullptr) {
            Serial.printf("Timer '%s' already initialized\n", name);
            return false;
        }
        
        callback = cb;
        userArg = arg;
        oneShot = !periodic;
        
        esp_timer_create_args_t timerConfig = {
            .callback = &timerCallbackWrapper,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = name,
            .skip_unhandled_events = false
        };
        
        esp_err_t err = esp_timer_create(&timerConfig, &timer);
        if (err != ESP_OK) {
            Serial.printf("ERROR: Failed to create timer '%s': %d\n", name, err);
            return false;
        }
        
        Serial.printf("Timer '%s' initialized (%s)\n", 
                     name, oneShot ? "one-shot" : "periodic");
        return true;
    }
    
    /**
     * Start timer with microsecond period
     * @param intervalUs Interval in microseconds
     * @return true if successful
     */
    bool start(uint64_t intervalUs) {
        if (!timer) {
            Serial.printf("ERROR: Timer '%s' not initialized\n", name);
            return false;
        }
        
        if (running) {
            stop();
        }
        
        periodUs = intervalUs;
        
        esp_err_t err;
        if (oneShot) {
            err = esp_timer_start_once(timer, periodUs);
        } else {
            err = esp_timer_start_periodic(timer, periodUs);
        }
        
        if (err != ESP_OK) {
            Serial.printf("ERROR: Failed to start timer '%s': %d\n", name, err);
            return false;
        }
        
        running = true;
        return true;
    }
    
    /**
     * Start timer with millisecond period (convenience)
     * @param intervalMs Interval in milliseconds
     * @return true if successful
     */
    bool startMs(uint32_t intervalMs) {
        return start(intervalMs * 1000ULL);
    }
    
    /**
     * Start timer with second period (convenience)
     * @param intervalSec Interval in seconds
     * @return true if successful
     */
    bool startSec(uint32_t intervalSec) {
        return start(intervalSec * 1000000ULL);
    }
    
    /**
     * Stop timer
     */
    void stop() {
        if (timer && running) {
            esp_timer_stop(timer);
            running = false;
        }
    }
    
    /**
     * Check if timer is running
     */
    bool isRunning() const {
        return running;
    }
    
    /**
     * Get current period in microseconds
     */
    uint64_t getPeriodUs() const {
        return periodUs;
    }
    
    /**
     * Change period (restarts timer if running)
     * @param newPeriodUs New period in microseconds
     */
    bool setPeriod(uint64_t newPeriodUs) {
        bool wasRunning = running;
        if (wasRunning) {
            stop();
        }
        
        periodUs = newPeriodUs;
        
        if (wasRunning) {
            return start(periodUs);
        }
        return true;
    }
    
    /**
     * Get high-resolution timestamp in microseconds
     * Uses ESP32 hardware timer (more accurate than micros())
     */
    static uint64_t getTimestamp() {
        return esp_timer_get_time();
    }
    
    /**
     * Get high-resolution timestamp in milliseconds
     */
    static uint64_t getTimestampMs() {
        return esp_timer_get_time() / 1000ULL;
    }
    
    /**
     * High-precision delay in microseconds
     * More accurate than delayMicroseconds() for short delays
     * @param us Microseconds to delay
     */
    static void delayUs(uint64_t us) {
        uint64_t start = esp_timer_get_time();
        while ((esp_timer_get_time() - start) < us) {
            // Busy wait for precise timing
        }
    }
    
    /**
     * Measure execution time of a function
     * @param func Function to measure
     * @return Execution time in microseconds
     */
    template<typename F>
    static uint64_t measureTime(F func) {
        uint64_t start = esp_timer_get_time();
        func();
        return esp_timer_get_time() - start;
    }
    
    /**
     * Print timer information
     */
    void printInfo() const {
        Serial.printf("Timer '%s' Info:\n", name);
        Serial.printf("  Type: %s\n", oneShot ? "One-shot" : "Periodic");
        Serial.printf("  Period: %llu µs (%.3f ms)\n", periodUs, periodUs / 1000.0);
        Serial.printf("  Status: %s\n", running ? "Running" : "Stopped");
        Serial.printf("  Callback: %s\n", callback ? "Set" : "None");
    }
};

/**
 * Stopwatch class for high-precision timing measurements
 */
class ESP32_Stopwatch {
private:
    uint64_t startTime;
    uint64_t stopTime;
    bool running;

public:
    ESP32_Stopwatch() : startTime(0), stopTime(0), running(false) {}
    
    void start() {
        startTime = ESP32_Timer::getTimestamp();
        running = true;
    }
    
    void stop() {
        if (running) {
            stopTime = ESP32_Timer::getTimestamp();
            running = false;
        }
    }
    
    void reset() {
        startTime = 0;
        stopTime = 0;
        running = false;
    }
    
    uint64_t elapsedUs() const {
        if (running) {
            return ESP32_Timer::getTimestamp() - startTime;
        } else {
            return stopTime - startTime;
        }
    }
    
    float elapsedMs() const {
        return elapsedUs() / 1000.0f;
    }
    
    float elapsedSec() const {
        return elapsedUs() / 1000000.0f;
    }
    
    bool isRunning() const {
        return running;
    }
};

#endif
