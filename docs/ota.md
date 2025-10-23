# OTA (Over-the-Air) Workflow

## Overview
- Wireless firmware updates use PlatformIO’s `esp32dev_ota` environment and ArduinoOTA.
- Log access is provided over Telnet (port 23) with the centralized `LoggingManager`.
- OTA automatically pauses audio playback and Bluetooth retries; both resume after the update finishes or aborts.

## Requirements
- SD card `/config/config.txt` must contain:
  ```ini
  wifi_ssid=YourNetwork
  wifi_password=YourPassword
  ota_hostname=death-fortune-teller
  ota_password=YourSecurePassword
  ```
- On your development machine:
  1. Create `platformio.local.ini` in the project root (git-ignored):
     ```ini
     [env:esp32dev_ota]
     upload_flags =
         --auth=${sysenv.ESP32_OTA_PASSWORD}
     ```
  2. Export `ESP32_OTA_PASSWORD` (or replace the placeholder with your password):
     ```bash
     export ESP32_OTA_PASSWORD=YourSecurePassword
     ```
- Optional: set `DEATH_FORTUNE_HOST` if the device IP changes (defaults to `192.168.86.29`).

## Building & Uploading OTA
- CLI (one-off):
  ```bash
  pio run -e esp32dev_ota -t upload
  ```
- Project Tasks (VS Code / PlatformIO panel):
  - `esp32dev_ota → OTA Upload` (custom target defined in `extra/death_tasks.py`).
- VS Code tasks:
  - `Terminal → Run Task… → Death OTA: Upload`.

## Telnet Logging & Commands
- Telnet address defaults to `DEATH_FORTUNE_HOST:23`.
- Manual session:
  ```bash
  telnet <device-ip> 23
  ```
  Available commands: `status`, `wifi`, `ota`, `log`, `startup`, `head N`, `tail N`, `stream on|off`, `help`.
- Helper scripts:
  - `python scripts/telnet_command.py status`
  - `python scripts/telnet_command.py log`
  - `python scripts/telnet_stream.py`
  - Use `--strict` to fail on connection issues; otherwise scripts retry three times and exit 0 so tasks stay green.
- PlatformIO custom targets (under `esp32dev_ota`):
  - `Telnet Status`, `Telnet Log`, `Telnet Startup`, `Telnet Head`, `Telnet Tail`, `Telnet Help`, `Telnet Stream`.
- VS Code tasks mirror the same commands (`Death OTA: Telnet Status`, etc.).

## Runtime Behavior
- OTA start: logs `⏸️ OTA start: pausing peripherals`, mutes audio, stops Bluetooth retries.
- OTA finish: logs `▶️ OTA completed: resuming peripherals`, restores audio/Bluetooth.
- OTA auth failure: logs `🔐 Authentication failed – ensure host upload password matches ota_password on SD`.
- OTA receive failure: logs `📶 Receive failure – check Wi-Fi signal quality and minimize peripheral activity during OTA`.

## Troubleshooting Tips
- If OTA reports `Host ... Not Found`, ensure the ESP32 is on Wi-Fi and reachable (ping or `python scripts/telnet_command.py status`).
- Telnet tasks exiting with retry warnings indicate the board is offline; reset or reconnect Wi-Fi, then rerun.
- Serial fallback: use `python - <<'PY' ...` snippets (`/ai-work/ota-logging.md`) to grab 115200 baud logs when OTA/Telnet is unavailable.

