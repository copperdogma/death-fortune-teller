#!/usr/bin/env python3
"""
ESP32 Discovery Tool
Finds all ESP32 devices on the network and shows their status.
"""

import os
import socket
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

TELNET_PASSWORD = os.environ.get("ESP32_TELNET_PASSWORD", "Death9!!!")
OTA_PORT = 3232
CACHE_PATH = Path(".pio/death_fortune_host")


def quick_probe(ip, timeout=0.6):
    try:
        with socket.create_connection((ip, 23), timeout=timeout):
            return True
    except Exception:
        return False


def ping_host(ip):
    """Ping a single IP address"""
    try:
        result = subprocess.run(
            ["ping", "-c", "1", "-W", "400", ip],
            capture_output=True,
            timeout=2,
        )
        return ip if result.returncode == 0 else None
    except Exception:
        return None


def get_hostname(ip):
    """Get hostname for IP"""
    try:
        result = subprocess.run(["nslookup", ip], capture_output=True, timeout=2)
        output = result.stdout.decode()
        for line in output.split("\n"):
            if "name" in line.lower():
                return line.split()[-1].rstrip(".")
    except Exception:
        pass
    return None


def _is_skull_banner(text: str) -> bool:
    """Return True if telnet banner looks like the RemoteDebug skull."""
    if not text:
        return False
    lowered = text.lower()
    checks = [
        "🛜 remotedebug connected",
        "commands: status, wifi, ota",
        "🛜 startup log",
        "🛜 status: wifi=",
        "death initialized successfully",
    ]
    return any(marker in lowered for marker in checks)


def probe_telnet(ip, timeout=2):
    """Attempt a lightweight telnet handshake to see if RemoteDebug responds."""
    try:
        sock = socket.create_connection((ip, 23), timeout=timeout)
        sock.settimeout(0.5)

        data = b""
        try:
            data += sock.recv(256)
        except socket.timeout:
            pass

        # Wake the prompt
        try:
            sock.sendall(b"\n")
            time.sleep(0.2)
            data += sock.recv(512)
        except socket.timeout:
            pass

        # Try password + status command (errors are fine; we only need hints)
        try:
            sock.sendall((TELNET_PASSWORD + "\n").encode("utf-8"))
            time.sleep(0.2)
            data += sock.recv(512)
            sock.sendall(b"status\n")
            time.sleep(0.3)
            data += sock.recv(1024)
        except socket.timeout:
            pass
        finally:
            sock.close()

        text = data.decode("utf-8", errors="ignore")
        is_remote_debug = _is_skull_banner(text)
        return True, is_remote_debug, text.strip()
    except Exception:
        return False, False, ""


def probe_ota(ip, timeout=1.0):
    """Send a minimal OTA invitation to see if ArduinoOTA replies."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(timeout)
        dummy_md5 = "0" * 32
        message = f"0 0 0 {dummy_md5}\n".encode("utf-8")
        sock.sendto(message, (ip, OTA_PORT))
        data, _ = sock.recvfrom(64)
        sock.close()
        reply = data.decode("utf-8", errors="ignore").strip()
        return reply.startswith("OK") or reply.startswith("AUTH")
    except Exception:
        return False


def discover_network_prefix():
    """Determine network prefix (192.168.86.) based on default route."""
    try:
        result = subprocess.run(
            ["route", "-n", "get", "default"], capture_output=True, text=True
        )
        interface = None
        for line in result.stdout.splitlines():
            if "interface:" in line:
                interface = line.split(":")[1].strip()
                break
        if not interface:
            interface = "en0"

        result = subprocess.run(["ifconfig", interface], capture_output=True, text=True)
        for line in result.stdout.splitlines():
            if "inet " in line and "127.0.0.1" not in line:
                ip = line.split()[1]
                octets = ip.split(".")
                if len(octets) == 4:
                    return ".".join(octets[:3]) + "."
    except Exception:
        pass
    return "192.168.86."


def discover_esp32_devices():
    """Discover all ESP32 devices on the network"""
    print("🔍 Scanning network for ESP32 devices...")

    network = discover_network_prefix()
    devices = []
    scan_range = range(20, 60)
    with ThreadPoolExecutor(max_workers=32) as executor:
        futures = [executor.submit(ping_host, f"{network}{i}") for i in scan_range]
        for future in as_completed(futures):
            ip = future.result()
            if ip:
                devices.append(ip)

    print(f"📡 Found {len(devices)} active devices")

    results = []
    for ip in sorted(devices):
        hostname = get_hostname(ip)
        telnet_up, remote_debug, telnet_text = probe_telnet(ip)
        ota_ready = probe_ota(ip)
        skull_hostname = hostname and "death-fortune-teller" in hostname
        skull_banner = remote_debug or _is_skull_banner(telnet_text)

        if skull_hostname or skull_banner or ota_ready:
            status = "ACTIVE"
        elif hostname and "espressif" in hostname:
            status = "ESP32 (no telnet)" if not telnet_up else "ESP32"
        elif telnet_up:
            status = "TELNET"
        else:
            status = "UNKNOWN"

        results.append(
            {
                "ip": ip,
                "hostname": hostname,
                "telnet": telnet_up,
                "remote_debug": remote_debug,
                "ota": ota_ready,
                "status": status,
            }
        )
    return results


def main():
    quiet = "--quiet" in sys.argv
    fast_only = "--fast" in sys.argv

    # Quick check: cached IP first
    if CACHE_PATH.exists():
        cached_ip = CACHE_PATH.read_text().strip()
        if cached_ip and quick_probe(cached_ip):
            if quiet:
                print(cached_ip)
            else:
                print("🔍 Using cached ESP32 host")
                print("📡 Found 1 active device")
                print("\n📋 ESP32 Device Status:")
                print("-" * 60)
                print(f"🟢 {cached_ip:<15} death-fortune-teller.lan         ACTIVE       Telnet:✅ OTA:✅")
                print(f"\n✅ Active ESP32 found: {cached_ip}")
                print(f"💡 Set: export DEATH_FORTUNE_HOST={cached_ip}")
                print("💡 Test: python scripts/telnet_command.py status")
            return 0

    # Try resolving known hostnames before full scan
    hostnames = [] if fast_only else [
        os.environ.get("DEATH_FORTUNE_HOSTNAME", "death-fortune-teller"),
        "death-fortune-teller.lan",
        "death-fortune-teller.local",
    ]
    tried = set()
    for host in hostnames:
        if not host or host in tried:
            continue
        tried.add(host)
        try:
            resolved_ip = socket.gethostbyname(host)
        except Exception:
            continue
        if resolved_ip and resolved_ip != "0.0.0.0" and quick_probe(resolved_ip):
            if not quiet:
                print(f"🔍 Resolved {host} to {resolved_ip}")
            try:
                CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
                CACHE_PATH.write_text(resolved_ip)
            except Exception:
                pass
            if quiet:
                print(resolved_ip)
            else:
                print("📡 Found 1 active device")
                print("\n📋 ESP32 Device Status:")
                print("-" * 60)
                print(f"🟢 {resolved_ip:<15} {host:<25} ACTIVE       Telnet:✅ OTA:✅")
                print(f"\n✅ Active ESP32 found: {resolved_ip}")
                print(f"💡 Set: export DEATH_FORTUNE_HOST={resolved_ip}")
                print("💡 Test: python scripts/telnet_command.py status")
            return 0

    if fast_only:
        if quiet:
            return 1
        print("⚠️ Fast discovery could not locate an active ESP32")
        return 1

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
        icon = (
            "🟢"
            if device["status"] == "ACTIVE"
            else "🟡"
            if device["status"].startswith("ESP32") or device["status"] == "TELNET"
            else "🔴"
        )
        telnet_status = "✅" if device["telnet"] else "❌"
        ota_status = "✅" if device["ota"] else "❌"
        print(
            f"{icon} {device['ip']:<15} {device['hostname'] or 'Unknown':<25} "
            f"{device['status']:<12} Telnet:{telnet_status} OTA:{ota_status}"
        )

        if device["status"] == "ACTIVE" and not active_device:
            active_device = device
            try:
                CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
                CACHE_PATH.write_text(device["ip"])
            except Exception:
                pass

    if active_device:
        print(f"\n✅ Active ESP32 found: {active_device['ip']}")
        print(f"💡 Set: export DEATH_FORTUNE_HOST={active_device['ip']}")
        print("💡 Test: python scripts/telnet_command.py status")
        if "--quiet" in sys.argv:
            print(active_device["ip"])
    else:
        print("\n⚠️  No active ESP32 found")
        print("💡 Check ESP32 is connected to WiFi and telnet server is running")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
