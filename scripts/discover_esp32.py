#!/usr/bin/env python3
"""
ESP32 Discovery Tool
Finds all ESP32 devices on the network and shows their status.
"""

import socket
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

def ping_host(ip):
    """Ping a single IP address"""
    try:
        result = subprocess.run(['ping', '-c', '1', '-W', '1000', ip], 
                              capture_output=True, timeout=2)
        return ip if result.returncode == 0 else None
    except:
        return None

def test_telnet(ip, port=23, timeout=2):
    """Test if telnet port is open"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((ip, port))
        sock.close()
        return result == 0
    except:
        return False

def get_hostname(ip):
    """Get hostname for IP"""
    try:
        result = subprocess.run(['nslookup', ip], capture_output=True, timeout=2)
        output = result.stdout.decode()
        for line in output.split('\n'):
            if 'name' in line.lower():
                return line.split()[-1].rstrip('.')
    except:
        pass
    return None

def discover_esp32_devices():
    """Discover all ESP32 devices on the network"""
    print("🔍 Scanning network for ESP32 devices...")
    
    # Get local network range
    import subprocess
    try:
        result = subprocess.run(['route', '-n', 'get', 'default'], capture_output=True, text=True)
        for line in result.stdout.split('\n'):
            if 'interface:' in line:
                interface = line.split(':')[1].strip()
                break
        else:
            interface = 'en0'  # Default on macOS
        
        # Get IP range
        result = subprocess.run(['ifconfig', interface], capture_output=True, text=True)
        for line in result.stdout.split('\n'):
            if 'inet ' in line and '127.0.0.1' not in line:
                ip = line.split()[1]
                network = '.'.join(ip.split('.')[:-1]) + '.'
                break
        else:
            network = '192.168.86.'
    except:
        network = '192.168.86.'
    
    # Scan network
    devices = []
    with ThreadPoolExecutor(max_workers=50) as executor:
        futures = []
        for i in range(20, 60):
            ip = f"{network}{i}"
            futures.append(executor.submit(ping_host, ip))
        
        for future in as_completed(futures):
            ip = future.result()
            if ip:
                devices.append(ip)
    
    print(f"📡 Found {len(devices)} active devices")
    
    # Test each device
    results = []
    for ip in devices:
        hostname = get_hostname(ip)
        has_telnet = test_telnet(ip)
        
        status = "UNKNOWN"
        if "death-fortune-teller" in (hostname or ""):
            status = "ACTIVE" if has_telnet else "STALE"
        elif "espressif" in (hostname or ""):
            status = "ESP32" if has_telnet else "ESP32 (no telnet)"
        elif has_telnet:
            # If it has telnet but no recognized hostname, it might be our ESP32
            status = "POTENTIAL"
        
        results.append({
            'ip': ip,
            'hostname': hostname,
            'has_telnet': has_telnet,
            'status': status
        })
    
    return results

def main():
    devices = discover_esp32_devices()
    
    if not devices:
        print("❌ No ESP32 devices found on network")
        print("💡 Check:")
        print("   - ESP32 is powered on")
        print("   - ESP32 is connected to WiFi")
        print("   - SD card is inserted with config")
        return 1
    
    print("\n📋 ESP32 Device Status:")
    print("-" * 60)
    
    active_device = None
    for device in devices:
        status_icon = "🟢" if device['status'] == "ACTIVE" else "🟡" if "ESP32" in device['status'] or device['status'] == "POTENTIAL" else "🔴"
        telnet_status = "✅" if device['has_telnet'] else "❌"
        
        print(f"{status_icon} {device['ip']:<15} {device['hostname'] or 'Unknown':<20} {device['status']:<10} Telnet: {telnet_status}")
        
        if device['status'] == "ACTIVE":
            active_device = device
        elif device['status'] == "POTENTIAL" and not active_device:
            # Use potential device if no active device found
            active_device = device
    
    if active_device:
        print(f"\n✅ Active ESP32 found: {active_device['ip']}")
        print(f"💡 Set: export DEATH_FORTUNE_HOST={active_device['ip']}")
        print(f"💡 Test: python scripts/telnet_command.py status")
        
        # For automation: output just the IP address
        if "--quiet" in sys.argv:
            print(active_device['ip'])
    else:
        print("\n⚠️  No active ESP32 found")
        print("💡 Check ESP32 is connected to WiFi and telnet server is running")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
