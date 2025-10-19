#!/usr/bin/env python3
"""
Convert HTML file to C++ PROGMEM string for embedding in firmware.
This allows the web interface to persist across reboots and firmware updates.
"""

import sys
import gzip
from pathlib import Path

def html_to_progmem(input_file, output_file, var_name="index_html"):
    """Convert HTML file to gzipped PROGMEM C++ header."""
    
    # Read the HTML file
    html_path = Path(input_file)
    if not html_path.exists():
        print(f"Error: Input file '{input_file}' not found!")
        return False
    
    html_content = html_path.read_bytes()
    print(f"Original HTML size: {len(html_content)} bytes")
    
    # Gzip compress the content
    compressed = gzip.compress(html_content, compresslevel=9)
    print(f"Compressed size: {len(compressed)} bytes ({len(compressed)*100//len(html_content)}% of original)")
    
    # Generate C++ header file
    output_path = Path(output_file)
    
    with output_path.open('w', encoding='utf-8') as f:
        f.write("// Auto-generated file - DO NOT EDIT\n")
        f.write("// Generated from: " + html_path.name + "\n")
        f.write(f"// Original size: {len(html_content)} bytes\n")
        f.write(f"// Compressed size: {len(compressed)} bytes\n\n")
        f.write("#ifndef WEB_INTERFACE_H\n")
        f.write("#define WEB_INTERFACE_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write("// Gzip-compressed HTML content stored in PROGMEM\n")
        f.write(f"const uint32_t {var_name}_gz_len = {len(compressed)};\n")
        f.write(f"const uint8_t {var_name}_gz[] PROGMEM = {{\n")
        
        # Write bytes in rows of 16
        for i in range(0, len(compressed), 16):
            chunk = compressed[i:i+16]
            hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f"  {hex_values},\n")
        
        f.write("};\n\n")
        f.write("#endif // WEB_INTERFACE_H\n")
    
    print(f"Generated: {output_file}")
    print(f"\nTo use in your code:")
    print(f'  request->send_P(200, "text/html", {var_name}_gz, {var_name}_gz_len);')
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python html_to_progmem.py <input.html> [output.h] [variable_name]")
        print("\nExample:")
        print("  python html_to_progmem.py data/index.html include/WebInterface.h index_html")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "include/WebInterface.h"
    var_name = sys.argv[3] if len(sys.argv) > 3 else "index_html"
    
    if html_to_progmem(input_file, output_file, var_name):
        print("\n✓ Success! Now update WebServer.cpp to serve from PROGMEM instead of SPIFFS.")
    else:
        sys.exit(1)
