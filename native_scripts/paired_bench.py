#!/usr/bin/env python3
"""Read-only card/audio qualification of two USB-connected skulls.

Owns both serial ports, resets once, exercises each named skit through the normal
BLE handshake, and checkpoints evidence after every file. Output may include
private Matter traffic: use a fresh ignored build/validation directory.
Requires pyserial (available in the ESP-IDF Python environment).
"""
import argparse
import json
import os
from pathlib import Path
import re
import select
import sys
import time
import wave

import serial


class Board:
    def __init__(self, name, device, directory):
        self.name = name
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0)
        self.port.dtr = False
        self.port.rts = False
        self.port.port = device
        self.port.open()
        self.log = (directory / f"{name}.log").open("xb")
        self.partial = b""
        self.role = None
        self.initialized = False
        self.status = {}
        self.producer = {}
        self.actuators = {}
        self.status_count = 0
        self.completions = []
        self.rejected = 0
        self.boots = 0
        self.panics = []
        self.watchdogs = 0
        self.reboot_refusals = 0

    def read(self):
        data = self.port.read(self.port.in_waiting or 1)
        if not data:
            return
        self.log.write(data)
        self.log.flush()
        self.partial += data
        while b"\n" in self.partial:
            line, self.partial = self.partial.split(b"\n", 1)
            text = line.decode(errors="replace")
            if "TwoSkulls shared WROVER firmware" in text:
                self.boots += 1
            if "Guru Meditation Error" in text or "Cache disabled but cached memory region accessed" in text:
                self.panics.append(text.strip())
            if "task_wdt: Task watchdog got triggered" in text:
                self.watchdogs += 1
            if "Reboot refused while" in text:
                self.reboot_refusals += 1
            role = re.search(r"Configured role=(primary|secondary)", text)
            if role:
                self.role = role[1]
            if "Startup announcement PCM delivered" in text:
                self.initialized = True
            fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", text)}
            if " STATUS " in text:
                self.status = fields
                self.status_count += 1
            elif " PRODUCER " in text:
                self.producer = fields
            elif " ACTUATORS " in text:
                self.actuators = fields
            complete = re.search(r"Playback complete: (.+?) frames=(\d+) underruns=(\d+)", text)
            if complete:
                self.completions.append({"path": complete[1], "frames": int(complete[2]),
                                         "underruns": int(complete[3]), "host_monotonic": time.monotonic()})
            if "Named paired diagnostic ignored" in text:
                self.rejected += 1

    def send(self, command):
        self.port.write((command + "\n").encode())
        self.port.flush()

    def close(self):
        self.log.close()
        self.port.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--primary-port", required=True)
    parser.add_argument("--secondary-port", required=True)
    parser.add_argument("--audio-directory", type=Path, default=Path("sd_card_files/audio"))
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--linger-seconds", type=float, default=0)
    parser.add_argument("--skit", action="append", help="Exact WAV basename; repeat option to choose files")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--check-reboot-guard", action="store_true", help="Verify secondary refuses a reboot during first skit")
    args = parser.parse_args()
    args.output_directory.mkdir(parents=True, exist_ok=False)
    boards = []
    report = {"results": [], "complete": False, "error": None}

    def save():
        report["diagnostics"] = {b.name: {"boots": b.boots, "panics": b.panics, "watchdogs": b.watchdogs,
                                                 "status": b.status.copy()} for b in boards}
        (args.output_directory / "summary.json").write_text(json.dumps(report, indent=2) + "\n")

    def pump():
        for board in boards:
            board.read()
            if not report["complete"] and (board.panics or board.boots > 1):
                raise RuntimeError(f"{board.name}: unexpected panic/reboot; qualification stopped")
        time.sleep(0.01)

    def wait_for(predicate, seconds, message):
        deadline = time.monotonic() + seconds
        while not predicate():
            if time.monotonic() >= deadline:
                raise TimeoutError(message)
            pump()

    def pause(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            pump()

    try:
        boards.append(Board("primary", args.primary_port, args.output_directory))
        boards.append(Board("secondary", args.secondary_port, args.output_directory))
        primary, secondary = boards
        for board in boards:
            board.port.rts = True
        time.sleep(0.1)
        for board in boards:
            board.port.reset_input_buffer()
            board.port.rts = False
        print("Waiting for both roles, speakers, startup audio and BLE link", flush=True)
        wait_for(lambda: primary.role == "primary" and secondary.role == "secondary" and
                 all(b.initialized and b.status.get("sd") == 1 and b.status.get("stream") == 1
                     for b in boards) and primary.status.get("peer") == 1,
                 180, "both-board startup/readiness timed out")
        wait_for(lambda: any(c["path"] == "/audio/Marco.wav" for c in primary.completions) and
                 any(c["path"] == "/audio/Polo.wav" for c in secondary.completions),
                 30, "startup Marco/Polo greeting timed out")
        pause(11)  # Cooldown starts after the greeting ACK, before Polo completes.
        report["baseline"] = {b.name: b.status.copy() for b in boards}
        for board in boards:
            board.send("heap-start")
        pause(0.5)
        files = sorted(args.audio_directory.glob("Skit*.wav"))
        if args.skit:
            selected = set(args.skit)
            files = [file for file in files if file.name in selected]
            if {file.name for file in files} != selected:
                raise RuntimeError("requested skit is not an available fixture")
        if not files or args.repeat < 1:
            raise RuntimeError("no skit fixtures or invalid repetition count")
        files *= args.repeat
        for index, file in enumerate(files):
            path = "/audio/" + file.name
            with wave.open(str(file)) as audio:
                expected = audio.getnframes()
                duration = expected / audio.getframerate()
            offsets = {b.name: len(b.completions) for b in boards}
            watchdogs = {b.name: b.watchdogs for b in boards}
            rejected = primary.rejected
            print(f"[{index + 1}/{len(files)}] Paired {file.name} ({duration:.1f}s)", flush=True)
            primary.send("pair " + path)
            if args.check_reboot_guard and index == 0:
                before_refusals = secondary.reboot_refusals
                pause(5)
                secondary.send("reboot")
                wait_for(lambda: secondary.reboot_refusals > before_refusals, 5,
                         "busy reboot was not explicitly refused")
                report["busy_reboot_refused"] = True

            def finished():
                if primary.rejected != rejected:
                    raise RuntimeError("named pair refused; no automatic retry")
                return all(any(c["path"] == path for c in b.completions[offsets[b.name]:]) for b in boards)

            wait_for(finished, duration + 25, f"paired completion timeout: {path}")
            counts = {b.name: b.status_count for b in boards}
            for board in boards:
                board.send("status")
            wait_for(lambda: all(b.status_count > counts[b.name] for b in boards), 5, "status sample timeout")
            row = {"path": path, "expected_frames": expected, "boards": {}}
            for board in boards:
                completion = next(c for c in board.completions[offsets[board.name]:] if c["path"] == path)
                row["boards"][board.name] = {**completion, "status": board.status.copy(),
                                              "producer": board.producer.copy(),
                                              "actuators": board.actuators.copy(),
                                              "watchdogs": board.watchdogs - watchdogs[board.name]}
            row["passed"] = all(b["frames"] == expected and b["underruns"] == 0 and b["watchdogs"] == 0
                                for b in row["boards"].values())
            report["results"].append(row)
            save()
            print("  " + ("PASS" if row["passed"] else "FAIL") + ": " + ", ".join(
                f"{name} frames={b['frames']} underruns={b['underruns']} watchdogs={b['watchdogs']}"
                for name, b in row["boards"].items()), flush=True)
            pause(11)
        for board in boards:
            board.send("heap-stop")
        pause(1)
        report["final"] = {b.name: b.status.copy() for b in boards}
        report["complete"] = True
        report["passed"] = all(r["passed"] for r in report["results"]) and all(
            b.watchdogs == 0 and not b.panics and b.boots == 1 for b in boards)
        save()
        print("CATALOG COMPLETE; all passed=" + str(report["passed"]), flush=True)
        print("Interactive commands while lingering: primary:<command> or secondary:<command>", flush=True)
        deadline = time.monotonic() + args.linger_seconds
        pending = b""
        while time.monotonic() < deadline:
            pump()
            if select.select([sys.stdin], [], [], 0)[0]:
                data = os.read(sys.stdin.fileno(), 4096)
                if not data:
                    break
                pending += data
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    target, separator, command = line.decode().strip().partition(":")
                    if separator and target in ("primary", "secondary"):
                        (primary if target == "primary" else secondary).send(command)
        return 0 if report["passed"] else 1
    except Exception as error:
        report["error"] = str(error)
        save()
        raise
    finally:
        for board in boards:
            board.close()


if __name__ == "__main__":
    sys.exit(main())
