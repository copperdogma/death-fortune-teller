# Story: Runtime Diagnostics Coverage & CLI Service

> **Reverted — 2025-11-06:** Plan below was superseded; we reverted the associated work to simplify the runtime strategy. Story 014 defines the current modular-runtime effort. This file remains for reference only.

**Status**: Reverted

---

## Problem Statement
`AppController::processCLI` spans 250+ lines of hand-rolled serial parsing that mutates runtime state, finger sensor tuning, printer calibration, and logging flags. The logic is tightly coupled to `Serial`, global singletons, and concrete controllers, preventing deterministic tests and making the loop nondeterministic when CLI commands block. In parallel, high-risk runtime services such as `FingerSensor`, `ThermalPrinter`, and `LoggingManager` contain bespoke state machines (drift compensation, print queue draining, remote debug toggles) without host coverage. These gaps leave calibrations, printer fault recovery, and diagnostic tooling unverified across builds, and they block reuse for future animatronic props where the same CLI/diagnostics surface is required.

## Related Requirements
- [docs/stories/story-005a-serial-console-diagnostics.md](story-005a-serial-console-diagnostics.md) — Defines expectations for a structured diagnostics console.
- [docs/stories/story-012-runtime-modularity-and-coverage.md](story-012-runtime-modularity-and-coverage.md) — Establishes interfaces for runtime seams; this story extends them to diagnostics, sensor, and printer services.
- [docs/spec.md §4 Diagnostics](../spec.md#4-diagnostics) — Requires deterministic CLI behavior and observable subsystem health.

## Acceptance Criteria
- CLI command handling lives in a dedicated, testable module (`CliCommandRouter` or similar) that exposes a pure API (e.g., `handleCommand(StringView, CommandContext&)`) decoupled from `Serial`. Unit tests cover happy paths and validation errors for calibration (`cal`), sensitivity (`fsens`/`fthresh`), sensor streaming (`fon`/`foff`), printer dumps, and help output.
- The finger sensor pipeline supports dependency injection for timing/ADC sampling so host tests can cover baseline drift, multisample averaging, and threshold clamping. Tests assert that CLI-driven adjustments update both runtime config snapshots and sensor behavior without accessing hardware.
- Thermal printer queuing is refactored behind an interface that separates queue management from hardware I/O. Host tests verify queue draining, retry/backoff, logo preload, and error latching so printer faults trigger LED indicators deterministically.
- Remote debug and logging toggles triggered by CLI (and OTA hooks) are exercised via native tests, ensuring that enabling/disabling telnet or auto streaming propagates without using real `RemoteDebug`.
- Documentation (`README` or dedicated diagnostics doc) briefly explains the new CLI module, available commands, and how to run the host test suite covering diagnostics.

## Tasks
- [ ] **CLI Router Extraction**
  - [ ] Finalize router scaffolding and remove legacy `processCLI` entry once all commands are migrated.
  - [x] Port finger sensor calibration/tuning commands (`cal`, `fsens`, `fthresh`, `fdebounce`, etc.) into the router with native tests.
  - [x] Port servo diagnostics commands (`sinit`, `smin`/`smax`, `smic`, `sdeg`, `srev`, `scfg`) into the router with native tests.
  - [x] Port printer diagnostics commands (`ptest`, `pstatus`, queue inspection) into the router with native tests.
  - [x] Port SD/config reporting commands (`sd`, `config`, `settings`) into the router with native tests.
- [ ] **Finger Sensor Test Harness**
  - [ ] Introduce abstractions for timing, ADC reads, and config sinks so `FingerSensor` can run in native tests.
  - [ ] Write host tests verifying calibration flow, sensitivity/threshold setters, streaming toggles, and debounce timing.
- [ ] **Thermal Printer Queue Coverage**
  - [ ] Isolate queue management and logo/prelude handling behind a testable adapter.
  - [ ] Add native tests covering enqueue/dequeue, error paths, and LED fault latching.
- [ ] **Logging & Remote Debug Toggles**
  - [ ] Provide a fake `RemoteDebugManager` and tests that assert CLI + OTA pathways pause/resume streaming and respect persisted flags.
  - [ ] Ensure documentation captures how to exercise diagnostics tests alongside hardware runs.

## Risks & Mitigations
- **Risk:** CLI refactor could break runtime commands if serial timing assumptions change.  
  **Mitigation:** Preserve existing command strings, add integration smoke test on hardware after refactor, and document command acknowledgement behavior.
- **Risk:** Finger sensor abstractions may add overhead on device builds.  
  **Mitigation:** Use header-only interfaces with `constexpr` optimizations; guard host-only logic behind `UNIT_TEST`.
- **Risk:** Printer queue refactor might increase flash footprint.  
  **Mitigation:** Monitor `pio run -e esp32dev -t size`; target ≤ +6 KB flash/+1 KB RAM relative to pre-story baseline.

## Work Log
### 20251106-1645 — Story drafted from diagnostics gap review
- **Result:** Success
- **Notes:** Captured uncovered CLI, finger sensor, printer, and remote debug risks; aligned acceptance criteria with deterministic host coverage.
- **Next:** Await prioritization and kick-off before implementing extraction and test scaffolds.
### 20251106-2015 — Established CLI router scaffolding & unit harness
- **Result:** Partial success
- **Notes:** Added `CliCommandRouter` abstraction with serial printer adapter stub, wired `CliService` to use router with legacy fallback, and created native tests validating help output plus fallback delegation. Router currently implements help/fhelp commands; remaining commands still flow through `legacyProcessCLI`.
- **Next:** Migrate additional command groups (finger calibration, sensor tuning, servo controls) into router and extend tests; introduce deterministic output adapter for printer diagnostics.
### 20251106-2108 — Migrated finger sensor CLI commands & tests
- **Result:** Success
- **Notes:** Injected finger sensor and debounce pointers into router dependencies, implemented calibration/tuning commands (`cal`, `fsens`, `fthresh`, `fdebounce`, `finterval`, `fon`/`foff`, `fcycles`, `falpha`, `fdrift`, `fmultisample`), and added `tests/unit/test_cli_command_router` with UNIT_TEST stubs to verify behavior and error handling. `CliService` now routes these commands without touching `legacyProcessCLI`.
- **Next:** Move remaining diagnostics (status dumps, servo/printer commands) into router and document new command coverage.
### 20251106-2203 — Ported servo diagnostics CLI commands & tests
- **Result:** Success
- **Notes:** Added servo controller injection to the router, implemented `sinit`, `smin`/`smax`, `smic`, `sdeg`, `srev`, and `scfg` handlers, and removed their legacy counterparts. Introduced a servo controller stub with tracking hooks and expanded `tests/unit/test_cli_command_router` to cover movement, min/max adjustments, pulse writes, direction toggles, and configuration reporting. Router now handles servo diagnostics end-to-end.
- **Next:** Migrate printer diagnostics (`ptest`, queue inspection) into the router and build corresponding unit coverage before tackling SD/config reporting.
### 20251106-2245 — Ported printer & SD/config CLI commands with tests
- **Result:** Success
- **Notes:** Injected thermal-printer and config/SD providers into the router, added `ptest`/`pstatus` handlers plus config (`config`/`settings`) and SD (`sd`/`sdcard`) reporting, and removed the legacy branches. Expanded CLI router tests to cover happy/error paths (ready, not-ready, failure) and SD/config delegation. Updated printer stubs to expose `isPrinting`/`hasError`, keeping existing adapters green.
- **Next:** Design a printer queue diagnostics adapter (visibility into queued fortunes and LED latch), then move on to remote-debug logging toggles and documentation updates.
