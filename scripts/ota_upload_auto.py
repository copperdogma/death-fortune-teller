#!/usr/bin/env python3
"""OTA helper that discovers the skull, disables Bluetooth, uploads, and restores state."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_HOST = os.environ.get("DEATH_FORTUNE_HOST", "192.168.86.29")
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
        result = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=60,
        )
        output = (result.stdout or "").strip()
        if result.returncode == 0 and output:
            ip = output.splitlines()[-1].strip()
            if ip:
                write_cached_host(ip)
                return ip
    except Exception as exc:
        print(f"❌ Discovery failed: {exc}")
    return None


def test_connection(ip_address: str) -> bool:
    """Lightweight check that RemoteDebug responds."""
    try:
        print(f"🔍 Testing connection to {ip_address}…")
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
        if any(keyword in text for keyword in ("RemoteDebug", "WiFi:", "🛜")):
            print("✅ RemoteDebug responded")
        else:
            print("⚠️ Telnet responded but handshake was unexpected; continuing anyway")
        return True
    except Exception as exc:
        print(f"⚠️ Connection test failed: {exc}")
        return True  # still attempt OTA; the invite may succeed even if telnet doesn’t


def bluetooth_toggle(ip_address: str, state: str) -> bool:
    """Best-effort Bluetooth toggle via telnet."""
    state = state.lower()
    post_wait = "3" if state == "off" else "2"
    cmd = [
        sys.executable,
        "scripts/telnet_command.py",
        "bluetooth",
        state,
        "--host",
        ip_address,
        "--read-timeout",
        "2",
        "--post-send-wait",
        post_wait,
    ]
    try:
        subprocess.run(cmd, check=False, timeout=15)
        return True
    except Exception as exc:
        print(f"⚠️ Bluetooth {state} attempt failed: {exc}")
        return False


def perform_ota_upload(ip_address: str, disable_bluetooth: bool) -> bool:
    """Perform the OTA upload using PlatformIO, targeting the discovered IP."""
    toggled = False
    if disable_bluetooth:
        toggled = bluetooth_toggle(ip_address, "off")
        if not toggled:
            print("⚠️ Proceeding without disabling Bluetooth")

    try:
        print("🚀 Starting OTA upload…")
        cmd = [
            "pio",
            "run",
            "-e",
            "esp32dev_ota",
            "-t",
            "upload",
            "--upload-port",
            ip_address,
        ]
        env = os.environ.copy()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300, env=env)
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
    finally:
        if disable_bluetooth and toggled:
            bluetooth_toggle(ip_address, "on")


def main() -> int:
    parser = argparse.ArgumentParser(description="OTA upload helper")
    parser.add_argument("--host", help="IP/hostname of the skull (fallback to env/discovery)")
    parser.add_argument(
        "--skip-bt-toggle",
        action="store_true",
        help="Leave Bluetooth enabled during OTA",
    )
    parser.add_argument(
        "--full-discovery",
        action="store_true",
        help="Allow a full network scan fallback if fast discovery cannot locate the skull",
    )
    args = parser.parse_args()
    
    candidates: list[str] = []
    for candidate in [
        (args.host or "").strip(),
        os.environ.get("DEATH_FORTUNE_HOST", "").strip(),
        read_cached_host() or "",
        DEFAULT_HOST,
    ]:
        if candidate and candidate not in candidates:
            candidates.append(candidate)

    if not candidates:
        print("❌ Unable to determine a target host")
        return 1

    attempted_fast = False
    attempted_full = False

    while candidates:
        ip_address = candidates.pop(0)
        print(f"Using host: {ip_address}")

        print("\nStep 2: Testing connection…")
        if not test_connection(ip_address):
            print("⚠️ Telnet connection test failed; continuing regardless")

        print("\nStep 3: Performing OTA upload…")
        if perform_ota_upload(ip_address, disable_bluetooth=not args.skip_bt_toggle):
            write_cached_host(ip_address)
            print("\n🎉 OTA upload completed successfully!")
            return 0

        print("❌ OTA upload failed on host", ip_address)

        if not attempted_fast:
            print("🔁 Attempting fast rediscovery before retrying…")
            new_host = run_discovery(force_full=False)
            attempted_fast = True
            if new_host and new_host != ip_address and new_host not in candidates:
                print(f"🔍 Auto-discovered ESP32 at {new_host}")
                candidates.append(new_host)
                continue

        if args.full_discovery and not attempted_full:
            print("🔁 Attempting full network discovery…")
            new_host = run_discovery(force_full=True)
            attempted_full = True
            if new_host and new_host != ip_address and new_host not in candidates:
                print(f"🔍 Full-scan discovered ESP32 at {new_host}")
                candidates.append(new_host)
                continue

        print("❌ No additional hosts to try.")
        break

    print("\n💥 OTA upload did not complete. Ensure the skull is powered and reachable.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
