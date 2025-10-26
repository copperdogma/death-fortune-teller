#!/usr/bin/env python3
"""OTA Troubleshooting Guide for the Death Fortune Teller."""

from __future__ import annotations

import argparse
import os
import sys
import subprocess
import socket
from pathlib import Path

CACHE_PATH = Path(".pio/death_fortune_host")


def read_cached_host() -> str | None:
    try:
        if CACHE_PATH.exists():
            cached = CACHE_PATH.read_text().strip()
            if cached:
                return cached
    except Exception:
        pass
    return None


def write_cached_host(host: str) -> None:
    if not host:
        return
    try:
        CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
        CACHE_PATH.write_text(host.strip())
    except Exception:
        pass


def run_discovery(force_full: bool = False) -> str | None:
    args = [sys.executable, "scripts/discover_esp32.py", "--fast", "--quiet"]
    if force_full:
        args = [sys.executable, "scripts/discover_esp32.py", "--quiet"]
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=35)
        if result.returncode == 0:
            output = (result.stdout or "").strip()
            if output:
                ip = output.splitlines()[-1].strip()
                if ip:
                    write_cached_host(ip)
                    return ip
    except Exception:
        pass
    return None

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

def resolve_host(full: bool = False) -> str | None:
    env_host = os.environ.get("DEATH_FORTUNE_HOST", "").strip()
    if env_host:
        return env_host

    cached = read_cached_host()
    if cached:
        return cached

    host = run_discovery(force_full=False)
    if host or not full:
        return host
    return run_discovery(force_full=True)


def check_wifi_connection(full=False):
    """Check if ESP32 is connected to WiFi"""
    print("\n🛜 Checking WiFi connection...")
    
    host = resolve_host(full=full)
    if host:
        print("✅ ESP32 is connected to WiFi")
        return True
    
    print("❌ ESP32 not found on network")
    print("💡 Possible issues:")
    print("   - ESP32 not connected to WiFi")
    print("   - Wrong WiFi credentials in config.txt")
    print("   - WiFi network not available")
    print("   - ESP32 still booting (wait 30 seconds)")
    
    return False

def check_telnet_server(full=False):
    """Check if telnet server is running"""
    print("\n📡 Checking telnet server...")
    
    ip = resolve_host(full=full)
    if ip:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2)
            res = sock.connect_ex((ip, 23))
            sock.close()
            if res == 0:
                print(f"✅ Telnet server is running on {ip}")
                return True
        except Exception:
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
    parser = argparse.ArgumentParser(description="Death Fortune Teller troubleshooting")
    parser.add_argument("--fast", action="store_true", help="Use cached/fast discovery only")
    args = parser.parse_args()

    print("🔧 Death Fortune Teller - OTA Troubleshooting Guide")
    print("=" * 60)
    
    # Step 1: Check power
    if not check_esp32_power():
        return 1
    
    # Step 2: Check SD card
    if not check_sd_card():
        return 1
    
    # Step 3: Check WiFi
    if not check_wifi_connection(full=not args.fast):
        print("\n💡 Try these solutions:")
        print("   1. Wait 30 seconds for ESP32 to boot")
        print("   2. Check WiFi credentials in config.txt")
        print("   3. Ensure WiFi network is available")
        print("   4. Power cycle the ESP32")
        return 1
    
    # Step 4: Check telnet
    if not check_telnet_server(full=not args.fast):
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
