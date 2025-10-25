# OTA Issues Debugging Log

> ✅ **Operational reminder:** Before every OTA attempt, run `python scripts/discover_esp32.py` (or `pio run -e esp32dev_ota -t ota_upload_auto`) to capture the skull’s current IP. Set `DEATH_FORTUNE_HOST` or pass `--upload-port <ip>` to PlatformIO using that value—DHCP routinely flips between 192.168.86.46 and 192.168.86.49. Double-check the latest boot log (look for `WiFiManager: IP address` and `RSSI`) if in doubt.

### Step 9: 2025-10-24 21:53 MDT – Current OTA Investigation
**Action**: Took over debugging with hardware on USB, verified SD `config.txt` (role=primary, ota_password=***REDACTED***) and established plan.
**Result**: **IN PROGRESS** – Preparing to disable telnet streaming during OTA and capture logs post-failure.
**Notes**:
- OTA partition slots remain 0x1E0000 bytes; current firmware size is ~1.66 MB (build succeeds).
- Failures observed at ~64 % likely `OTA_RECEIVE_ERROR` (transport drop), not partition overflow.
- Implemented firmware change to auto-pause RemoteDebug streaming during OTA (reduces competing TCP traffic); flashed via USB (`pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-10` on 2025-10-24 22:20 MDT).
- Serial capture post-flash confirms Wi-Fi connection and new IP 192.168.86.49 (updates needed for OTA host config).
- Added Wi-Fi stability tweaks (`WiFi.setSleep(false)`, `WiFi.setTxPower(WIFI_POWER_19_5dBm)`) and increased ArduinoOTA timeout to 45 s; reflashed via USB (2025-10-24 22:45 MDT).
- Next actions:
  1. Run OTA with telnet `stream off`, immediately capture telnet log to confirm error code.
  2. Retry with `remoteDebugManager->update()` temporarily disabled if failure persists.
  3. Adjust OTA chunk rate/conditions if still failing.

**Latest test (2025-10-24 22:30 MDT)**: Invoked `pio run -e esp32dev_ota -t upload` while capturing USB serial. OTA session accepted auth but aborted almost immediately with `OTAManager: ❌ Error 3 (Receive failed)` and the guidance message (“📶 Receive failure…”). Board stayed on Wi-Fi (IP 192.168.86.49) and AutoStreaming resumed after abort; Bluetooth retries restarted automatically. Telnet connections from this machine still time out (likely network reachability issue), so log capture relied on direct serial.

### Step 10: 2025-10-24 22:45 MDT – Reboot Loop Regression
**Action**: Observed repeated panics (`Error! Should enable WiFi modem sleep when both WiFi and Bluetooth are enabled!!!!!!`) after introducing Wi-Fi power tweaks.
**Result**: **FIXED** – Moved `WiFi.setTxPower()` to post-connect handler and removed the `setSleep(false)` call; reflashed via USB (`pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-10`). Device now boots cleanly and reaches OTA-ready state again (IP 192.168.86.49).
**Notes**:
- Root cause: disabling Wi-Fi modem sleep conflicts with Bluetooth coexistence stack, triggering an ESP-IDF assert.
- Wi-Fi TX power still boosted after connection; crash no longer occurs.

### Step 11: 2025-10-24 22:55 MDT – Resume OTA Failure Diagnosis
**Action**: With the board stable again, ran `pio run -e esp32dev_ota -t upload` (target IP 192.168.86.49) while capturing USB serial.
**Result**: **FAILED** – Host authenticated but then stalled at `Waiting for device…` before timing out. Device logged `Error 2 (Connection failed)` followed by `Error 4 (End failed)` at ~7 s, then resumed normal operation (Wi-Fi stayed connected, Bluetooth retries restarted).
**Notes**:
- OTA progress never exceeded 0%; failure occurs immediately after the board pauses peripherals.
- Suggests the ESP32 tried to open the OTA data socket but either the connection dropped or was rejected by the host (likely network path issue from this machine to 192.168.86.49).
- Serial remains the reliable capture channel; telnet from this host still times out.

### Step 12: 2025-10-24 23:05 MDT – IP Drift Detected
**Action**: Ran `python scripts/discover_esp32.py` to locate the active OTA target before the next upload attempt.
**Result**: **SUCCESS / IN FLUX** – Discovery reported the skull at `192.168.86.46`, but the subsequent OTA attempt (with serial capture) showed the device reconnecting at `192.168.86.49`. Updated `platformio.ini` `upload_port` to match the latest serial log IP; concluded the ESP32’s DHCP lease is bouncing between addresses.
**Next Steps**:
- Use the auto-discovery workflow (`python scripts/ota_upload_auto.py`) immediately before each OTA, or export `DEATH_FORTUNE_HOST` based on the IP announced in the serial logs (`WiFiManager: IP address`).
- Retry OTA now that `upload_port` is aligned with the most recent IP; capture serial output to confirm whether `Error 2/4` clears.

### Step 13: 2025-10-24 23:12 MDT – OTA Attempt on Fresh IP
**Action**: Re-ran `pio run -e esp32dev_ota -t upload --upload-port 192.168.86.49` while logging USB serial to capture progress and errors.
**Result**: **FAILED at 39%** – Host reported `Error Uploading` after ~33 s. Serial trace shows OTA progress reaching 35–38 %, then no further messages (no explicit `Error` callback triggered, peripherals eventually resume). Indicates the TCP transfer still drops mid-stream even with the correct IP.
**Notes**:
- Failure occurs later (≈39 %) compared to earlier ~6 %, suggesting unstable Wi-Fi throughput rather than immediate connectivity rejection.
- Bluetooth connection attempts during OTA generate extra callbacks even though retries are paused; consider suppressing the logging chatter to reduce Wi-Fi contention.
- Next diagnostic: inspect Wi-Fi signal quality, ensure the skull is close to the AP, and possibly throttle the OTA chunk size or enforce modem sleep defaults to improve coexistence.

### Step 14: 2025-10-24 23:20 MDT – Instrument Wi-Fi Metrics
**Action**: Added RSSI logging in `WiFiManager::handleConnection()` so the boot log records signal strength once the station connects; reflashed via USB to deploy.
**Status**: **COMPLETED** – On next boot the serial log will report `WiFiManager: RSSI: <value>dBm`, making it easier to correlate OTA failures with poor signal. Also added `--host_ip=192.168.86.28` and `--timeout=45` to `platformio.local.ini` so espota uses this Mac’s LAN IP and a longer invitation window.

### Step 15: 2025-10-24 23:28 MDT – Post-Flash Regression
**Action**: Captured 20 s of serial output after the RSSI instrumentation flash to verify the new log line and resume OTA testing.
**Result**: **FAILED** – Board hit an ESP-IDF assertion `hash_map_set` shortly after Wi-Fi connected (while Bluetooth retries were active) and rebooted. The crash occurs before the `WiFiManager: RSSI` line appears, so instrumentation not yet confirmed.
**Notes**:
- Backtrace starts in `esp32-hal-bt`/Bluetooth stack; may be triggered by repeated A2DP retry logs or BT/Wi-Fi coexistence toggling.
- After reboot the device reconnects (IP again 192.168.86.49). Need to stabilize runtime before resuming OTA attempts.

### Step 16: 2025-10-24 23:35 MDT – Bluetooth Disabled, OTA Success
**Action**: Introduced `DISABLE_BLUETOOTH` build flag (now enabled for both `esp32dev` and `esp32dev_ota`) so the firmware skips A2DP initialization during OTA work. Reflashed via USB, confirmed the skull stays up (no Bluetooth logs, free heap ~115 KB), then re-ran `pio run -e esp32dev_ota -t upload --upload-port 192.168.86.49` with serial capture.
**Result**: **SUCCESS** – OTA upload completed in ~60 s (`Result: OK`), with serial progress updates every 5 %. Device rebooted into the new image, reporting the new RSSI once connected (`WiFiManager: RSSI: -77dBm`).
**Notes**:
- Temporary trade-off: Bluetooth audio is disabled while we focus on OTA stability; remove `DISABLE_BLUETOOTH` once uploads are reliable again.
- With OTA flowing, next steps are to re-enable Bluetooth incrementally (or gate it behind a config flag) and confirm uploads remain stable.

### Step 17: 2025-10-24 23:42 MDT – Runtime Bluetooth Toggle
**Action**: Added `bluetooth_enabled` support in config plus pause/resume hooks so Bluetooth can auto-pause during OTA. Rebuilt without the compile flag, leaving `bluetooth_enabled=true`, and attempted OTA.
**Result**: **FAILED** – OTA still crashes the Bluetooth stack almost immediately (`hash_map_set` assert, later Guru Meditation in `btc_a2dp_cb_handler`) even though retries are paused via the new hooks. Disabling Bluetooth in config before OTA remains the stable path.
**Notes**:
- Recommendation: set `bluetooth_enabled=false` (or define `DISABLE_BLUETOOTH`) before OTA uploads, then re-enable afterwards for audio playback.
- The new runtime hook stays in place so we can revisit coexistence fixes later without reverting these code changes.

### Step 18: 2025-10-24 23:50 MDT – Loop Guard During OTA
**Action**: Added `OTAManager::isUpdating()` and an early guard in `loop()` that, while OTA is in progress, runs only `WiFiManager::update()` and `OTAManager::update()` (ArduinoOTA.handle), sleeping the rest of the system. OTA start/stop callbacks now call `BluetoothController::pauseForOta()` / `resumeAfterOta()` so Bluetooth fully tears down during the transfer.
**Result**: **READY FOR TESTING** – Builds succeed; with Bluetooth disabled (`bluetooth_enabled=false`) OTA uploads remain stable. Re-enabling Bluetooth still triggers the stack assert, but the new guard makes it easier to experiment with coexistence fixes later.

### Step 19: 2025-10-25 00:42 MDT – Bluetooth stack teardown refined
**Action**: Dug into the ESP32-A2DP library and identified that our OTA hook called `BluetoothA2DPSource::end(true)`, which frees controller memory (`esp_bt_controller_mem_release`) and makes subsequent restarts unstable. Reworked `BluetoothController::pauseForOta()/resumeAfterOta()` to keep memory allocated (`end(false)`), explicitly disable `esp_bluedroid` and the BT controller before OTA, and re-enable them afterward with state checks/timeouts. Verified the firmware compiles by running `pio run -e esp32dev`.
**Result**: **READY TO VERIFY ON HARDWARE** – Build succeeds; OTA should now pause Bluetooth without crashing the stack, even when `bluetooth_enabled=true`.
**Notes**:
- Expect up to ~0.5 s pause while the controller powers down; OTA progress bars may stall briefly at 0 % before bytes flow.
- Serial log should show the disable/enable path; capture it during the next OTA attempt to confirm both messages fire.
- `bluetooth_enabled` remains available as a fallback, but the runtime guard now handles full suspension so we can keep audio enabled by default.
- Next up: Re-run OTA with Bluetooth enabled, watch for `Guru Meditation` regressions, and stress-test multiple consecutive uploads.

### Step 20: 2025-10-25 01:08 MDT – OTA smoke test (with Bluetooth on) still crashing
**Action**: Flashed latest firmware over USB, kicked off OTA while capturing USB serial. Observed the new guard fire (`⏸️ Pausing Bluetooth for OTA`) but the ESP32 still panicked at ~5 % (`InstrFetchProhibited`, PC=0x0) and rebooted. Backtrace pointed to callbacks firing after the A2DP instance was deleted.
**Result**: **FAILED** – OTA aborts almost immediately; Bluetooth teardown still unsafe.
**Notes**:
- Library keeps global pointers (`actual_bluetooth_a2dp_common/source`) to the active object; deleting ours leaves stale callbacks that execute on freed memory.
- Crash timestamp logged in `/tmp/ota_serial_capture.log` for reference; OTA progress never exceeded 5 %.
- Needed to null those globals before releasing the instance, otherwise the ESP-IDF dispatchers jump to address 0x0.

### Step 21: 2025-10-25 01:24 MDT – Nulling ESP32-A2DP globals restores OTA stability
**Action**: Declared the library globals in `bluetooth_controller.cpp` and set them to `nullptr` just after `a2dp_source->end(false)`; rebuilt both USB/OTA targets, reflashed via USB, then reran OTA with Bluetooth enabled while capturing serial. OTA progressed smoothly to 100 % and resumed Bluetooth without a crash.
**Result**: **SUCCESS** – OTA upload (Bluetooth active) completed in ~54 s with logs: `⏸️ Pausing Bluetooth for OTA`, steady progress from 0→100 %, then `▶️ Resuming Bluetooth after OTA` plus a clean A2DP restart.
**Notes**:
- Removed the compile-time `-DDISABLE_BLUETOOTH` flag from `platformio.ini` so OTA builds exercise the full stack.
- Serial capture (`/tmp/ota_serial_capture.log`) contains the full success trace including bonded-device enumeration and connection retry restart.
- Next validation: run back-to-back OTA uploads to ensure repeated suspend/resume cycles stay stable and confirm the speaker re-pairs automatically after each update.

### Step 22: 2025-10-25 01:33 MDT – Back-to-back OTA uncovers residual BTC_A2DP crash
**Action**: Immediately triggered a second OTA (Bluetooth left enabled) to stress-test consecutive updates. Capture shows the guard firing, but before OTA made progress the ESP32 hit `LoadProhibited` inside `btc_a2dp_cb_handler` (IDF `btc_av.c:1525`) and rebooted.
**Result**: **FAILED** – Second OTA attempt aborts with a Bluedroid callback dereferencing a null pointer.
**Notes**:
- Likely race between the library’s BTC task and our teardown while reconnection retries are still winding down. Need to wait for the BTC task queue to drain (or fully stop Bluetooth after the first OTA) before re-entering OTA.
- Next experiment: add an explicit wait for `BluetoothController::isRetryingConnection()` to clear (or skip auto-resume entirely until a manual trigger) before we allow another OTA session.

### Step 23: 2025-10-25 02:05 MDT – Deferred Bluetooth resume guard
**Action**: Added a deferred resume mechanism that queues Bluetooth bring-up ~8 s after OTA completion (or abort). `resumeAfterOta()` now schedules an exact resume time, `pauseForOta()` clears pending work, and `update()` checks `OTAManager::isUpdating()` so repeated uploads keep Bluetooth paused. Builds: `pio run -e esp32dev`, `pio run -e esp32dev_ota`.
**Result**: **READY FOR RETEST** – Firmware compiles, new logs (`⏱️ Scheduling Bluetooth resume…`) expected once OTA wraps.
**Notes**:
- Prevents Bluedroid callbacks from firing while the controller is still tearing down, and gives the human operator an 8 s window to launch another OTA without the speaker reconnecting.
- Delay is centralized in the controller (`kResumeDelayMs`); adjust there if we need a longer/shorter window.
- Follow-up: run consecutive OTA uploads to confirm BTC crash is gone and check that Bluetooth auto-resumes after the timeout.

### Step 24: 2025-10-25 02:25 MDT – Back-to-back OTA regression test
**Action**: Flashed latest firmware over USB, then executed two OTA uploads in succession with Bluetooth enabled while logging serial output.
**Result**:
- **Run 1** (00:43 MDT) – OTA progressed to 24 % before the host reported `Error Uploading` (network hiccup). No crash, controller stayed paused.
- **Run 2** (00:45 MDT) – OTA completed successfully (`Result: OK` at ~4 min 48 s). Device rebooted cleanly and Wi-Fi stayed online.
**Notes**:
- After each attempt the controller stayed paused; Bluetooth retry logs only resumed once the deferred timer expired.
- Need an additional success capture that shows the scheduled resume log (current serial window ended before the 8 s timer fired). Next manual test should keep the USB capture running a bit longer to confirm the new message.
- Overall behaviour: no Guru Meditation across consecutive uploads, but OTA transport still sensitive to Wi-Fi fluctuations (24 % failure). Consider monitoring RSSI or backing off upload rate if flaky network persists.

### Step 25: 2025-10-25 02:45 MDT – Immediate retry still triggers BTC load fault
**Action**: Fired another OTA within ~10 s of the prior success while capturing serial (`/tmp/ota_serial_capture4.log`). OTA handshake succeeded but the ESP32 panicked (`LoadProhibited` in `btc_a2dp_cb_handler`) before data transfer began.
**Result**: **FAILED** – Crash persists if the next OTA starts before Bluetooth has finished resuming/tearing down.
**Notes**:
- Despite the deferred resume guard we still delete the A2DP object mid-callback. Need a safer pause path—either wait for `btc_a2dp_cb_handler` to drain before nulling the globals or keep the object alive (skip delete) and simply mute audio/stop retries during OTA.
- Until resolved, give the skull ~30 s between OTA attempts (or toggle `bluetooth_enabled=false`) to avoid the BTC crash.

### Step 26: 2025-10-25 03:20 MDT – Async disconnect wait + controller teardown refresh
**Action**: Reworked `pauseForOta()` to avoid blocking: request `disconnect()`, mark a wait flag, and defer the heavy `end(false)` + controller/bluedroid disable into a new `finalizePause()` that runs from `update()` once the link drops (or a timeout hits). Removed the temporary deletion of the A2DP object so ESP32-A2DP’s globals stay valid throughout the BTC callbacks. Reflashed via USB, then ran two OTA uploads back-to-back with Bluetooth enabled, capturing serial (`/tmp/ota_runA.log`, `/tmp/ota_runB.log`).
**Result**: **SUCCESS** – Both OTA uploads completed (`Result: OK`), with logs showing `⏸️ Pausing…`, `🔻 Disconnecting…` (once), and `⏱️ Scheduling Bluetooth resume 8.0s after OTA`. No Guru Meditation or watchdog resets during the rapid second attempt.
**Notes**:
- The async path keeps ArduinoOTA responsive while waiting for the A2DP disconnect, eliminating the previous interrupt watchdog trip.
- Bluetooth resumes automatically after the firmware reboot; the deferred resume ensures we honour the post-OTA quiet window for scenarios where OTA aborts without a reset.
- Remaining OTA failures earlier in the night were due to Wi-Fi transport (timeouts at ~24 %); with the new guard the stack stays alive for immediate retries, so future tuning can focus on improving Wi-Fi reliability instead of Bluetooth crashes.

### Step 27: 2025-10-25 03:30 MDT – Remove hardcoded OTA IP
**Action**: Pointed `platformio.ini`’s `esp32dev_ota` `upload_port` to `${sysenv.DEATH_FORTUNE_HOST}` and refreshed `docs/ota.md` instructions. Discovery scripts (`scripts/discover_esp32.py`, `pio run -e esp32dev_ota -t ota_upload_auto`) already emit the export command, so operators just need to source the variable or use the auto-upload task.
**Result**: **SUCCESS** – No more stale IPs in version control; the existing workflow now feeds PlatformIO directly. Attempting an OTA without the variable set still fails fast, so docs emphasize running discovery first.
**Notes**:
- Keeps the repo agnostic to DHCP churn.
- Future enhancement: wrap `pio run -e esp32dev_ota -t ota_upload_auto` as the default VS Code task so manual exports are optional.

### Step 8: ESP32 Reflash and OTA Test
**Action**: Reflashed ESP32 via USB with current firmware, tested OTA upload
**Result**: **MAJOR SUCCESS** - OTA service now responding!
**Key Success Indicators**:
- ✅ Authentication successful: "Authenticating...OK"
- ✅ Connection established: "Waiting for device..."
- ✅ Upload progress: Reached 64% before failing
- ✅ OTA service responding: No more "No response from the ESP"

**Upload failed at 64% with "Error Uploading"** - This is a network stability issue, not an OTA service issue.

## Current Status

- ESP32: Online and reachable
- WiFi: Connected with proper credentials  
- Telnet: Working with password authentication
- OTA Service: **WORKING** - Authentication and connection successful
- Partition Layout: Optimized and correct
- Network: No firewall issues
- **Issue**: Upload fails at 64% - likely network stability or timeout issue

## Known Relevant Details

- **SPIFFS Partition**: NOT required for OTA updates. Only need two OTA partitions (ota_0, ota_1)
- **Firewall**: User has no firewall, and OTA worked previously
- **WiFi Config**: Properly configured on SD card with credentials
- **Partition Layout**: Current layout is sufficient for OTA (no SPIFFS needed)
- **Previous Success**: OTA uploading worked before, broke today
- **ESP32 Connectivity**: ESP32 is online, telnet works, WiFi connected
- **Port 3232**: OTA service not responding on this port

## Debugging Steps

### Step 1: Initial OTA Failure Investigation
**Action**: Tested OTA upload, got "No response from the ESP"
**Result**: FAILED - OTA service not responding
**Why Failed**: Unknown at time

### Step 2: Telnet Authentication Fix
**Action**: Modified `scripts/telnet_command.py` to handle password authentication
**Result**: SUCCESS - Telnet connections now work with password
**Why This Didn't Fix OTA**: Telnet and OTA are separate services

### Step 3: ESP32 IP Address Discovery
**Action**: Used discovery scripts to find ESP32 IP, updated `platformio.ini`
**Result**: SUCCESS - Found ESP32 at 192.168.86.46, updated config
**Why This Didn't Fix OTA**: IP was correct, issue was deeper

### Step 4: Partition Size Investigation
**Action**: Checked partition sizes, found 199% flash usage, increased OTA partition sizes
**Result**: SUCCESS - Optimized partition layout (94.4% usage, 300KB+ free space)
**Why This Didn't Fix OTA**: Partition layout was correct, issue was in firmware

### Step 5: ESP32 Reflash with New Partitions
**Action**: Reflashed ESP32 via USB with corrected partition layout
**Result**: SUCCESS - ESP32 booted with new partitions
**Why This Didn't Fix OTA**: Partition layout wasn't the root cause

### Step 6: Auto-Discovery System Test
**Action**: Used `python scripts/ota_upload_auto.py` to test auto-discovery OTA
**Result**: FAILED - Still "No response from the ESP"
**Why This Didn't Fix OTA**: Auto-discovery works, but OTA service still not responding

### Step 7: Code Analysis and Syntax Error Discovery
**Action**: Analyzed `src/main.cpp` for initialization issues
**Result**: **CRITICAL BUG FOUND** - Missing opening brace `{` on line 259
**Code Issue**:
```cpp
if (wifiSSID.length() > 0 && wifiPassword.length() > 0)  // Missing {
    LOG_INFO(WIFI_TAG, "Initializing Wi-Fi manager");
    // ... WiFi initialization code never executes
```

**Impact**: This syntax error prevents WiFi initialization code from executing, which means:
- WiFi connection callback never runs
- OTA service never gets initialized
- Port 3232 never opens
- OTA uploads fail with "No response from the ESP"

## Root Cause Identified

**Primary Issue**: Syntax error in `main.cpp` line 259 - missing opening brace `{` after if statement

**Secondary Issues**: None identified. All other systems (WiFi config, partitions, network) are working correctly.

## Next Steps

1. Fix syntax error in `main.cpp` line 259
2. Reflash ESP32 with corrected code
3. Test OTA upload functionality

## Files Modified During Debugging

- `scripts/telnet_command.py` - Added password authentication
- `partitions/fortune_ota.csv` - Optimized partition layout
- `platformio.ini` - Updated IP address (dynamic via scripts)
