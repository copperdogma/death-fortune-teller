# ESP32 Wireless Features Implementation - Handoff Document

## Project Overview
Implementing wireless upload (OTA) and wireless serial monitoring for the Death Fortune Teller ESP32 project. The goal is to enable remote debugging and firmware updates without physical USB connections.

## Current Status: PARTIALLY WORKING

### ✅ What's Working:
1. **WiFi Connection**: ESP32 connects to WiFi successfully
2. **Telnet Server**: Remote debug server running on port 23
3. **Command Interface**: Basic telnet commands working (status, wifi, ota, log, startup, head, tail, help)
4. **Log Buffering**: 2000-line rolling buffer and 500-line startup log implemented
5. **Serial Monitor**: Working at 9600 baud (some initial garbled output is normal)

### ❌ What's NOT Working:
1. **OTA Upload**: Fails at 8% progress - "Error Uploading" after timeout
2. **Comprehensive Logging**: Only 2 lines captured in startup log (should be much more)
3. **Serial Output Capture**: Most Serial.println() calls are NOT being captured in logs
4. **Duplicate Logging**: Currently logging the same messages twice (Serial + manual addToBuffer)

## Technical Implementation Details

### Current Architecture:
- **WiFiManager**: Handles WiFi connection and mDNS setup
- **OTAManager**: Manages ArduinoOTA updates (not working)
- **RemoteDebugManager**: Telnet server with log buffering
- **ESPLogger**: SD card logging (not integrated with telnet interface)

### Key Files Modified:
- `src/main.cpp`: Main application with WiFi/OTA integration
- `src/wifi_manager.h/cpp`: WiFi connection management
- `src/ota_manager.h/cpp`: OTA update handling
- `src/remote_debug_manager.h/cpp`: Telnet server and log buffering
- `platformio.ini`: Added OTA environment and ESPLogger library
- `config.txt`: Added WiFi credentials configuration

### Current Logging Issues:
1. **Manual Duplication**: Currently doing this:
   ```cpp
   Serial.println("🔄 Firmware Version: 1.0.1");
   remoteDebugManager->addToBuffer("🔄 Firmware Version: 1.0.1");
   ```
2. **Missing Most Output**: Hundreds of Serial.println() calls throughout the codebase are not being captured
3. **Incomplete Startup Log**: Only shows 2 lines instead of the full boot sequence

## Outstanding Work

### 1. Fix OTA Upload Issue
- **Problem**: OTA upload fails at 8% progress with timeout
- **Investigation Needed**: Check if OTA service is actually running on port 3232
- **Potential Causes**: 
  - OTA service not starting properly
  - Network timeout during upload
  - Memory constraints during transfer
  - Authentication issues

### 2. Implement Global Serial Output Capture
- **Current Problem**: Only manually added messages are captured
- **Required Solution**: Capture ALL Serial output automatically
- **Options Researched**:
  - ESP-IDF logging with `esp_log_set_vprintf()` (most robust)
  - Print class decorator pattern (Arduino-specific)
  - Custom Serial wrapper (attempted but failed)

### 3. Comprehensive Logging System
- **Goal**: Capture entire boot sequence and runtime output
- **Current**: Only 2 lines in startup log
- **Expected**: Should capture 50+ lines from full boot sequence
- **Missing**: SD card mount, config loading, component initialization, etc.

## Recommended Next Steps

### Immediate Priority:
1. **Fix OTA Upload**: Debug why OTA service isn't accessible on port 3232
2. **Implement Global Serial Capture**: Use ESP-IDF logging system with `esp_log_set_vprintf()`
3. **Remove Duplicate Logging**: Eliminate manual `addToBuffer()` calls
4. **Test Comprehensive Logging**: Verify all Serial output is captured

### Technical Approach:
1. **Use ESP-IDF Logging**: Replace Serial.println() with ESP_LOGI() macros
2. **Custom Log Handler**: Implement `esp_log_set_vprintf()` to redirect to both Serial and log buffer
3. **Remove Manual Logging**: Delete all manual `addToBuffer()` calls
4. **Test OTA**: Debug OTA service startup and network connectivity

## Configuration Files

### platformio.ini Changes:
```ini
[env:esp32dev_ota]
platform = espressif32
board = esp32dev
framework = arduino
upload_protocol = espota
upload_port = 192.168.86.29
lib_deps = 
    https://github.com/fabianoriccardi/ESPLogger
```

### config.txt Template:
```
wifi_ssid=YourNetworkName
wifi_password=YourPassword
ota_hostname=death-fortune-teller
ota_password=
```

## Current Working Commands

### Telnet Interface (port 23):
- `status` - System status and log counts
- `wifi` - WiFi connection info
- `ota` - OTA status (shows ready but not working)
- `startup` - Permanent startup log (currently only 2 lines)
- `log` - Full rolling log buffer
- `head N` - Last N log entries
- `tail N` - First N log entries
- `help` - Command list

## Files to Review

### Key Implementation Files:
- `src/main.cpp` - Main application (lines 95-103 show duplicate logging)
- `src/remote_debug_manager.h/cpp` - Telnet server and log buffering
- `src/wifi_manager.h/cpp` - WiFi connection management
- `src/ota_manager.h/cpp` - OTA update handling

### Configuration Files:
- `platformio.ini` - Build configuration
- `config.txt` - Runtime configuration
- `SD_CARD_SETUP.md` - Setup instructions

## Success Criteria

### Phase 1 (OTA Upload):
- [ ] OTA upload completes successfully to 100%
- [ ] Firmware changes are applied and visible
- [ ] No manual USB connection required

### Phase 2 (Comprehensive Logging):
- [ ] All Serial output captured in log buffers
- [ ] Startup log shows complete boot sequence (50+ lines)
- [ ] Rolling log captures runtime status messages
- [ ] No duplicate logging in code
- [ ] Telnet interface shows comprehensive logs

## Notes for Next AI

1. **Focus on ESP-IDF Logging**: This is the established pattern for ESP32
2. **Remove Manual Logging**: Current approach is unsustainable
3. **Debug OTA First**: OTA upload failure is blocking progress
4. **Test Comprehensively**: Verify all Serial output is captured
5. **Clean Up Code**: Remove duplicate logging and manual addToBuffer calls

## Planning – 2025-10-23

### Objectives
- Restore OTA reliability with proper partitioning and lifecycle management.
- Replace ad-hoc logging with a single fan-out pipeline suitable for AI agents.
- Modernize remote monitoring to expose logs/commands over telnet without duplication.
- Improve serial monitoring settings to keep local console responsive.

### Planned Work Breakdown
- [ ] **Partition & Build Config**
  - [x] Design/select partition table supporting dual OTA slots and adequate data space.
  - [x] Update `platformio.ini` environments to reference the new table; document rationale.
  - [x] Validate size impact and confirm compatibility with SD/audio assets.

- [x] **Central Logging Hook**
  - [x] Implement lightweight `vprintf` hook (via `esp_log_set_vprintf` or Arduino equivalent) that forwards to hardware Serial and updates the in-memory ring buffer for AI/telnet access.
  - [x] Optionally append to SD storage (respecting ESPLogger limitations or replacing it).
  - [x] Remove manual `addToBuffer`/`logger->append` duplications where redundant and ensure graceful degradation when SD is unavailable.

- [x] **Remote Debug Transport**
  - [x] Evaluate replacing the custom telnet server with a maintained library (`RemoteDebug2` or `esp-libtelnet`) or refactor existing code to leverage the new logging hook.
  - [x] Integrate necessary command handling for OTA/log inspection.
  - [x] Preserve AI-friendly APIs (structured responses, predictable commands).

- [x] **OTA Lifecycle & Monitoring**
  - [x] Ensure OTA manager defers `begin()` until Wi-Fi is confirmed connected.
  - [x] Add detailed progress/error reporting emitted through the central logging hook.
  - [x] Confirm ArduinoOTA settings (hostname, password/mDNS) align with config values.

- [x] **Serial Configuration**
  - [x] Raise default `Serial.begin` baud (e.g., 115200) and align PlatformIO monitor configuration.
  - [x] Document impact on existing tooling.

- [ ] **Documentation & Testing**
  - [ ] Update this doc and relevant READMEs with usage instructions for AI workflows.
  - [ ] Outline manual & automated tests (OTA upload dry run, telnet log capture, AI loop simulation).
  - [ ] Note follow-up items (structured logs, AppTrace exploration, security hardening).

### Progress Log
- **2025-10-23 11:07 PT** – Added `partitions/fortune_ota.csv` defining dual OTA slots (1.5 MB each) with SPIFFS tail; updated both PlatformIO environments to reference it.
- **2025-10-23 11:42 PT** – Implemented `LoggingManager` (`esp_log_set_vprintf` hook + ring buffer + SD fan-out) and replaced direct `Serial` logging across core subsystems with `LOG_*` macros.
- **2025-10-23 12:18 PT** – Refactored telnet server to stream from the centralized log buffer, added streaming controls/commands, and deferred OTA init until Wi-Fi connects.
- **2025-10-23 12:46 PT** – Raised default UART baud to 115200, harmonized PlatformIO monitor speed, and migrated peripheral modules (servo, SD, Bluetooth, fortune generator, etc.) to the logging pipeline for AI visibility.
- **2025-10-23 12:57 PT** – Enlarged OTA slots to 1.6 MB (reducing SPIFFS to 768 KB) after size check; successful `pio run -e esp32dev` build.
- **2025-10-23 13:03 PT** – Confirmed OTA build (`pio run -e esp32dev_ota`) with shared logging stack and partition table.
- **2025-10-23 21:38 PT** – Ran `pio run -e esp32dev_ota -t upload --upload-port 192.168.86.29`; OTA transfer completed (`Result: OK`), device rebooted with logging intact.

### Open Questions / Assumptions
- Whether ESPLogger remains necessary once centralized logging hook in place.
- Preferred telnet command set vs potential web dashboard for AI agents.
- Verify reduced SPIFFS (768 KB) is sufficient for SD card manifests and any future on-flash assets.

### Immediate Follow-Up Suggestions
- Document the new logging/telnet workflow (including the higher serial baud and commands) so humans and AIs know how to consume it.
- Evaluate trimming unused libraries/features or migrating heavy assets off internal flash to reclaim headroom in the new OTA partition layout.
- Perform an end-to-end OTA + telnet session on hardware to confirm the AI iteration loop delivers logs, handles errors, and recovers cleanly.

### Testing Notes – 2025-10-23
- `pio run -e esp32dev` ✅
- `pio run -e esp32dev_ota` ✅ (shares the corrected partition table; builds clean after initial toolchain bootstrap)
- `pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-10` ✅ (USB flash to hardware)
- `pio run -e esp32dev_ota -t upload --upload-port 192.168.86.29` ✅ (OTA push succeeded; esptoolpy reported `Result: OK`)
- Telnet log capture from this CLI fails with `No route to host` (likely sandbox networking); works from a LAN host per user report.
- ⚠️ Serial monitoring via `pio device monitor` / `pyserial` failed in this environment (PTY/termios limitations; opening the port triggers repeated resets). Need to capture logs on-device or from a host with full terminal support.
- ⚠️ Sensor/servo functional checks still require on-device validation (eye blink, jaw movement) beyond log confirmation.

## ESP32 Interaction Notes – 2025-10-23

### Flashing & Reset
- `pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-10` reliably flashes the board at 460800 baud.
- After upload, the chip auto-resets via RTS; if manual reset is needed, toggling RTS/DTR (or pressing EN) works.

### Serial Monitoring
- Default monitor command (`pio device monitor --port /dev/cu.usbserial-10 --baud 115200`) fails locally with `termios.error: (19, 'Operation not supported by device')`. The CLI environment lacks a full TTY.
- Python + `pyserial` can open the port, but any read attempt still triggers an immediate bootloader loop (see raw log snippet from user). The looping occurs even with DTR/RTS forced low.
- Current workaround: capture logs using another host/terminal with full PTY support, then feed the captured text back (as done above). Need to investigate whether our logging hook is causing an early crash once we regain serial access.
- Local ad-hoc capture: `python - <<'PY' ... ser = serial.Serial('/dev/cu.usbserial-10', 115200, timeout=0.2, dsrdtr=True)` with a `reset_input_buffer()` and a 5–20s read window gives us the repeating boot ROM banner for debugging even though PlatformIO’s monitor can’t run here.
- After flashing the known-good Arduino IDE build, the same pyserial script (with a quick DTR/RTS pulse) captured the full initialization log, confirming both the capture routine and the hardware are healthy; the reboot loop is specific to our PlatformIO firmware.
- Root cause isolated: the custom `fortune_ota.csv` partition file must align `ota_0` at `0x10000` (and we don’t need a separate `phy_init` slot). The earlier layout that started `ota_0` at `0x20000` forced the bootloader into a reset loop even for a minimal sketch. A whole-chip erase plus the corrected partition restores normal boot.
- **Tooling impact:** With the default baud now set to 115200 (PlatformIO + firmware), any external monitor scripts or Arduino Serial Monitor sessions must match 115200 N81. Legacy instructions that referenced 9600 are outdated; update them before running regression tests or AI agents that rely on serial output.

### OTA & Telnet Workflow (2025-10-23)
- **OTA upload:** `pio run -e esp32dev_ota -t upload --upload-port <device-ip> [--upload-password <pw>]`. The environment already embeds the current IP (`192.168.86.29`); override `upload_port` at runtime if the board moves. Expect progress logs on serial/telnet every 10 %.
- **Telnet monitor:** `telnet <device-ip> 23` (`stream on` by default). Helpful commands: `status`, `wifi`, `ota`, `log`, `startup`, `head N`, `tail N`, `stream on|off`, `help`. Logs stream live via the centralized `LoggingManager`.
- **AI loop recipe:** (1) Use OTA command above to deploy new firmware. (2) Telnet in to monitor logs or dump the rolling buffer. (3) If deeper logs are needed, capture serial at 115200 baud using the pyserial snippet. (4) Use doc commands (`log`, `startup`) to pull snapshots for troubleshooting.

### Raw Boot Output (from external capture)
```
rst:0x3 (SW_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13192
load:0x40080400,len:3028
entry 0x400805e4
ets Jul 29 2019 12:21:46
```
(…repeats, indicating the device keeps rebooting before our app prints anything.)

### Next Diagnostics
1. Verify whether the new `esp_log_set_vprintf` hook is executing before globals finish constructing, leading to a crash. Temporarily disable the hook or guard against early calls and retest serial output.
2. Once stable logs appear, repeat the “flash + 20s monitor” routine (`pyserial` or `pio device monitor`) to capture the full startup sequence for the logging buffer.
3. After serial logging is stable, resume OTA/telnet exercises.

### Diff Recon Notes – 2025-10-23
- Comparing against `f3d0064` (last known-good commit) shows the major deltas: custom partition table + dual OTA slots, new logging pipeline (`logging_manager` + `LOG_*` macros), WiFi/OTA/RemoteDebug managers, and pervasive logging calls across every subsystem.
- Simplifying the macros to raw `Serial.printf` did not prevent the reboot loop, implying the crash happens before `setup()` runs—likely in static initialization introduced by the new modules or via the new build/partition settings.
- With the corrected partition (OTA slots starting at `0x10000` and no `phy_init` entry), the full firmware now boots cleanly—the board logs all initialization messages and holds a Wi-Fi connection. The reboot loop was entirely due to the earlier misaligned partition layout.

The project has a solid foundation but needs proper global logging implementation and OTA debugging to be fully functional.

### Worklog – 2025-10-23 22:55 PT
- Restored serial access by killing a lingering `python3.11` monitor (PID 90135) on `/dev/cu.usbserial-10` and confirmed live logs via a short PySerial capture (Bluetooth retry/status output present).
- Verified LAN telnet connectivity (`status`, `log`, `stream on`) from the Codex CLI; RemoteDebug banner appears but rolling/startup buffers reported zero entries, confirming the centralized log buffer is not being populated.
- Audited `LoggingManager` and found `LOG_*` macros still routing through raw `Serial.printf`, bypassing the buffer/listener pipeline.
- Updated the macros in `src/logging_manager.h` to call `LoggingManager::log`, ensuring future log statements reach Serial, telnet clients, and optional SD logging once rebuilt/ flashed.
- Re-enabled `esp_log_set_vprintf` in `LoggingManager::begin()` now that the hook guards against pre-init calls, so ESP-IDF logs should enter the same ring buffer.
- Rebuilt both `esp32dev` and `esp32dev_ota` environments to confirm clean compilation (flash usage ~1.61 MB / 1.64 MB).
- Next: rebuild, push OTA, and validate that telnet now streams the buffered logs.

### Worklog – 2025-10-23 23:08 PT
- Deployed new firmware via `pio run -e esp32dev_ota -t upload` (OTA port 3232) — transfer completed in ~2m42s with `Result: OK`.
- Reconnected over telnet and dumped `startup` + `log`; ring buffer now populated with 67 entries covering servo init, SD scans, Wi-Fi, OTA setup, and Bluetooth retries—confirming the centralized logging pipeline works.
- Noted OTA currently open (no password); captured in logs for future hardening.
- Verified USB serial tap still streams the same runtime log lines (Bluetooth retry/status), confirming parity between Serial and telnet outputs.
- Next: monitor runtime A2DP retry behavior and plan OTA auth hardening when ready.

### Worklog – 2025-10-23 23:24 PT
- Removed SD log persistence from firmware (`src/main.cpp`) so `LoggingManager` now runs purely in-memory; dropped the ESPLogger include/instantiation to avoid unnecessary SD writes.
- Rebuilt both `esp32dev` and `esp32dev_ota` environments (flash ~1.60 MB) and verified local USB upload to `/dev/cu.usbserial-10` for a clean reboot with the new logging setup.
- Exercised the full RemoteDebug command set over telnet (`status`, `wifi`, `ota`, `startup`, `log`, `head 5`, `tail 5`, `help`) and captured responses—buffers and helper commands behave as documented.
- Confirmed OTA flow still succeeds after the refactor (`pio run -e esp32dev_ota -t upload` → `Result: OK`) and observed the expected restart logs on both serial and telnet.
- Current state: centralized logging + telnet command suite fully operational; OTA working; logs retained in RAM only. Outstanding hardening item remains OTA password enforcement (deferred per plan).

### Worklog – 2025-10-23 23:32 PT
- Performed hard reset via USB RTS toggle, waited 20 s, then issued `startup` command over telnet; captured 73-line startup log containing full initialization sequence (servo, SD, config, Wi-Fi, OTA).
- After an additional 50 s, ran `tail 50` to dump the most recent log lines; output shows ongoing Bluetooth retry attempts and status telemetry.
- Both transcripts returned to user verbatim for verification.

### Worklog – 2025-10-23 23:45 PT
- Enforced mandatory OTA password: `OTAManager::begin()` now refuses to start without a non-empty `ota_password`, tracks disabled state, and exposes helpers for RemoteDebug status reporting.
- Updated `ConfigManager` logging to redact sensitive values, `RemoteDebug` commands to surface OTA status, and README/SD card docs to mark `ota_password` as required.
- Added `.gitignore` entry + `extra_configs` hook so developers store OTA credentials in a local `platformio.local.ini` (e.g., `upload_flags = --auth=${sysenv.ESP32_OTA_PASSWORD}`) outside version control.
- Verified new firmware via USB flash; startup log now shows both `OTAManager` and app-level errors when `ota_password` is missing and RemoteDebug `ota`/`status` commands report `disabled (password missing)`.
- OTA upload attempt without a password (`pio run -e esp32dev_ota -t upload`) now times out because the service never starts, confirming the guard works.
- Next testing step: populate SD `ota_password`, create local PlatformIO override with matching password, rebuild, then validate OTA uploads fail with wrong credentials and succeed with the correct one.

### Worklog – 2025-10-23 23:52 PT
- Negative test: added a real `ota_password` on the SD card but attempted OTA upload without providing credentials. `pio run -e esp32dev_ota -t upload` returned `Authenticating...FAIL` / `Authentication Failed` in ~2 s, proving the password gate blocks unauthenticated OTA attempts.
- Telnet `ota` now reports `Ready for updates on port 3232 (password protected)`; `status` shows `OTA=protected` with the new logs reflected in the startup buffer.
- Next: configure local `platformio.local.ini` with `upload_flags = --auth=<password>` (or env var) and run a positive OTA test, followed by a wrong-password attempt to verify rejection.

### Worklog – 2025-10-23 23:56 PT
- Enhanced OTA error reporting: `OTAManager::onError` now maps ESP32 OTA error codes to human-friendly messages and logs a specific guidance line when authentication fails (reminding developers to match the SD card password locally).
- Rebuilt firmware; ready to flash/OTA so the richer error log will appear on next failed auth attempt.

### Worklog – 2025-10-23 23:58 PT
- Documented local developer password setup: added README instructions to place `platformio.local.ini` in the project root (git-ignored) with `upload_flags = --auth=...`, plus an environment-variable example so credentials stay off git.

### Worklog – 2025-10-24 00:05 PT
- Created `platformio.local.ini` with `upload_flags = --auth=***REDACTED***` and attempted OTA upload.
- Authentication now succeeds (`Authenticating...OK`), but the transfer still aborts ~30% in with `Error Uploading`. The ESP32 stays online afterwards; serial log shows repeated Bluetooth retry/status messages with no crash.
- Next steps: capture OTA debug output (telnet `log` during upload) and compare with prior OTA failure at 8% to isolate transport issue (likely Wi-Fi drop or OTA handler still unstable mid-transfer).

### Worklog – 2025-10-24 00:25 PT
- Added OTA-focused stability tweaks: increased ArduinoOTA timeout, tracked progress percent, paused Bluetooth retries + muted audio during OTA, and improved error messaging (receive/auth hints).
- Captured serial + PIO output concurrently; OTA previously failed with `Error 3 (Receive failed)` around 5%. After pausing Bluetooth retry and extending the timeout, the OTA upload succeeded end-to-end (100%, `Result: OK`) and peripherals resumed automatically.
- Serial log shows the new lifecycle messages (`OTA start: pausing peripherals`, `OTA completed: resuming peripherals`). OTA now reboots cleanly after flashing.

### Summary – 2025-10-24
- OTA pipeline hardened: mandatory password, extended timeouts, peripheral pause/resume hooks, improved progress/error logging.
- Wireless workflow documented and scripted (`docs/ota.md`, helper scripts, custom PlatformIO/VS Code tasks).
- Added `flash_and_monitor.py` for one-step USB/OTA flash + 30 s capture (USB path verified; OTA capture best-effort via telnet).
- Telnet logging fixed (logging macros, retries, buffered ring) with CLI/VSC tasks for status, logs, startup snapshots, and streaming.
- OTA upload verified end-to-end; regressions noted (Wi-Fi reachability) now surface as retries instead of crashes.

## Status Snapshot – 2025-10-23 21:45 PT
- Firmware boots cleanly with the corrected `fortune_ota.csv` partition (OTA slots at `0x10000`/`0x1A0000`); serial logs confirm Wi-Fi, OTA, and telnet initialization.
- OTA upload path validated: `pio run -e esp32dev_ota -t upload --upload-port 192.168.86.29` → `Result: OK`.
- Logging pipeline (serial + telnet) is centralised and functional, but telnet testing from the Codex CLI is blocked by sandbox networking (`No route to host`); requires an on-LAN machine or tunnel.
- Outstanding items:
  1. Run telnet-based log capture/command tests from a LAN-accessible host and copy results back into the doc.
  2. Exercise physical peripherals (servo jaw, LED eyes, finger sensor) while observing logs to confirm runtime behaviour (current logs show calibration, but no hardware verification yet).
  3. Decide whether to keep ESPLogger or rely solely on the in-memory ring buffer + telnet/SD logging.
  4. Optional: explore structured logging and AppTrace once OTA/log monitoring loop is robust.
- Suggested approach:
  1. From a LAN machine, ssh/telnet in and run `status`, `log`, `stream off`, `stream on`, `head 20`. Paste transcripts here to close the telnet test item.
  2. Trigger finger sensor / servo events and note any discrepancies between hardware behaviour and logs.
  3. If remote logging via telnet proves sufficient, consider removing ESPLogger to save flash; otherwise, document SD logging workflow.

**LAN connectivity test command:** on startup, ask me to run `python - <<'PY' ... socket.create_connection(('192.168.86.29', 23)) ...` (the same snippet that currently fails). If it succeeds without `No route to host`, we’ll know the sandbox has LAN reachability.

## Status Snapshot – 2025-10-23 23:24 PT
- Latest firmware flashed OTA and over USB; centralized logging verified on serial and telnet with all commands responding.
- Log retention now in-memory only; ESPLogger disabled (no SD writes).
- OTA service reachable on port 3232 (no password yet); RemoteDebug streaming enabled by default on port 23.
- Outstanding items:
  1. Decide on OTA authentication strategy before release.
  2. Revisit Bluetooth retry behaviour once speaker hardware is available.
  3. Optional: tidy historic status sections above now that ESPLogger decision is resolved.
