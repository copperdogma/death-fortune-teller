# Story 018 — Retained fault reports and rolling SD logs

Status: In Review. Implementation and machine/device checks complete; human review pending.

## Intent / acceptance

Cam authorizes implementing the proposed rolling RAM log, compact internal-flash fault fallback and bounded SD reports. When SD cannot write, keep the frozen report in RAM and retry on a cadence. Do not fill the card or interfere with playback, Matter fabrics, saved settings or audio files.

- Fixed recent-event ring and first-fault snapshot; later errors coalesced while pending.
- Deferred SD write retries every60seconds while idle; no remount/unmount or busy-loop retries.
- Eight fixed report slots plus one bounded staging file in `/sdcard/diagnostics`.
- Compact versioned NVS report in a separate namespace; recovery across reboot, no NVS erasure and reserved free entries for Matter/system state.
- Checked write/flush/sync/close/readback before replacing an old report. Failed archive retains RAM/NVS.
- Serial status/dump and explicit idle-only controlled test. No raw config, credentials or provisioning logs captured.

## Work log

### 20261026 — Implementation

- Reviewed ESP-IDF5.5.4 reentrant log hook contract, existing native audio quiescence and NVS use. References/decision in `docs/research/2026-10-26-fault-log-retention.md`.
- Added portable bounded capture/filter/cadence core, fixed-slot archive writer and native hook/NVS wrapper. Capture is RAM-only, fixed stack/buffers; allocation in PSRAM occurs once at boot. Deferred persistence runs on existing diagnostic owner after complete show/audio/startup/tone/printer quiescence. Early SD-mount failure loop can preserve compact fault data to flash without any SD operations.
- SD archive writes only diagnostics names, never touches source assets. No automatic formatting, NVS erasure, card remount, recovery retries in audio reader or flash partition changes. One pending snapshot retained on continued faults; later errors coalesced/count visible. NVS fallback stores last roughly1KiB including fault context, not the whole rolling RAM history.
- Production tests/build and hardware verification in progress. No commit/push authorized for this new story.


### Review, automated qualification and first device export

- Independent review requested through the project-authorized lower-cost reviewer. Added configuration-summary filtering. Moved first-fault report formatting/copy out of the hook critical section: hook copies at most192bytes and freezes indices; control owner materializes the frozen history outside the lock, then resumes rolling capture even during show cleanup/cooldown. A fixed-bound guard protects report append limits.
- Portable core/archive ASan/UBSan tests PASS: ring wrap/freeze/coalescing, ANSI/tag selection, truncation, cadence, >8 exports/rotation, invalid inputs, filesystem staging/replace failures and audio preservation. Native wrapper tests PASS with fake NVS/clock/archive: no I/O during playback or hook, unavailable SD consumes60second cadence, recovery export, pending/recovered/acknowledged state across simulated reboots, reserved NVS entries, commit/ack failure and concurrent logs. All general host suites and production-source Death flow/assets/fortune suites PASS. Native ESP32 build PASS after fixing a host-versus-Xtensa integer typedef mismatch.
- App-only image SHA256 `72a5e8f915d9cb05797637b838d3abbd3c84eaeba9960d3fa3b8366bae53a856` uploaded/hash-verified. NVS and partition table unchanged. Boot mounted replacement SD immediately; startup PCM staging/PSRAM FNVd6d8b5d5; saved JBL reconnected. No pairing reset. Normal breathing default retained.
- Real device `fault-test` at34011ms triggered production hook. Internal retention ESP_OK34021ms; SD slot0 saved/readback-verified34091ms; RAM pending cleared after NVS acknowledgement. AttemptsSD1/NVS1, nextSD~60seconds. Internalfree~25.8KiB, min15.3KiB, no radio fault/drop. Raw log kept privately under ignored build validation. This verifies export machinery, not a fix for the underlying intermittent SD read failure.


### Device playback, reboot and archive rotation qualification

- Normal-jaw Welcome replay completed540540frames, expected rawFNV9f4f6cb1, peak19558, zero underruns, then cooldown/Waiting. No unexpected logger I/O during playback. No new CRC failure in this bounded replay; intermittent SD fault is not declared fixed.
- Controlled idle reboot: previous acknowledged report was not restored as pending (recovered0), no duplicate export. Saved speaker reconnected on third automatic attempt after two page/open failures (~84seconds); startup announcement delivered, no new pairing needed. This delay is recorded rather than claimed instant reconnect.
- Second labelled fault test after reboot: internal retention ESP_OK108171ms, SD archive saved/readback-verified slot1 at108221ms, RAM cleared after acknowledgement108231ms. Advancing from slot0 to slot1 across reboot verifies the persisted rotation record was loaded. No unbounded append files. Current image remains72a5e8f9… .
- Final: Waiting, normal breathing, speaker/stream available, no radio fault/drop, no pending fault report. NVS namespace and rolling diagnostics files are the only new persistent writes. General host, targeted updated core/runtime sanitizer suites, production Death suites and native build PASS; diff whitespace check PASS. Failure/retry/recovery and pending-report reboot cases are automated/fake-backend tests; actual device verified healthy export, acknowledged reboot and slot advance. No real card removal/corruption was induced to test retries. No commit/push. Story stays In Review pending human acceptance.

- Additional observed idle radio event before closing serial: selected speaker ACL disconnect reason264 at125471ms; automatic reconnect succeeded133841ms, subsequent136961/147061ms status speaker/stream1, fault0. This recovered link event is recorded separately from logging qualification and the unresolved SD CRC failure. USB relay closed without reset.


### Real user cutoff — fault retained while SD export fails

- Cam ran Home Start after deployment; speech stopped about5seconds in. Attached serial without reset and requested status/fault-log. Retained report IDb83bceb300000002 contains accepted Home Start, Welcome asset/audio progress and SDMMC read error0x109/status0x900. LAST_INTERRUPTION count1 reason audio read/worker failure phase welcome asset/audio/welcome/welcome2.wav. Delivered228608frames (~5.18s), underruns0. Speaker/stream remained1, radiofault/drop0, showreturnedWaiting.
- Logger status at~339s: pending1, coalesced4, SD attempts2, NVS attempts2, SD errno5/EIO, NVS ESP_OK, nextSD~32.5seconds. Report recoverable via fault-log without the card; full dump copied to private `story018-user-cutoff.log`. This is actual failed-card RAM retention and compact flash persistence, not merely the earlier synthetic test. SD export not yet successful at this checkpoint. Waiting for scheduled retry, without reboot or card removal.

- Scheduled retry succeeded at371561ms: SD archive slot2 saved/readback-verified, then flash acknowledgement succeeded and pending cleared. Status379371ms shows sameID, SD attempts3, NVS attempts2, SDerrno0, pending0, nextattempt~52s. This demonstrates real failed export followed by cadence-driven recovery without restarting the board; file `/sdcard/diagnostics/fault-2.log`. No code/firmware/card-content changes other than the authorized rolling report.
