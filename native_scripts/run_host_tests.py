#!/usr/bin/env python3
"""Run portable production-source checks without hardware or ESP-IDF.

Requires Python 3 and a C++17 compiler with Address/UndefinedBehaviorSanitizer.
All generated fixtures and executables live in an isolated temporary directory.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    run([sys.executable, "-m", "unittest", "discover", "-s", "native_scripts/tests", "-v"])
    suites = [
        ("radio", [], "radio_stubs"),
        ("wave", ["skull_wave"], None),
        ("catalog", ["skull_catalog", "skull_wave"], None),
        ("config", ["skull_config"], None),
        ("secondary", [], None),
        ("actuators", ["skull_actuators"], "actuator_stubs"),
        ("pwm_probe", [], None),
        ("fault_log", ["death_fault_archive"], None),
        ("fault_runtime", [], "fault_log_stubs"),
        ("audio", ["skull_audio", "skull_wave"], "audio_stubs"),
        ("startup", ["skull_startup", "skull_wave"], "startup_stubs"),
        ("show", ["skull_show", "skull_catalog", "skull_wave"], "show_stubs"),
        ("storage", ["skull_storage", "skull_config"], "storage_stubs"),
        ("storage_spi", ["skull_storage", "skull_config"], "storage_spi_stubs"),
        ("printer", ["death_printer"], "printer_stubs"),
    ]
    with tempfile.TemporaryDirectory(prefix="death-host-") as directory:
        temporary = Path(directory)
        for name, sources, stubs in suites:
            print(f"\n=== {name} ===", flush=True)
            executable = temporary / name
            args = shlex.split(os.environ.get("CXX", "c++")) + [
                "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-Ifirmware/main",
            ]
            if stubs:
                args += [f"-Inative_scripts/tests/{stubs}"]
            if name == "fault_runtime":
                args += ["-pthread"]
            if name == "audio":
                args += ["-pthread", f'-DSKULL_AUDIO_SD_ROOT="{temporary / "audio-fixtures"}"']
            if name == "startup":
                args += [f'-DSKULL_STARTUP_PATH="{temporary / "initialized.wav"}"']
            if name == "storage":
                args += [f'-DSKULL_STORAGE_ROOT="{temporary / "storage-fixtures"}"']
            if name == "storage_spi":
                args += ["-Inative_scripts/tests/storage_stubs", "-DESP_PLATFORM", "-DDEATH_SD_SPI_TRANSPORT=1",
                         f'-DSKULL_STORAGE_ROOT="{temporary / "storage-spi-fixtures"}"']
            args += [f"native_scripts/tests/test_skull_{name}.cpp"]
            args += [f"firmware/main/{source}.cpp" for source in sources]
            run(args + ["-o", str(executable)])
            fixtures = []
            if name == "startup":
                import shutil
                fixture = temporary / "initialized.wav"
                shutil.copyfile(ROOT / "sd-card-files/audio/initialized.wav", fixture)
                for mode in ("success", "psram-fail", "staging-fail", "probe", "probe-alloc-fail", "cancel-ready", "cancel-preroll", "cancel-audio"):
                    print(f"--- startup {mode} ---", flush=True)
                    run([str(executable), mode, str(fixture)])
            if name == "wave":
                fixtures = [str(path) for path in sorted((ROOT / "sd-card-files/audio").glob("*.wav"))]

            if name != "startup":
                run([str(executable)] + fixtures)
    print("\nAll host suites passed.")


if __name__ == "__main__":
    main()
