#!/usr/bin/env python3
"""Continuous Telnet stream helper that mirrors 'log' output."""
import argparse
import os
import socket
import sys
import time

DEFAULT_HOST = os.environ.get("DEATH_FORTUNE_HOST", "192.168.86.29")
DEFAULT_PORT = 23


def reconnecting_stream(host: str, port: int, retries: int, delay: float):
    attempt = 0
    while retries < 0 or attempt < retries:
        attempt += 1
        try:
            with socket.create_connection((host, port), timeout=5) as sock:
                sock.settimeout(1)
                # Drain greeting
                _drain(sock)
                sock.sendall(b"log\n")
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
            sys.stderr.write(f"Stream attempt {attempt} failed: {exc}\n")
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
    args = parser.parse_args()

    reconnecting_stream(args.host, args.port, args.retries, args.delay)


if __name__ == "__main__":
    main()
