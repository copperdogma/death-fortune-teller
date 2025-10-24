# OTA (Over-the-Air) Workflow

## Overview
- Wireless firmware updates use PlatformIO’s `esp32dev_ota` environment and ArduinoOTA.
- Log access is provided over Telnet (port 23) with the centralized `LoggingManager`.
- OTA automatically pauses audio playback and Bluetooth retries; both resume after the update finishes or aborts.

## Requirements
- Device must be powered on and connected to your network. If you are unsure, verify via USB console first or ensure the ESP32 has booted into OTA mode before triggering uploads.
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

## Flash + Monitor Helpers
- USB (serial capture):
  ```bash
  python scripts/flash_and_monitor.py --mode usb --seconds 30
  ```
  - Available as `Flash + Monitor (USB)` under the `esp32dev` Project Tasks group or `Death USB: Flash + Monitor` from VS Code’s task menu.
- OTA (telnet capture):
  ```bash
  python scripts/flash_and_monitor.py --mode ota --capture telnet --seconds 30 --delay-after-flash 8
  ```
  - Surfaced via `Flash + Monitor (OTA)` in Project Tasks / `Death OTA: Flash + Monitor` in VS Code.
  - After the OTA upload completes the script waits a few seconds for the board to reboot, then collects telnet logs. If the board is offline, it exits successfully but prints the connection warning.

## Runtime Behavior
- OTA start: logs `⏸️ OTA start: pausing peripherals`, mutes audio, stops Bluetooth retries.
- OTA finish: logs `▶️ OTA completed: resuming peripherals`, restores audio/Bluetooth.
- OTA auth failure: logs `🔐 Authentication failed – ensure host upload password matches ota_password on SD`.
- OTA receive failure: logs `📶 Receive failure – check Wi-Fi signal quality and minimize peripheral activity during OTA`.

## Managing Multiple Skulls

When you have multiple Death Fortune Tellers powered up simultaneously, each needs to be uniquely identified to avoid conflicts during OTA updates.

### The Problem
- All skulls with identical `config.txt` settings will advertise the same OTA hostname
- Multiple devices responding to the same OTA name causes race conditions
- No way to distinguish which skull you're targeting for updates

### Solution: Unique Configuration Per Skull

**1. Give each skull its own identity:**
```ini
# Skull 1 config.txt
wifi_ssid=YourNetwork
wifi_password=YourPassword
ota_hostname=death-skull-1
ota_password=YourSecurePassword

# Skull 2 config.txt  
wifi_ssid=YourNetwork
wifi_password=YourPassword
ota_hostname=death-skull-2
ota_password=YourSecurePassword

# Skull 3 config.txt
wifi_ssid=YourNetwork
wifi_password=YourPassword
ota_hostname=death-skull-3
ota_password=YourSecurePassword
```

**2. Set target hostname before OTA operations:**
```bash
# Target specific skull
export DEATH_FORTUNE_HOST=192.168.86.30  # Skull 1 IP
pio run -e esp32dev_ota -t upload

# Or use script overrides
python scripts/telnet_command.py status --host 192.168.86.31  # Skull 2
```

**3. Optional: Create separate PlatformIO environments**
```ini
# platformio.local.ini
[env:esp32dev_ota_skull1]
upload_flags = --auth=${sysenv.ESP32_OTA_PASSWORD}
upload_port = 192.168.86.30

[env:esp32dev_ota_skull2]  
upload_flags = --auth=${sysenv.ESP32_OTA_PASSWORD}
upload_port = 192.168.86.31

[env:esp32dev_ota_skull3]
upload_flags = --auth=${sysenv.ESP32_OTA_PASSWORD}
upload_port = 192.168.86.32
```

**4. Discovery and verification:**
```bash
# Check which skulls are online
python scripts/telnet_command.py status --host 192.168.86.30
python scripts/telnet_command.py status --host 192.168.86.31  
python scripts/telnet_command.py status --host 192.168.86.32

# Verify you're targeting the right skull
telnet 192.168.86.30 23
# Then run: status
```

### Best Practices
- Use static IPs or DHCP reservations for consistent addressing
- Keep a reference list of skull hostnames and IPs
- Always verify target skull before OTA operations
- Consider using mDNS discovery for automatic skull detection

## Troubleshooting Tips
- If OTA reports `Host ... Not Found`, ensure the ESP32 is on Wi-Fi and reachable (ping or `python scripts/telnet_command.py status`).
- Telnet tasks exiting with retry warnings indicate the board is offline; reset or reconnect Wi-Fi, then rerun.
- Serial fallback: use `python - <<'PY' ...` snippets (`/ai-work/ota-logging.md`) to grab 115200 baud logs when OTA/Telnet is unavailable.
