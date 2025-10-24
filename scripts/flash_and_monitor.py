
#!/usr/bin/env python3
"""
flash_and_monitor.py

Upload firmware (USB or OTA) and capture logs for a short window.

Usage examples:
  python scripts/flash_and_monitor.py --mode usb --seconds 30
  python scripts/flash_and_monitor.py --mode ota --capture telnet --seconds 30 --delay-after-flash 8
"""

import argparse
import os
import subprocess
import sys
import time

try:
    import serial
except ImportError:
    serial = None

DEFAULT_HOST = os.environ.get("DEATH_FORTUNE_HOST", "192.168.86.29")
DEFAULT_SECONDS = 30


def run_command(cmd, **kwargs):
    env = os.environ.copy()
    env.setdefault("PLATFORMIO_HOME_DIR", os.path.join(os.getcwd(), ".piohome"))
    kwargs.setdefault("env", env)

    if isinstance(cmd, list):
        import shlex
        cmd = " ".join(shlex.quote(str(arg)) for arg in cmd)

    process = subprocess.Popen(
        ["bash", "-lc", cmd],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        **kwargs,
    )

    output = []
    for line in process.stdout:
        sys.stdout.write(line)
        output.append(line)
    process.wait()
    return process.returncode, "".join(output)


def flash_usb(env):
    return run_command(["pio", "run", "-e", env, "-t", "upload"])


def flash_ota(env):
    return run_command(["pio", "run", "-e", env, "-t", "upload"])


def capture_serial(port, baud, seconds):
    if serial is None:
        return 1, "pyserial not available"

    end_time = time.time() + seconds
    output = []
    print(f"
[flash_and_monitor] Starting serial capture for {seconds}s...
")

    try:
        with serial.Serial(port, baudrate=int(baud), timeout=0.2) as ser:
            while time.time() < end_time:
                data = ser.read(1024)
                if data:
                    text = data.decode("utf-8", errors="replace")
                    sys.stdout.write(text)
                    sys.stdout.flush()
                    output.append(text)
    except serial.SerialException as exc:
        return 1, str(exc)

    return 0, "".join(output)


def capture_telnet(host, seconds):
    deadline = time.time() + seconds
    output = []

    try:
        import telnetlib
    except ImportError:
        return run_command([
            "python",
            "scripts/telnet_command.py",
            "log",
            "--retries",
            "1",
            "--host",
            host,
        ])

    try:
        tn = telnetlib.Telnet(host, 23, timeout=5)
        tn.read_until(b"help", timeout=2)
        tn.write(b"log
")
        while time.time() < deadline:
            chunk = tn.read_very_eager()
            if chunk:
                text = chunk.decode("utf-8", errors="replace")
                sys.stdout.write(text)
                sys.stdout.flush()
                output.append(text)
            time.sleep(0.1)
        tn.close()
        return 0, "".join(output)
    except Exception as exc:
        return 0, f"Telnet capture failed: {exc}
"


def main():
    parser = argparse.ArgumentParser(description="Flash firmware and capture logs")
    parser.add_argument("--mode", choices=["usb", "ota"], default="usb")
    parser.add_argument("--usb-env", default="esp32dev")
    parser.add_argument("--ota-env", default="esp32dev_ota")
    parser.add_argument("--serial-port", default="/dev/cu.usbserial-10")
    parser.add_argument("--serial-baud", default="115200")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--capture", choices=["serial", "telnet"], default="serial")
    parser.add_argument("--seconds", type=int, default=DEFAULT_SECONDS)
    parser.add_argument("--delay-after-flash", type=float, default=3.0)
    parser.add_argument("--strict", action="store_true",
                        help="Exit non-zero if capture fails (default: warn only)")
    args = parser.parse_args()

    print(f"[flash_and_monitor] Mode: {args.mode}")
    if args.mode == "usb":
        exit_code, _ = flash_usb(args.usb_env)
    else:
        exit_code, _ = flash_ota(args.ota_env)

    if exit_code != 0:
        sys.exit(exit_code)

    if args.delay_after_flash > 0:
        print(f"[flash_and_monitor] Waiting {args.delay_after_flash}s before capture...")
        time.sleep(args.delay_after_flash)

    if args.capture == "serial":
        cap_code, info = capture_serial(args.serial_port, args.serial_baud, args.seconds)
    else:
        cap_code, info = capture_telnet(args.host, args.seconds)

    if info:
        sys.stdout.write(info)
        sys.stdout.flush()

    if args.strict:
        sys.exit(cap_code)
    sys.exit(0)


if __name__ == "__main__":
    main()
