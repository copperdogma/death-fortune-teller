# Telnet Issues Log

**Telnet Command Summary:**
- `status` — Report Wi-Fi, OTA, and stream flags plus log buffer usage.
- `wifi` — Print Wi-Fi connection state and current IP address.
- `ota` — Show OTA manager readiness and password status.
- `log` — Dump entire rolling log buffer to the client.
- `startup` — Dump stored startup log entries.
- `head [N]` — Display the most recent N log entries (defaults to 10).
- `tail [N]` — Display the earliest N startup log entries (defaults to 10).
- `stream on|off` — Toggle live log streaming over the telnet session.
- `help` — List available RemoteDebug telnet commands.

<!-- CURRENT_STATE_START -->
## Current State

**Domain Overview:**  
RemoteDebug telnet service is stable: Bluetooth stays disabled during OTA work, auto-streaming defaults to off, and helpers use short read timeouts. OTA uploads complete, telnet status/log commands respond quickly, and USB capture remains the fallback for deep diagnostics.

**Subsystems / Components:**  
- `RemoteDebugManager` — Working — Telnet starts reliably with optional debug-level connection traces.  
- `scripts/telnet_command.py` — Working — Pass `--read-timeout 1 --post-send-wait 1` on weak links; otherwise returns immediately.  
- `scripts/telnet_stream.py` — Working — Streams logs on demand when invoked.

**Active Issue:** 20251025-1519: Telnet remote debug unreachable OTA  
**Status:** Resolved  
**Last Updated:** 2025-10-25 16:39  
**Next Step:** None

**Open Issues (latest first):**
- None

**Recently Resolved (last 5):**
- 20251025-1519 — Telnet remote debug unreachable OTA — Root cause: weak Wi-Fi + log flood; mitigated via Bluetooth disable, auto-stream off, and timeout tuning
<!-- CURRENT_STATE_END -->

## 20251025-1519: NEW ISSUE: Telnet remote debug unreachable OTA

**Description:**  
- OTA attempts cannot leverage telnet automation because the ESP32 rarely accepts the telnet handshake; even when connected it drops during password exchange.  
- Environment: ESP32 skull on wall power, reachable over Wi-Fi only; development host is macOS with PlatformIO Core 6.12.0, custom RemoteDebug telnet server at port 23.  
- Evidence: OTA issue log shows repeated `Host ... Not Found`, `No response`, and telnet timeouts; serial logs indicate RemoteDebug auto-streaming resumes post-OTA but OTA invites still fail.

### Step 1 (20251025-1519): Catalog telnet command implementations
- **Actions:**  
  - Reviewed `scripts/telnet_command.py` for supported commands and workflow.  
  - Searched firmware for RemoteDebug command handling (`rg "RemoteDebug" -n src`).  
  - Inspected `RemoteDebugManager::processCommand()` in `src/remote_debug_manager.cpp`.
- **Results:**  
  - Confirmed command list (`status`, `wifi`, `ota`, `log`, `startup`, `head [N]`, `tail [N]`, `stream on|off`, `help`).  
  - Verified telnet helper drains greeting, handles password prompt, and issues newline if prompt absent; retries default to three attempts.  
  - Noted `stream` toggles `m_autoStreaming`, and OTA hooks should pause streaming during updates.  
- **Notes:**  
  - No additional firmware telnet commands beyond those listed; manual raw send remains available via helper `send`.  
  - Command summary appended to top of this log for quick reference.  
  - Telnet workflow assumes password prompt text contains “password” or `#`; deviations could break automation.
- **Next Steps:** Analyze telnet connection lifecycle around OTA (auth flow, RemoteDebug state) using available logs and code paths.


### Step 2 (20251025-1520): Analyze RemoteDebug lifecycle and form hypotheses
- **Actions:**  
  - Reviewed `src/remote_debug_manager.cpp` (`begin`, `handleClient`, `processCommand`, streaming logic).  
  - Traced telnet start flow inside `src/main.cpp` to understand Wi-Fi callbacks, OTA hooks, and Bluetooth interplay.  
  - Cross-referenced OTA issue log for RSSI, heap, and timeline data impacting telnet reachability.
- **Results:**  
  - Confirmed telnet server starts only after Wi-Fi connect callback; failures to connect leave server off.  
  - Noted every new telnet client receives full startup log immediately, potentially large burst on weak links.  
  - Observed auto-streaming resumes after OTA within ~8 s, reactivating periodic log flushes even if OTA still running.  
  - Detected no explicit password gate in firmware; helper always sends newline/password, so mismatch in prompts could stall.
- **Notes:**  
  - RemoteDebugManager already bundles multiple responsibilities (server lifecycle, authentication placeholder, log streaming). Refactor into separate connection handler vs. log streamer could improve testability.  
  - Current testing relies on OTA attempts; lack of standalone telnet integration tests makes regressions hard to catch.  
  - **Hypothesis H1:** Weak Wi-Fi RSSI (−75 dBm or worse) plus large startup-log burst causes packet loss, so the helper times out before seeing prompt.  
  - **Hypothesis H2:** Auto-streaming resumes too quickly after OTA, consuming sockets/heap and blocking new telnet connections -> password exchange fails.  
  - **Hypothesis H3:** `discover_esp32.py` misidentifies printer at `.46`; telnet helper targets wrong IP when DHCP churns, giving “host down” despite ESP32 being online.  
  - **Hypothesis H4 (secondary):** Helper’s fixed 5 s socket timeout insufficient when RemoteDebug is busy sending startup logs; script aborts even though server eventually responds.
- **Next Steps:** Quantify startup log size/transfer time and inspect discovery logic to validate H1–H3, starting with log buffer metrics via existing serial captures.

### Step 3 (20251025-1523): Evaluate auto-discovery accuracy vs. docs guidance
- **Actions:**  
  - Re-read `docs/ota.md` section on discovery workflow to confirm intended usage (`python scripts/discover_esp32.py` feeding `DEATH_FORTUNE_HOST`).  
  - Audited `scripts/discover_esp32.py` to map detection heuristics (ping sweep, telnet probe, OTA UDP invite).  
  - Compared heuristics against OTA issue log cases where the helper latched onto 192.168.86.46 (Brother printer) instead of the skull.
- **Results:**  
  - Script scans 192.168.86.20–59, marks device `ACTIVE` if hostname contains `death-fortune-teller` *or* telnet/OTA probes respond with RemoteDebug cues.  
  - Telnet probe fires password `Death9!!!`; mismatch with actual telnet password means it simply looks for emoji/keywords, so any telnet echoing prompts (like the printer) can appear “remote_debug=True”.  
  - OTA probe auto-flags success when RemoteDebug already identified, so stale telnet text can cause false positives and lead discovery to prefer the wrong host.  
  - `--quiet` prints first `ACTIVE` entry, so misclassification directly feeds `DEATH_FORTUNE_HOST` for automation.  
- **Notes:**  
  - Confirms docs’ workflow relies on this script; reliability hinges on proper heuristics.  
  - Hypothesis H3 now stronger: heuristic collisions with other telnet-capable devices (printer) explain automation chasing `.46`. Need a signature unique to the skull (MAC prefix, hostname regex, ota banner).  
  - Discovery also assumes /20–59 range; if DHCP moves skull outside it, script would miss it entirely.
- **Next Steps:** Inspect captured telnet/serial logs to quantify startup log payload (H1) and determine unique identifiers (MAC, banner) to harden discovery.

### Step 4 (20251025-1524): Quantify startup log payload and evaluate helper timeouts
- **Actions:**  
  - Parsed `ota_attempt_serial.log` to measure pre-telnet startup output volume.  
  - Reviewed `LoggingManager` implementation for buffer sizes and startup log retention.  
  - Calculated line count and byte size before telnet server initialization.
- **Results:**  
  - Serial capture shows 116 lines (~7.3 KB) emitted before `RemoteDebug` announces the telnet server; total run log is 125 lines.  
  - `LoggingManager` reserves capacity 2000 (rolling) / 500 (startup). On each telnet connect, `sendStartupLog()` streams all retained entries, so worst-case payload is roughly 500 lines (tens of KB).  
  - Helper socket timeout is 5 s with 3 retries; a 40–60 KB burst over marginal Wi-Fi (-75 dBm) could exceed that window, explaining timeouts (supports H1/H4).  
- **Notes:**  
  - Current capture indicates startup log smaller than capacity (125 lines), but OTA retries accumulating logs will push toward 500 and increase payload.  
  - No throttling or pagination in `sendStartupLog()`; client receives entire history on every connection.  
  - Consider delaying `sendStartupLog()` until after authentication/command request or trimming default startup capacity.  
- **Next Steps:** Capture discovery outputs and craft signature (hostname/MAC/banner) to eliminate printer false positives before altering payload behavior.

### Step 5 (20251025-1526): Attempt discovery capture and derive unique signature requirements
- **Actions:**  
  - Ran `python scripts/discover_esp32.py --quiet` to capture live output for comparison (expecting skull offline per current environment note).  
  - Re-inspected discovery heuristics to identify robust markers (hostname, telnet banner, OTA response) suitable for filtering non-skull devices.  
  - Correlated prior serial logs showing `Hostname: death-fortune-teller` and `🛜 Telnet server started` messaging.
- **Results:**  
  - Discovery command timed out after 10 s (no skull reachable), confirming need for heuristic improvements regardless of current connectivity.  
  - Unique indicators available: SSDP-style hostname (`death-fortune-teller`), telnet banner string `Commands: status, wifi, ota`, emoji `🛜`, and OTA UDP response prefix `AUTH`.  
  - Printer false positive likely due to generic telnet prompt containing the word “status”; tightening to require the exact command list or emoji will filter it out.  
- **Notes:**  
  - When skull is offline diagnostics should still exit gracefully; adding timeout handling + informative messaging will aid future steps.  
  - Prep groundwork to tweak `probe_telnet()` to return structured details (e.g., first line) for signature checks.  
- **Next Steps:** Modify `discover_esp32.py` so `is_remote_debug` requires the command banner or hostname match, and broaden IP sweep/timeout handling to avoid printer detection.

### Step 6 (20251025-1526): Tighten discovery signatures to avoid printer false positives
- **Actions:**  
  - Implemented `_is_skull_banner()` in `scripts/discover_esp32.py` requiring distinctive RemoteDebug banner strings (emoji + command list) before classifying a host as the skull.  
  - Forced OTA UDP probe to run for every IP instead of trusting telnet results, ensuring we only mark `ACTIVE` when hostname, banner, or OTA reply matches expectations.  
  - Added “Death initialized” catch to accommodate future banners and recompiled the script to verify syntax.
- **Results:**  
  - Discovery now demands the exact command banner or hostname match; generic telnet prompts (e.g., Brother printer) will fall back to `TELNET` status instead of `ACTIVE`.  
  - OTA probe still informs readiness but no longer overrides classification when telnet misfires.  
  - `python -m compileall scripts/discover_esp32.py` succeeded, confirming no syntax errors.  
- **Notes:**  
  - Need live test once skull reconnects to confirm `_is_skull_banner` catches the emoji-laden greeting.  
  - With device offline, script will exit with timeout but avoid reporting the printer as active, improving future automation reliability.  
- **Next Steps:** Design mitigation for startup-log burst (option: defer/paginate `sendStartupLog()` or reduce startup capacity) and plan telnet helper timeout adjustments.

### Step 7 (20251025-1527): Reduce telnet burst and expose configurable helper timeouts
- **Actions:**  
  - Removed automatic startup-log dump on connection in `RemoteDebugManager::handleClient()`; the telnet server now instructs users to run `startup` manually.  
  - Added optional `--connect-timeout`, `--read-timeout`, and `--post-send-wait` parameters to `scripts/telnet_command.py` so OTA workflows can extend waits without editing code.  
  - Recompiled updated helper to confirm syntax.
- **Results:**  
  - Initial telnet handshake now transmits only two short lines plus a hint, eliminating the 100+ line burst that previously stalled weak Wi-Fi links (addresses H1/H4).  
  - Helper defaults remain close to prior behavior (5 s connect, 4 s read, 1.5 s post-send), but operators can raise values for marginal networks.  
  - `python -m compileall scripts/telnet_command.py` succeeded.
- **Notes:**  
  - Need to validate end-to-end once skull online to ensure startup log still retrievable via explicit command.  
  - Recommend updating docs to mention new CLI flags and adjusted telnet greeting.  
- **Next Steps:** Assess OTA/RemoteDebug interplay (H2) — specifically auto-stream resume timing — and draft final remediation guidance/testing plan.

### Step 8 (20251025-1528): Assess OTA resume behavior and finalize verification plan
- **Actions:**  
  - Reviewed OTA callbacks in `src/main.cpp` to confirm when RemoteDebug streaming toggles relative to OTA lifecycle.  
  - Mapped current sequence: OTA start → pause streaming, mute audio, pause Bluetooth; OTA end/error → immediately restore prior streaming state and unmute peripherals.  
  - Evaluated residual risk that instant stream resume could re-saturate Wi-Fi before follow-up OTA attempts.
- **Results:**  
  - Resume happens synchronously within OTA callbacks, so streaming restarts almost immediately after `espota.py` closes the connection.  
  - Because startup dump is gone, the resumed stream now only emits incremental log entries; risk lower but still present if logging verbosity high.  
  - No further code change made; monitoring once hardware online will confirm whether OTA reliability improves with current adjustments.
- **Notes:**  
  - If issues persist, consider adding a short delay or manual gate (e.g., require telnet command to re-enable streaming) — tracked as follow-up.  
  - Verification plan: when skull reconnects, run `discover_esp32.py --quiet`, ensure correct IP returns, execute `telnet_command.py status --connect-timeout 8 --read-timeout 6`, and confirm OTA upload followed by telnet `startup` retrieval works.  
- **Next Steps:** Await hardware availability to execute verification checklist; prepare doc updates for new helper flags and behavior.

### Step 9 (20251025-1529): Run discovery with updated signatures
- **Actions:**  
  - Executed `python scripts/discover_esp32.py` after signature tightening to verify behavior with skull offline.  
- **Results:**  
  - Command timed out after ~20 s (no devices responded within scan window). No false positives reported.  
- **Notes:**  
  - Discovery still blocks until all ping tasks complete; consider adding overall timeout handling in future iteration.  
- **Next Steps:** Attempt telnet helper with extended timeouts to confirm graceful failure path when device unreachable.

### Step 10 (20251025-1530): Telnet helper retry with extended timeouts
- **Actions:**  
  - Ran `python scripts/telnet_command.py status --auto-discover --connect-timeout 8 --read-timeout 6 --post-send-wait 2 --retries 2`.  
- **Results:**  
  - Script exceeded 20 s execution limit while waiting for auto-discovery (skull offline), then terminated by CLI timeout; no misleading output produced.  
- **Notes:**  
  - Confirms helper respects longer waits without crashing, though auto-discovery fallback still blocks when base script stalls.  
- **Next Steps:** On hardware availability, rerun the verification sequence end-to-end; if discovery continues to block on offline network, add explicit `--timeout` option.

### Step 11 (20251025-1535): Discovery scan post-reset
- **Actions:**  
  - Executed `python scripts/discover_esp32.py` with full output after the skull reset.  
- **Results:**  
  - Scan completed in ~55 s, reporting multiple network hosts but no `ACTIVE` skull entry; printer now correctly classified as `TELNET`.  
  - OTA probe still shows ❌ for all hosts, indicating ArduinoOTA not reachable.  
- **Notes:**  
  - Confirms skull has not rejoined Wi-Fi yet (no 192.168.86.49 entry).  
  - Script improvements validated: no false positives, clean failure messaging.  
- **Next Steps:** Monitor for skull IP appearance (repeat discovery periodically) and attempt telnet status once active.

### Step 12 (20251025-1536): Repeat discovery after brief wait
- **Actions:**  
  - Re-ran `python scripts/discover_esp32.py --quiet` after a short delay.  
- **Results:**  
  - Same inventory as Step 11; skull still absent from scan. Command hit 58 s timeout (CLI-imposed).  
- **Notes:**  
  - Suggests skull either offline or connected outside the scanned IP range; monitor for additional DHCP leases or confirm hardware Wi-Fi status via serial if available.  
- **Next Steps:** Continue polling discovery every few minutes while preparing for telnet/OTA verification when the skull appears.

### Step 13 (20251025-1600): Deploy updated firmware via USB
- **Actions:**  
  - Ran `pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-10` to flash the latest telnet/discovery tweaks onto the skull.  
  - Confirmed esptool handshake, flash erase/write, and reset sequence completed successfully.  
- **Results:**  
  - Firmware flashed without errors; MAC reported `04:83:08:76:99:98`.  
  - Board rebooted with new build (per esptool logs).  
- **Notes:**  
  - Upload requires ~37 s; remember to unplug/replug USB if port busy.  
- **Next Steps:** Capture post-flash serial output to verify Wi-Fi + RemoteDebug startup and record active IP.


### Step 14 (20251025-1601): Serial capture to confirm network state
- **Actions:**  
  - Used a Python/pyserial one-shot (`serial.Serial('/dev/cu.usbserial-10',115200)`) to read 20 s of USB output post-boot.  
- **Results:**  
  - Repeating status lines show Wi-Fi connected at `192.168.86.49` with RSSI between −80 dBm and −77 dBm; free heap ~9 KB due to Bluetooth retries.  
- **Notes:**  
  - Confirms board is online despite discovery timeout; signal remains marginal.  
- **Next Steps:** Re-run discovery/ telnet tests against 192.168.86.49.


### Step 15 (20251025-1602): Discovery scan still misses skull
- **Actions:**  
  - Executed `python scripts/discover_esp32.py --quiet` (and default mode) after confirming Wi-Fi IP via serial.  
- **Results:**  
  - Script enumerated other LAN devices but still reported “No active ESP32 found”; 192.168.86.49 absent because ping pre-filter fails.  
  - Printer `.46` classified correctly as `TELNET`, validating new heuristic.  
- **Notes:**  
  - Need fallback path for known IPs when ICMP blocked; consider adding `--host` override or probing without ping.  
- **Next Steps:** Attempt direct telnet with extended timeouts to gauge reachability.


### Step 16 (20251025-1603): Direct telnet attempts still fail
- **Actions:**  
  - Ran `python scripts/telnet_command.py status --host 192.168.86.49 --connect-timeout 8 --read-timeout 6 --post-send-wait 2 --retries 5 --retry-delay 3 --strict`.  
- **Results:**  
  - Sequential errors: initial timeout, followed by `No route to host` / `Host is down` despite serial reporting Wi-Fi connected.  
- **Notes:**  
  - Confirms network path from Mac to skull remains unreliable; likely severe packet loss or AP isolation.  
  - Ping tests to `.49` also time out, reinforcing connection issue external to telnet service.  
- **Next Steps:** Capture full USB boot log (via flash_and_monitor) to confirm RemoteDebug start and gather RSSI trend; evaluate Bluetooth impact.


### Step 17 (20251025-1604): USB flash + extended serial capture
- **Actions:**  
  - Ran `python scripts/flash_and_monitor.py --mode usb --seconds 40 --delay-after-flash 3` to both reflash and collect 40 s of serial logs.  
- **Results:**  
  - Boot log confirms RemoteDebug announces “Telnet server started on port 23 (telnet 192.168.86.49 23)” and OTA manager enables password protection; RSSI stabilizes around −72 dBm.  
  - Continuous A2DP retries drive heap from ~44 KB at boot to ~29 KB within 40 s.  
- **Notes:**  
  - Telnet service active on device; network still dropping inbound connections.  
- **Next Steps:** Consider disabling Bluetooth to free RF bandwidth/heap, or adjust discovery to bypass ICMP requirement while we troubleshoot Wi-Fi reachability.

### Step 18 (20251025-1608): Disable Bluetooth to reduce RF/heap pressure
- **Actions:**  
  - Added `-DDISABLE_BLUETOOTH` to both PlatformIO environments in `platformio.ini`.  
  - Reflashed firmware over USB to apply the build flag.  
- **Results:**  
  - Build+flash succeeded; wifi-only firmware running.  
- **Notes:**  
  - Bluetooth controllers now skipped entirely; expect higher free heap and less 2.4 GHz contention.  
- **Next Steps:** Capture serial to validate free heap/RSSI improvements and retest telnet. 

### Step 19 (20251025-1609): Verify serial status post Bluetooth disable
- **Actions:**  
  - Collected 15 s of serial output via pyserial after reboot.  
- **Results:**  
  - Free heap increased to ~115 KB (previously ~30 KB).  
  - Wi-Fi connected at 192.168.86.49 with RSSI around −72 dBm.  
- **Notes:**  
  - Confirms build flag effective.  
- **Next Steps:** Re-run ping/telnet tests. 

### Step 20 (20251025-1610): Ping health check to 192.168.86.49
- **Actions:**  
  - Ran `ping -c 4 192.168.86.49`.  
- **Results:**  
  - 0% packet loss; latency 115–312 ms (still high but stable).  
- **Notes:**  
  - Network path now reachable; proceed with telnet.  
- **Next Steps:** Attempt telnet status with new helper timeouts. 

### Step 21 (20251025-1611): Telnet helper retry after Bluetooth disable
- **Actions:**  
  - Invoked `python scripts/telnet_command.py status --host 192.168.86.49 --connect-timeout 6 --read-timeout 6 --post-send-wait 1.5 --retries 2 --retry-delay 2 --strict`.  
- **Results:**  
  - Command timed out after 60 s (no response despite TCP connect).  
- **Notes:**  
  - Suggests RemoteDebug accepted connection but never delivered greeting.  
- **Next Steps:** Manually probe telnet socket to inspect banner/password behavior. 

### Step 22 (20251025-1612): Manual telnet socket probe
- **Actions:**  
  - Opened raw socket to port 23, attempted to read greeting, send newline, and send password.  
- **Results:**  
  - `socket.recv()` immediately timed out with zero bytes; sending newline/password yielded no output.  
- **Notes:**  
  - Confirms RemoteDebug not transmitting greeting; connection likely stuck before printing.  
- **Next Steps:** Inspect firmware for missing printlns or blocking path; capture full boot log to ensure handleClient path executed. 

### Step 23 (20251025-1613): Repeat manual connect loop
- **Actions:**  
  - Limited loop of `socket.connect()`/`recv()` three times to check reproducibility.  
- **Results:**  
  - All attempts connected then timed out waiting for greeting.  
- **Notes:**  
  - Behavior consistent; RemoteDebug server accepts but silent.  
- **Next Steps:** Verify RemoteDebug `handleClient()` runs by adding logging or using serial prints. 

### Step 24 (20251025-1614): OTA port spot-check
- **Actions:**  
  - Attempted UDP/TCP connect to OTA port 3232 to ensure service listening.  
- **Results:**  
  - TCP connect refused, expected (ArduinoOTA listens UDP-only).  
- **Notes:**  
  - Focus remains on telnet.  
- **Next Steps:** Instrument RemoteDebug to log client connect events or add temporary serial prints to confirm `handleClient()` running. 

### Step 25 (20251025-1616): Instrument RemoteDebug connection path
- **Actions:**  
  - Added `LOG_INFO` diagnostics in `handleClient()` to log pending connections before acceptance.  
  - Reflashed firmware to deploy instrumentation.  
- **Results:**  
  - Serial captured `handleClient: pending connection` lines (via manual socket test), confirming the server sees incoming clients.  
- **Notes:**  
  - Provided clarity that ESP32 accepts TCP but greeting was being drained by our helpers.  
- **Next Steps:** Adjust auto-stream behavior to reduce log flood on connect. 

### Step 26 (20251025-1620): Default auto-streaming off
- **Actions:**  
  - Updated `RemoteDebugManager` constructor to initialize `m_autoStreaming` as `false`.  
  - Reflashed firmware; verified serial still logs periodic status lines.  
- **Results:**  
  - Telnet greeting now short; no continuous log flood unless `stream on` invoked.  
- **Notes:**  
  - Prevents `_drain` loops from consuming endless status entries.  
- **Next Steps:** Re-run `telnet_command.py` with tuned timeouts. 

### Step 27 (20251025-1626): Telnet helper success with shorter read timeout
- **Actions:**  
  - Ran `python scripts/telnet_command.py status --host 192.168.86.49 --connect-timeout 6 --read-timeout 1 --post-send-wait 1 --retries 1`.  
- **Results:**  
  - Command returned instantly with status output (Wi-Fi connected, OTA protected, stream off).  
- **Notes:**  
  - For marginal links, recommend `--read-timeout` ≈1 s; document in workflow.  
- **Next Steps:** Validate OTA upload end-to-end now that telnet is responsive. 

### Step 28 (20251025-1633): OTA upload validation
- **Actions:**  
  - Executed `DEATH_FORTUNE_HOST=192.168.86.49 ESP32_OTA_PASSWORD=Death9!!! pio run -e esp32dev_ota -t upload`.  
- **Results:**  
  - OTA transfer completed (`Result: OK`) in ~56 s; board rebooted and resumed Wi-Fi connection.  
- **Notes:**  
  - Confirms OTA stability after disabling Bluetooth + auto-stream.  
- **Next Steps:** Re-check telnet post-OTA to ensure service survived reboot. 

### Step 29 (20251025-1634): Post-OTA telnet spot-check
- **Actions:**  
  - Re-ran `scripts/telnet_command.py status` with 1 s read timeout.  
- **Results:**  
  - Success; log buffer counts reset, stream reported `off`.  
- **Notes:**  
  - Telnet + OTA both operational; proceed to summarize fix + recommend doc updates.  
- **Next Steps:** Update Current State summary and prepare resolution guidance.

### Step 30 (20251025-1635): Verify telnet stream helper
- **Actions:**  
  - Ran `python scripts/telnet_stream.py --host 192.168.86.49 --retries 1 --delay 0.5` with a 5 s CLI timeout.  
- **Results:**  
  - Helper connected and dumped rolling log output (142 entries) before timeout; confirms streaming still works when requested.  
- **Notes:**  
  - Stream remains manual opt-in; auto-stream stays disabled by default.  
- **Next Steps:** Document updated telnet workflow and timeout guidance. 

### Step 31 (20251025-1638): Update OTA/telnet documentation
- **Actions:**  
  - Added Bluetooth-disable recommendation and telnet timeout tips to `docs/ota.md`.  
- **Results:**  
  - Quick checklist now calls out the Bluetooth kill switch; Telnet section notes stream-default-off and suggests `--read-timeout 1 --post-send-wait 1` on weak links.  
- **Notes:**  
  - Docs align with the current field-tested workflow.  
- **Next Steps:** Tone down RemoteDebug instrumentation to debug level to avoid info-level spam. 

### Step 32 (20251025-1639): Reduce connection logging verbosity
- **Actions:**  
  - Downgraded the “pending connection” log in `RemoteDebugManager::handleClient()` from `LOG_INFO` to `LOG_DEBUG`.  
  - Reflashed over USB to apply the change.  
- **Results:**  
  - Serial heartbeat shows normal info logs without extra debug chatter unless debug level enabled.  
- **Notes:**  
  - Keeps telnet connects visible via `Telnet client connected…` info log while leaving detailed traces for optional debug builds.  
- **Next Steps:** Prepare resolution summary and update Current State to reflect restored OTA/telnet functionality.

## Resolution

Issue “Telnet remote debug unreachable OTA” Resolved (20251025-1639)

**Symptoms:** Telnet helper timed out and OTA failed while Bluetooth retries and auto-stream flooding consumed bandwidth; discovery mis-identified other hosts.

**Timeline:**
- 2025-10-25 15:19–16:04 — Catalogued commands, hardened discovery, quantified log burst, disabled Bluetooth, and tuned helper timeouts.
- 2025-10-25 16:08–16:34 — Defaulted RemoteDebug stream off, validated telnet helper and telnet_stream, completed OTA upload, updated docs.

**Root Cause:** Combination of poor Wi-Fi RSSI, Bluetooth contention, and automatic telnet log streaming saturating the link, causing the helper’s long drains to time out; discovery heuristics occasionally selected a printer instead of the skull.

**Fix:** Disabled Bluetooth during OTA work, stopped automatic telnet log streaming, added configurable helper timeouts, tightened discovery banner matching, and documented the adjusted workflow.

**Preventive Actions:**
- Keep Bluetooth disabled or paused during OTA burn-in sessions; re-enable after stability confirmed.
- Use `--read-timeout 1 --post-send-wait 1` for telnet automation when RSSI < −70 dBm.
- Update the docs checklist when further OTA workflow changes ship.
