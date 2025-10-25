#!/usr/bin/env python3
"""
OTA Troubleshooting Guide
Interactive troubleshooting for Death Fortune Teller OTA issues.
"""

import os
import sys
import subprocess
import socket
import time

def check_esp32_power():
    """Check if ESP32 is powered on"""
    print("🔌 Checking ESP32 power...")
    print("   - Is the ESP32 powered on? (wall adapter connected)")
    print("   - Are the LEDs on the ESP32 lit?")
    print("   - Does the skull move when powered on?")
    
    response = input("   Is the ESP32 powered on? (y/n): ").lower().strip()
    if response != 'y':
        print("❌ ESP32 is not powered on")
        print("💡 Solution: Connect wall adapter and ensure ESP32 is powered")
        return False
    
    print("✅ ESP32 appears to be powered on")
    return True

def check_sd_card():
    """Check if SD card is properly inserted"""
    print("\n💾 Checking SD card...")
    print("   - Is the SD card inserted into the ESP32?")
    print("   - Does the SD card have a /config/config.txt file?")
    print("   - Does the config.txt contain WiFi credentials?")
    
    response = input("   Is the SD card properly inserted with config? (y/n): ").lower().strip()
    if response != 'y':
        print("❌ SD card issue detected")
        print("💡 Solution: Insert SD card with proper config file")
        print("   Example config.txt:")
        print("   wifi_ssid=YourNetwork")
        print("   wifi_password=YourPassword")
        print("   ota_hostname=death-fortune-teller")
        print("   ota_password=YourSecurePassword")
        return False
    
    print("✅ SD card appears to be properly configured")
    return True

def check_wifi_connection():
    """Check if ESP32 is connected to WiFi"""
    print("\n🛜 Checking WiFi connection...")
    
    # Try to discover ESP32
    try:
        result = subprocess.run([sys.executable, 'scripts/discover_esp32.py'], 
                              capture_output=True, text=True, timeout=10)
        if result.returncode == 0 and "Active ESP32 found:" in result.stdout:
            print("✅ ESP32 is connected to WiFi")
            return True
    except:
        pass
    
    print("❌ ESP32 not found on network")
    print("💡 Possible issues:")
    print("   - ESP32 not connected to WiFi")
    print("   - Wrong WiFi credentials in config.txt")
    print("   - WiFi network not available")
    print("   - ESP32 still booting (wait 30 seconds)")
    
    return False

def check_telnet_server():
    """Check if telnet server is running"""
    print("\n📡 Checking telnet server...")
    
    # Try to discover ESP32
    try:
        result = subprocess.run([sys.executable, 'scripts/discover_esp32.py'], 
                              capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            for line in result.stdout.split('\n'):
                if 'Active ESP32 found:' in line:
                    ip = line.split(':')[1].strip()
                    # Test telnet connection
                    try:
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        sock.settimeout(3)
                        result = sock.connect_ex((ip, 23))
                        sock.close()
                        if result == 0:
                            print(f"✅ Telnet server is running on {ip}")
                            return True
                    except:
                        pass
    except:
        pass
    
    print("❌ Telnet server not accessible")
    print("💡 Possible issues:")
    print("   - ESP32 not connected to WiFi")
    print("   - Telnet server not started")
    print("   - Firewall blocking port 23")
    
    return False

def run_diagnostics():
    """Run system diagnostics"""
    print("\n🔍 Running system diagnostics...")
    
    try:
        result = subprocess.run([sys.executable, 'scripts/system_status.py'], 
                              capture_output=True, text=True, timeout=15)
        if result.returncode == 0:
            print("✅ System diagnostics completed")
            print(result.stdout)
            return True
    except:
        pass
    
    print("❌ System diagnostics failed")
    return False

def main():
    print("🔧 Death Fortune Teller - OTA Troubleshooting Guide")
    print("=" * 60)
    
    # Step 1: Check power
    if not check_esp32_power():
        return 1
    
    # Step 2: Check SD card
    if not check_sd_card():
        return 1
    
    # Step 3: Check WiFi
    if not check_wifi_connection():
        print("\n💡 Try these solutions:")
        print("   1. Wait 30 seconds for ESP32 to boot")
        print("   2. Check WiFi credentials in config.txt")
        print("   3. Ensure WiFi network is available")
        print("   4. Power cycle the ESP32")
        return 1
    
    # Step 4: Check telnet
    if not check_telnet_server():
        print("\n💡 Try these solutions:")
        print("   1. Wait for ESP32 to fully boot")
        print("   2. Check if telnet server is enabled")
        print("   3. Try: python scripts/discover_esp32.py")
        return 1
    
    # Step 5: Run diagnostics
    print("\n🎉 OTA system appears to be working!")
    run_diagnostics()
    
    print("\n💡 Next steps:")
    print("   python scripts/telnet_command.py status --auto-discover")
    print("   python scripts/telnet_command.py log --auto-discover")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
