#!/usr/bin/env python3
"""Simple Telnet helper for Death Fortune Teller.

Examples:
  python scripts/telnet_command.py status
  python scripts/telnet_command.py log
  python scripts/telnet_command.py send "ota\n"

By default connects to the OTA hostname and port.
"""
import argparse
import os
import socket
import sys
import time
import subprocess

DEFAULT_HOST = os.environ.get("DEATH_FORTUNE_HOST", "192.168.86.29")

def discover_esp32():
    """Auto-discover ESP32 if not set"""
    try:
        result = subprocess.run([sys.executable, 'scripts/discover_esp32.py'], 
                              capture_output=True, text=True)
        if result.returncode == 0:
            for line in result.stdout.split('\n'):
                if 'Active ESP32 found:' in line:
                    return line.split(':')[1].strip()
    except:
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
    args = parser.parse_args()
    
    # Auto-discover if needed
    if args.auto_discover and args.host == DEFAULT_HOST:
        discovered = discover_esp32()
        if discovered:
            print(f"🔍 Auto-discovered ESP32 at {discovered}")
            args.host = discovered
        else:
            print("❌ Could not auto-discover ESP32")
            print("💡 Run: python scripts/discover_esp32.py")
            return 1

    cmd = args.command
    payload = None
    if cmd.lower() == "send":
        payload = " ".join(args.extra) if args.extra else ""
    else:
        payload = cmd

    if not payload.endswith("\n"):
        payload += "\n"

    attempts = 0
    while True:
        attempts += 1
        try:
            with socket.create_connection((args.host, args.port), timeout=args.connect_timeout) as sock:
                sock.settimeout(args.read_timeout)
                _drain(sock)
                
                # Handle password prompt if present
                initial_response = _read_all(sock)
                if "password" in initial_response.lower() or "#" in initial_response:
                    # Send password
                    sock.sendall(f"{args.password}\n".encode("utf-8"))
                    time.sleep(0.5)
                    _drain(sock)  # Drain password response
                else:
                    # If no password prompt, we might need to send a newline first
                    sock.sendall(b"\n")
                    time.sleep(0.5)
                    _drain(sock)
                
                # Send the actual command
                sock.sendall(payload.encode("utf-8"))
                time.sleep(args.post_send_wait)
                response = _read_all(sock)
                sys.stdout.write(response)
                sys.stdout.flush()
                return
        except Exception as exc:
            if args.retries == 0:
                sys.stderr.write(f"Telnet command failed: {exc}\n")
                if args.strict:
                    sys.exit(1)
                return
            sys.stderr.write(f"Attempt {attempts}: {exc}\n")
            if args.retries > 0 and attempts >= args.retries:
                sys.stderr.write("Giving up after retries. Ensure the device is online and reachable.\n")
                if args.strict:
                    sys.exit(1)
                return
            time.sleep(args.retry_delay)


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


if __name__ == "__main__":
    main()
