#!/usr/bin/env python3
"""Capture a bounded serial run, optionally reset and send diagnostic commands.

Use the ESP-IDF Python environment (pyserial). Capture files can contain Matter
setup payloads; keep them in the ignored build directory or a private folder.
"""
import argparse
import os
import select
import sys
import time
from pathlib import Path
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--send", action="append", default=[])
    parser.add_argument("--send-after", type=float, default=8)
    parser.add_argument("--interactive", action="store_true", help="Forward stdin command lines while capturing")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(port=None, baudrate=115200, timeout=0.1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    try:
        if args.reset:
            port.rts = True
            time.sleep(0.1)
            port.reset_input_buffer()
            port.rts = False
        started = time.monotonic()
        sent = False
        received = 0
        stdin_active = args.interactive
        with args.output.open("xb") as output:
            while time.monotonic() - started < args.seconds:
                data = port.read(port.in_waiting or 1)
                if data:
                    output.write(data)
                    output.flush()
                    received += len(data)
                if not sent and time.monotonic() - started >= args.send_after:
                    for command in args.send:
                        port.write((command + "\n").encode())
                        port.flush()
                    sent = True
                if stdin_active and select.select([sys.stdin], [], [], 0)[0]:
                    # Read the fd directly: TextIOWrapper.readline() may buffer
                    # another complete command that select() can no longer see.
                    commands = os.read(sys.stdin.fileno(), 4096)
                    if not commands:
                        stdin_active = False
                    else:
                        port.write(commands)
                        port.flush()
        print(f"Captured {received} bytes in {args.output}")
    finally:
        port.close()


if __name__ == "__main__":
    main()
