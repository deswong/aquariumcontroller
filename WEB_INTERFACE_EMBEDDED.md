# Embedded Web Interface

## Overview

The web interface is now **embedded directly in the firmware** as compressed data in program memory (PROGMEM). This means the web interface:

- ✅ **Always available** - survives reboots
- ✅ **Persists across firmware updates** - no need to re-upload files
- ✅ **No SPIFFS dependency** - web interface works even if SPIFFS fails
- ✅ **Fast serving** - served directly from flash memory
- ✅ **Cached efficiently** - gzip-compressed, 24-hour browser cache

## Technical Details

### Compression
- Original HTML size: **140,560 bytes**
- Compressed size: **20,572 bytes** (14.6% of original)
- Compression: gzip level 9
- Storage: PROGMEM (flash memory)

### Implementation
The HTML file is converted to a C++ header file containing a byte array:

```cpp
const uint32_t index_html_gz_len = 20572;
const uint8_t index_html_gz[] PROGMEM = {
  0x1f, 0x8b, 0x08, 0x00, ...
};
```

The web server serves this directly with gzip encoding:

```cpp
server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/html", index_html_gz, index_html_gz_len
    );
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "max-age=86400");
    request->send(response);
});
```

## Updating the Web Interface

When you modify `data/index.html`, you must regenerate the embedded version:

### Step 1: Update index.html
Edit `data/index.html` with your changes.

### Step 2: Regenerate Header File
```bash
python tools/html_to_progmem.py data/index.html include/WebInterface.h index_html
```

This will output:
```
Original HTML size: 140560 bytes
Compressed size: 20572 bytes (14% of original)
Generated: include/WebInterface.h
```

### Step 3: Compile and Upload Firmware
```bash
pio run --environment esp32dev --target upload
```

That's it! The new web interface is now permanently embedded in the firmware.

## SPIFFS Usage

SPIFFS is still used for:
- **Event logs** (`/spiffs/events.log`)
- **Water change history** (if saved to SPIFFS)
- **Any runtime-generated files**

But the main web interface (index.html) is **completely independent** of SPIFFS.

## Benefits

### 1. Reliability
- Web interface always works, even if SPIFFS is corrupted
- No "file not found" errors after reboot
- No need to maintain SPIFFS filesystem

### 2. Convenience
- One-step deployment: just upload firmware
- No need to run `uploadfs` target separately
- Firmware updates don't erase web interface

### 3. Performance
- Faster load times (no file system access)
- Compressed transmission (20KB vs 140KB)
- Browser caching reduces repeat loads

### 4. Flash Efficiency
- Only 20KB of flash used (vs 192KB SPIFFS partition)
- Freed up 172KB of flash for other uses
- Could reduce SPIFFS partition or add more features

## Comparison

### Before (SPIFFS-based):
```
Upload Steps:
1. pio run --target upload         (upload firmware)
2. pio run --target uploadfs        (upload filesystem)
3. After every reboot: repeat step 2

Flash Usage:
- Firmware: 1,103 KB
- SPIFFS Partition: 192 KB
- index.html in SPIFFS: 140 KB
- Total: 1,295 KB

Issues:
❌ Files disappear after reboot
❌ Two-step upload process
❌ SPIFFS corruption risk
❌ Must remember to uploadfs
```

### After (PROGMEM embedded):
```
Upload Steps:
1. pio run --target upload         (upload firmware only!)

Flash Usage:
- Firmware: 1,121 KB (includes 20KB compressed HTML)
- SPIFFS Partition: 192 KB (for logs only)
- Total: 1,313 KB

Benefits:
✅ Web interface always available
✅ One-step deployment
✅ Survives all reboots
✅ No SPIFFS dependency
✅ Faster serving
```

## Tools

### html_to_progmem.py
Located in `tools/html_to_progmem.py`, this Python script:
- Reads HTML file
- Compresses with gzip (level 9)
- Generates C++ header with byte array
- Outputs compression statistics

Usage:
```bash
python tools/html_to_progmem.py <input.html> [output.h] [variable_name]
```

Example:
```bash
python tools/html_to_progmem.py data/index.html include/WebInterface.h index_html
```

## Future Enhancements

Potential improvements:
1. **Split CSS/JS**: Embed CSS and JavaScript separately for modular updates
2. **Multiple pages**: Embed additional pages (help, about, etc.)
3. **Images**: Embed logos and icons
4. **Compression options**: Add Brotli compression for even smaller size
5. **Build automation**: Auto-generate header during PlatformIO build

## Migration Notes

### From SPIFFS-based System
If migrating from a SPIFFS-based web interface:

1. The old index.html in SPIFFS is **ignored**
2. SPIFFS is still mounted for event logs
3. No data loss - all settings remain in NVS
4. Web interface URL unchanged: http://[device-ip]/

### Backwards Compatibility
The firmware still supports SPIFFS for:
- Event logging (`EventLogger.cpp`)
- Water change history
- Any custom files you create

Only the main web interface (/) is now served from PROGMEM.

## Troubleshooting

### "Web interface not loading"
1. Verify firmware uploaded successfully
2. Check serial monitor for "Web interface embedded in firmware" message
3. Clear browser cache (Ctrl+Shift+R)
4. Verify device IP address

### "After firmware update, old interface shows"
- Browser cache issue
- Hard refresh: Ctrl+Shift+R (Windows) or Cmd+Shift+R (Mac)
- Or add `?v=2` to URL: http://192.168.1.128/?v=2

### "Want to update HTML"
1. Edit `data/index.html`
2. Run: `python tools/html_to_progmem.py data/index.html include/WebInterface.h index_html`
3. Upload firmware: `pio run --environment esp32dev --target upload`

## Summary

The embedded web interface solution provides a **robust, reliable, and convenient** way to serve the aquarium controller's web interface. By storing the HTML in firmware instead of SPIFFS, you eliminate the most common source of issues (missing files after reboot) while improving performance and simplifying deployment.

**No more "file not found" errors!** 🎉
