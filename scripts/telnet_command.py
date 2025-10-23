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

DEFAULT_HOST = os.environ.get("DEATH_FORTUNE_HOST", "192.168.86.29")
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
    args = parser.parse_args()

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
            with socket.create_connection((args.host, args.port), timeout=5) as sock:
                sock.settimeout(3)
                _drain(sock)
                sock.sendall(payload.encode("utf-8"))
                time.sleep(1.0)
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
