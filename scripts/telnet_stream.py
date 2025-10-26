#!/usr/bin/env python3
"""Continuous Telnet stream helper that mirrors 'log' output."""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_HOST = os.environ.get("DEATH_FORTUNE_HOST", "192.168.86.29")
DEFAULT_PORT = 23
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
        result = subprocess.run(args, capture_output=True, text=True, timeout=60)
        if result.returncode == 0:
            output = (result.stdout or "").strip()
            if output:
                ip = output.splitlines()[-1].strip()
                if ip:
                    write_cached_host(ip)
                    return ip
    except Exception as exc:
        sys.stderr.write(f"⚠️ Discovery failed: {exc}\n")
    return None


def reconnecting_stream(host: str, port: int, retries: int, delay: float,
                        auto_discover: bool, allow_full: bool):
    attempt = 0
    attempted_fast = False
    attempted_full = False
    current_host = host

    while retries < 0 or attempt < retries:
        attempt += 1
        try:
            with socket.create_connection((current_host, port), timeout=5) as sock:
                sock.settimeout(1)
                _drain(sock)
                sock.sendall(b"log\n")
                write_cached_host(current_host)

                # Reset discovery flags once we have a clean stream
                attempted_fast = False
                attempted_full = False
                attempt = 0

                while True:
                    try:
                        data = sock.recv(4096)
                        if not data:
                            break
                        sys.stdout.buffer.write(data)
                        sys.stdout.flush()
                    except socket.timeout:
                        pass
        except Exception as exc:
            sys.stderr.write(f"[{current_host}] Stream attempt {attempt} failed: {exc}\n")

            if auto_discover:
                if not attempted_fast:
                    new_host = run_discovery(force_full=False)
                    attempted_fast = True
                    if new_host and new_host != current_host:
                        print(f"🔍 Auto-discovered ESP32 at {new_host}")
                        current_host = new_host
                        continue
                if allow_full and not attempted_full:
                    new_host = run_discovery(force_full=True)
                    attempted_full = True
                    if new_host and new_host != current_host:
                        print(f"🔍 Full-scan discovered ESP32 at {new_host}")
                        current_host = new_host
                        continue

        time.sleep(delay)


def _drain(sock):
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
    except socket.timeout:
        pass


def main():
    parser = argparse.ArgumentParser(description="Telnet log stream helper")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--retries", type=int, default=-1,
                        help="Number of reconnect attempts (-1 for infinite)")
    parser.add_argument("--delay", type=float, default=2.0,
                        help="Delay between reconnect attempts")
    parser.add_argument("--auto-discover", action="store_true",
                        help="Discover the skull when the cached host fails")
    parser.add_argument("--full-discovery", action="store_true",
                        help="Allow a full network scan fallback when needed")
    args = parser.parse_args()

    placeholder_host = DEFAULT_HOST
    explicit_host = args.host.strip() if args.host else ""

    if explicit_host and explicit_host != placeholder_host:
        target_host = explicit_host
    else:
        target_host = read_cached_host() or explicit_host or placeholder_host

    reconnecting_stream(target_host, args.port, args.retries, args.delay,
                        args.auto_discover, args.full_discovery)


if __name__ == "__main__":
    main()
