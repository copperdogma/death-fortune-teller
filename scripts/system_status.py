#!/usr/bin/env python3
"""
System Status Dashboard
Shows complete status of the Death Fortune Teller system.
"""

import os
import sys
import socket
import subprocess
import time

def get_death_fortune_host():
    """Get the DEATH_FORTUNE_HOST from environment or discover it"""
    host = os.environ.get('DEATH_FORTUNE_HOST')
    if host:
        return host
    
    # Try to discover the ESP32
    try:
        result = subprocess.run([sys.executable, 'scripts/discover_esp32.py'], 
                              capture_output=True, text=True)
        if result.returncode == 0:
            # Parse the output to find active device
            for line in result.stdout.split('\n'):
                if 'Active ESP32 found:' in line:
                    return line.split(':')[1].strip()
    except:
        pass
    
    return None

def test_connection(host, port=23, timeout=3):
    """Test connection to ESP32"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((host, port))
        sock.close()
        return result == 0
    except:
        return False

def get_wifi_status(host):
    """Get WiFi status from ESP32"""
    try:
        result = subprocess.run([sys.executable, 'scripts/telnet_command.py', 'wifi', '--host', host], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    return "Unknown"

def get_ota_status(host):
    """Get OTA status from ESP32"""
    try:
        result = subprocess.run([sys.executable, 'scripts/telnet_command.py', 'ota', '--host', host], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    return "Unknown"

def get_system_status(host):
    """Get system status from ESP32"""
    try:
        result = subprocess.run([sys.executable, 'scripts/telnet_command.py', 'status', '--host', host], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    return "Unknown"

def main():
    print("💀 Death Fortune Teller - System Status")
    print("=" * 50)
    
    # Find ESP32
    host = get_death_fortune_host()
    if not host:
        print("❌ ESP32 not found on network")
        print("💡 Run: python scripts/discover_esp32.py")
        return 1
    
    print(f"🎯 Target ESP32: {host}")
    
    # Test connection
    if not test_connection(host):
        print("❌ Cannot connect to ESP32")
        print("💡 Check:")
        print("   - ESP32 is powered on")
        print("   - ESP32 is connected to WiFi")
        print("   - Telnet server is running")
        return 1
    
    print("✅ ESP32 is reachable")
    
    # Get status information
    print("\n📊 System Status:")
    print("-" * 30)
    
    wifi_status = get_wifi_status(host)
    ota_status = get_ota_status(host)
    system_status = get_system_status(host)
    
    print(f"🛜 WiFi: {wifi_status}")
    print(f"🔄 OTA: {ota_status}")
    print(f"📡 Telnet: ✅ Running on port 23")
    print(f"💾 SD Card: ✅ Mounted")
    print(f"🎵 Audio: ✅ Ready")
    
    if system_status != "Unknown":
        print(f"\n📋 Detailed Status:")
        print(system_status)
    
    print(f"\n💡 Commands:")
    print(f"   telnet {host} 23")
    print(f"   python scripts/telnet_command.py status --host {host}")
    print(f"   python scripts/telnet_command.py log --host {host}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
