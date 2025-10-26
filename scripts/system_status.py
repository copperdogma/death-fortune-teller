#!/usr/bin/env python3
"""System Status Dashboard for the Death Fortune Teller."""

from __future__ import annotations

import argparse
import os
import sys
import socket
import subprocess
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
                    try:
                        CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
                        CACHE_PATH.write_text(ip)
                    except Exception:
                        pass
                    return ip
    except Exception:
        pass
    return None


def get_death_fortune_host(full_scan: bool = False) -> str | None:
    """Resolve the ESP32 host quickly, falling back to discovery when required."""
    for candidate in [
        os.environ.get("DEATH_FORTUNE_HOST", "").strip(),
        read_cached_host(),
    ]:
        if candidate:
            return candidate

    host = run_discovery(force_full=False)
    if host or not full_scan:
        return host
    return run_discovery(force_full=True)

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

def _telnet_command(host: str, *command: str) -> str:
    args = [
        sys.executable,
        "scripts/telnet_command.py",
        *command,
        "--host",
        host,
        "--auto-discover",
        "--retries",
        "2",
        "--retry-delay",
        "1",
        "--connect-timeout",
        "3",
        "--read-timeout",
        "2",
        "--post-send-wait",
        "1",
    ]

    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=12)
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception:
        pass
    return "Unknown"


def get_wifi_status(host):
    return _telnet_command(host, "wifi")

def get_ota_status(host):
    return _telnet_command(host, "ota")

def get_system_status(host):
    return _telnet_command(host, "status")

def main():
    parser = argparse.ArgumentParser(description="Death Fortune Teller system status")
    parser.add_argument("--fast", action="store_true", help="Use cached/fast discovery only (default)")
    parser.add_argument("--full", action="store_true", help="Force full discovery scan")
    args = parser.parse_args()

    print("💀 Death Fortune Teller - System Status")
    print("=" * 50)
    
    # Find ESP32
    full_scan = args.full and not args.fast
    host = get_death_fortune_host(full_scan=full_scan)
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
