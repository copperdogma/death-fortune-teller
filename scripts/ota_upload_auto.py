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
    """Use the discovery script and return the best match IP."""
    try:
        result = subprocess.run(
            [sys.executable, "scripts/discover_esp32.py"],  # prints friendly table
            capture_output=True,
            text=True,
            timeout=60,
        )
        output = result.stdout or ""
        if result.returncode != 0 or "Active ESP32 found:" not in output:
            return None
        for line in output.splitlines():
            if "Active ESP32 found:" in line:
                return line.split(":")[-1].strip()
    except Exception as exc:
        print(f"❌ Discovery failed: {exc}")
    return None


def test_connection(ip_address):
    """Lightweight check that RemoteDebug responds."""
    try:
        print(f"🔍 Testing connection to {ip_address}...")
        import socket

        sock = socket.create_connection((ip_address, 23), timeout=2)
        sock.settimeout(0.5)
        try:
            sock.sendall(b"\n")
            time.sleep(0.2)
            banner = sock.recv(512)
        except Exception:
            banner = b""
        finally:
            sock.close()

        text = banner.decode("utf-8", errors="ignore")
        if "RemoteDebug" in text or "WiFi:" in text or "🛜" in text:
            print("✅ RemoteDebug responded")
        else:
            print("⚠️ Telnet responded but handshake was unexpected; continuing anyway")
        return True
    except Exception as exc:
        print(f"⚠️ Connection test failed: {exc}")
        return True  # still attempt OTA; the invite may succeed even if telnet doesn’t


def perform_ota_upload(ip_address):
    """Perform the OTA upload using PlatformIO, targeting the discovered IP."""
    try:
        print("🚀 Starting OTA upload...")
        env = os.environ.copy()
        env["DEATH_FORTUNE_HOST"] = ip_address
        result = subprocess.run(
            ["pio", "run", "-e", "esp32dev_ota", "-t", "upload"],
            capture_output=True,
            text=True,
            timeout=300,
            env=env,
        )
        if result.returncode == 0:
            print("✅ OTA upload completed successfully!")
            return True
        print("❌ OTA upload failed:")
        print(result.stdout)
        print(result.stderr)
        return False
    except subprocess.TimeoutExpired:
        print("❌ OTA upload timed out")
        return False
    except Exception as exc:
        print(f"❌ OTA upload failed: {exc}")
        return False

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
    
    # Step 3: Perform OTA upload
    print("\nStep 3: Performing OTA upload...")
    if not perform_ota_upload(ip_address):
        print("❌ OTA upload failed")
        return 1
    
    print("\n🎉 Auto-discovery OTA upload completed successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
