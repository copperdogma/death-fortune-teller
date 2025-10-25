#!/usr/bin/env python3
"""
Auto-Discovery OTA Upload Script
Automatically discovers ESP32 devices and performs OTA upload.
"""

import subprocess
import sys
import os
import time

def discover_esp32_ip():
    """Discover the active ESP32 IP address"""
    try:
        # Use the working discovery script without quiet mode first
        result = subprocess.run([sys.executable, 'scripts/discover_esp32.py'], 
                              capture_output=True, text=True, timeout=60)
        if result.returncode == 0 and result.stdout.strip():
            # Extract IP from the output
            output = result.stdout.strip()
            lines = output.split('\n')
            for line in lines:
                if 'Active ESP32 found:' in line:
                    # Extract IP from line like "✅ Active ESP32 found: 192.168.86.46"
                    parts = line.split(':')
                    if len(parts) > 1:
                        return parts[-1].strip()
                elif 'POTENTIAL' in line and 'Telnet: ✅' in line:
                    # Extract IP from potential device line
                    parts = line.split()
                    for part in parts:
                        if '.' in part and part.count('.') == 3:
                            return part
    except Exception as e:
        print(f"❌ Discovery failed: {e}")
    return None

def update_platformio_config(ip_address):
    """Update platformio.ini with the discovered IP address"""
    try:
        # Read current config
        with open('platformio.ini', 'r') as f:
            content = f.read()
        
        # Update upload_port
        lines = content.split('\n')
        for i, line in enumerate(lines):
            if line.strip().startswith('upload_port = '):
                lines[i] = f'upload_port = {ip_address}'
                break
        
        # Write updated config
        with open('platformio.ini', 'w') as f:
            f.write('\n'.join(lines))
        
        print(f"✅ Updated platformio.ini with IP: {ip_address}")
        return True
    except Exception as e:
        print(f"❌ Failed to update platformio.ini: {e}")
        return False

def perform_ota_upload():
    """Perform the OTA upload using PlatformIO"""
    try:
        print("🚀 Starting OTA upload...")
        result = subprocess.run(['pio', 'run', '-e', 'esp32dev_ota', '-t', 'upload'], 
                              capture_output=True, text=True, timeout=300)
        
        if result.returncode == 0:
            print("✅ OTA upload completed successfully!")
            return True
        else:
            print(f"❌ OTA upload failed:")
            print(result.stderr)
            return False
    except subprocess.TimeoutExpired:
        print("❌ OTA upload timed out")
        return False
    except Exception as e:
        print(f"❌ OTA upload failed: {e}")
        return False

def test_connection(ip_address):
    """Test connection to ESP32 before upload"""
    try:
        print(f"🔍 Testing connection to {ip_address}...")
        result = subprocess.run([sys.executable, 'scripts/telnet_command.py', 'status', '--host', ip_address], 
                              capture_output=True, text=True, timeout=10)
        
        if result.returncode == 0:
            print("✅ ESP32 is reachable and responding")
            return True
        else:
            print("⚠️ ESP32 is reachable but not responding to telnet commands")
            print("   This may be normal if ESP32 is in error state")
            return True  # Continue anyway, OTA might still work
    except Exception as e:
        print(f"⚠️ Connection test failed: {e}")
        return True  # Continue anyway

def main():
    print("🔍 Auto-Discovery OTA Upload")
    print("=" * 40)
    
    # Step 1: Discover ESP32
    print("Step 1: Discovering ESP32 devices...")
    ip_address = discover_esp32_ip()
    
    if not ip_address:
        print("❌ No active ESP32 found")
        print("💡 Check:")
        print("   - ESP32 is powered on")
        print("   - ESP32 is connected to WiFi")
        print("   - SD card is inserted with config")
        return 1
    
    print(f"✅ Found active ESP32 at: {ip_address}")
    
    # Step 2: Test connection
    print("\nStep 2: Testing connection...")
    if not test_connection(ip_address):
        print("❌ Connection test failed")
        return 1
    
    # Step 3: Update configuration
    print("\nStep 3: Updating configuration...")
    if not update_platformio_config(ip_address):
        print("❌ Failed to update configuration")
        return 1
    
    # Step 4: Perform OTA upload
    print("\nStep 4: Performing OTA upload...")
    if not perform_ota_upload():
        print("❌ OTA upload failed")
        return 1
    
    print("\n🎉 Auto-discovery OTA upload completed successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
