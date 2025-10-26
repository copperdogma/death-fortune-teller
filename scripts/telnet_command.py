#!/usr/bin/env python3
"""Simple Telnet helper for Death Fortune Teller.

Examples:
  python scripts/telnet_command.py status
  python scripts/telnet_command.py log
  python scripts/telnet_command.py send "ota\n"

By default connects to the OTA hostname and port.
"""
from __future__ import annotations
import argparse
import os
import socket
import sys
import time
import subprocess
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
        result = subprocess.run(args, capture_output=True, text=True, timeout=60)
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
DEFAULT_PORT = 23


def main():
    parser = argparse.ArgumentParser(description="Telnet command helper")
    parser.add_argument("command", nargs="?", default="status",
                        help="Command to send (status/log/startup/head/tail/help). 'send' allows raw input")
    parser.add_argument("extra", nargs="*", help="Additional data (for send)")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--retries", type=int, default=3,
                        help="Connection retries before failing (-1 for infinite)")
    parser.add_argument("--retry-delay", type=float, default=2.0,
                        help="Delay in seconds between retries")
    parser.add_argument("--strict", action="store_true",
                        help="Exit with non-zero status if connection fails after retries")
    parser.add_argument("--auto-discover", action="store_true",
                        help="Auto-discover ESP32 if not found")
    parser.add_argument("--password", default=os.environ.get("ESP32_OTA_PASSWORD", "***REDACTED***"),
                        help="Password for telnet authentication")
    parser.add_argument("--connect-timeout", type=float, default=5.0,
                        help="Socket connect timeout in seconds")
    parser.add_argument("--read-timeout", type=float, default=4.0,
                        help="Socket read timeout in seconds")
    parser.add_argument("--post-send-wait", type=float, default=1.5,
                        help="Wait time after sending a command before reading response")
    parser.add_argument("--full-discovery", action="store_true",
                        help="Allow a full network scan fallback if fast discovery fails")
    args = parser.parse_args()

    # Build host candidate list (explicit host first, then cached/default)
    host_queue: list[str] = []
    placeholder_host = DEFAULT_HOST

    explicit_host = args.host.strip() if args.host else ""
    if explicit_host and explicit_host != placeholder_host:
        host_queue.append(explicit_host)
    else:
        cached = read_cached_host()
        if cached:
            host_queue.append(cached)
        if explicit_host and explicit_host not in host_queue:
            host_queue.append(explicit_host)
    if not host_queue:
        host_queue.append(placeholder_host)

    cmd = args.command
    payload = None
    if cmd.lower() == "send":
        payload = " ".join(args.extra) if args.extra else ""
    else:
        payload = cmd
        if args.extra:
            payload = " ".join([cmd] + args.extra)

    if not payload.endswith("\n"):
        payload += "\n"

    attempted_fast_discovery = False
    attempted_full_discovery = False

    while host_queue:
        host = host_queue.pop(0)
        if not host:
            continue

        attempt = 0
        max_attempts = None if args.retries < 0 else max(1, args.retries)

        while max_attempts is None or attempt < max_attempts:
            attempt += 1
            try:
                with socket.create_connection((host, args.port), timeout=args.connect_timeout) as sock:
                    sock.settimeout(args.read_timeout)
                    _drain(sock)

                    initial_response = _read_all(sock)
                    if "password" in initial_response.lower() or "#" in initial_response:
                        sock.sendall(f"{args.password}\n".encode("utf-8"))
                        time.sleep(0.5)
                        _drain(sock)
                    else:
                        sock.sendall(b"\n")
                        time.sleep(0.5)
                        _drain(sock)

                    sock.sendall(payload.encode("utf-8"))
                    time.sleep(args.post_send_wait)
                    response = _read_all(sock)
                    sys.stdout.write(response)
                    sys.stdout.flush()

                    write_cached_host(host)
                    return
            except Exception as exc:
                sys.stderr.write(f"[{host}] Attempt {attempt}: {exc}\n")
                if max_attempts is not None and attempt >= max_attempts:
                    break
                time.sleep(args.retry_delay)

        # If we reach here, attempts for this host failed. Try discovery if allowed.
        if args.auto_discover:
            if not attempted_fast_discovery:
                new_host = run_discovery(force_full=False)
                attempted_fast_discovery = True
                if new_host and new_host not in host_queue and new_host != host:
                    print(f"🔍 Auto-discovered ESP32 at {new_host}")
                    host_queue.append(new_host)
                    continue
            if args.full_discovery and not attempted_full_discovery:
                new_host = run_discovery(force_full=True)
                attempted_full_discovery = True
                if new_host and new_host not in host_queue and new_host != host:
                    print(f"🔍 Full-scan discovered ESP32 at {new_host}")
                    host_queue.append(new_host)
                    continue

    sys.stderr.write("Giving up after retries. Ensure the device is online and reachable.\n")
    if args.strict:
        sys.exit(1)


def _drain(sock):
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
    except socket.timeout:
        pass


def _read_all(sock):
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode("utf-8", errors="replace")


def _quick_probe(ip: str, timeout: float = 0.6) -> bool:
    try:
        with socket.create_connection((ip, DEFAULT_PORT), timeout=timeout):
            return True
    except Exception:
        return False


if __name__ == "__main__":
    main()
