# Story: Death State Controller Extraction

**Status**: Done

---

## Problem Statement
`src/main.cpp` currently bundles the runtime state machine, UART trigger handling, printer workflow, manual calibration, and breathing loop logic into a single 2,000+ line file. The absence of clear boundaries and injectable seams makes the behavior hard to reason about, impossible to unit test without the full sketch, and risky to extend. We need to extract the state machine and associated orchestration into a dedicated controller that can run under host-side tests while the Arduino entry points delegate to it.

## Related Requirement
- [docs/stories/story-011-testability-foundations.md](story-011-testability-foundations.md) — logging seam, dependency injection, and host test harness work this story builds on.
- [docs/spec.md §3 State Machine](../spec.md#3-state-machine-runtime) — authoritative description of runtime states and transitions.

## Alignment with Design
This story refactors existing behavior without changing the state chart defined in the spec. The goal is to isolate the orchestration logic into a `DeathController` (or similar) class that uses the seams introduced by Story 011 (filesystem, random source, log sink, etc.) so the state machine decisions are testable in isolation.

## Acceptance Criteria
- A new controller/module (e.g., `death_controller.*`) owns the state machine, trigger handling, fortune generation flow, and printing coordination logic previously embedded in `main.cpp`.
- `main.cpp` (Arduino `setup()`/`loop()`) becomes thin: it wires dependencies (AudioPlayer, BluetoothController, etc.), registers the shared log sink, and delegates to the controller for updates.
- Controller accepts explicit dependency structs/interfaces (no new globals) and uses the shared seams (`infra::log_sink`, `infra::IFileSystem`, `IRandomSource`).
- Host-side unit tests cover key transitions: FAR trigger, NEAR trigger, printer unavailable, debounce logic, and fallback fortune behavior, using fakes for timers, audio, printer, and RNG.
- Firmware build still succeeds for `esp32dev`; behavior on-device remains unchanged (manual testing notes captured in work log if relevant).

## Tasks
- [x] **Document Controller Plan** — Outline controller boundaries, dependencies, and testing approach.
- [x] **Sketch Controller Shape** — define dependency struct, controller interface, and timer abstraction to replace direct `millis()`/globals.
- [x] **Extract State Machine** — move state enums, trigger handling, fortune flow scaffolding, and printing coordination into the controller with injected collaborators.
- [x] **Introduce Time/Test Seams** — add `ITimeProvider`, RNG seam, and adapters so host tests can drive deterministic time/randomness.
- [x] **Wire Arduino Entry Points** — update `setup()`/`loop()` to instantiate adapters and delegate to the controller (manual calibration & fortune flow remaining pieces tracked below).
- [x] **Author Unit Tests** — host tests cover FAR/NEAR triggers, printer readiness, snap-no-finger timeout, cooldown flow, and manual calibration hold.
- [x] **Manual Calibration Migration** — controller now owns the manual calibration stages via a driver adapter; legacy functions removed.
- [x] **Fortune Flow Migration** — move fortune loading/printing helpers into controller and drop remaining legacy globals.
- [x] **Regression Build** — ensure `pio test -e native` and `pio run -e esp32dev` remain green after full migration; document hardware validation.

## Technical Implementation Details
- Controller should encapsulate state, timers, and decision logic currently spread across `enterState`, `updateStateMachine`, `handleUARTCommand`, and related helper functions.
- Use the existing logging seam (`infra::emitLog`) for all controller logging; avoid direct `Serial` usage except inside Arduino-only paths.
- Provide a lightweight interface for asynchronous events (e.g., fortune completion, printer callbacks) so tests can inject scenarios without the full hardware stack.
- Consider adding small adapter classes/fakes for audio queue, printer, and sensor metadata to keep controller unit tests deterministic.

## Dependencies
- Story 011 (Testability Foundations) — shared seams, harness, and fixtures.
- Logging seam from Story 011 (already applied) — controller should reuse the same infrastructure.

## Testing Strategy
1. **Unit Tests (Host)** — cover trigger handling, state transitions, fortune/printer edge cases using fakes.
2. **Integration Smoke (Host)** — optional test that drives a simplified dependency graph through a full FAR→NEAR flow.
3. **Firmware Build** — ensure `pio run -e esp32dev` still succeeds; capture any manual runtime verification in the work log.
4. **Logging Validation** — confirm controller emits key events (state changes, warnings) through the log sink so existing RemoteDebug tooling continues to work.

## Implementation Plan (2025-11-05)
- **Controller Surface:** Maintain `DeathController` as the orchestration hub with explicit seams for audio, fortunes, printer, time, calibration, and randomness.
- **Dependency Bundle:** Adapters translate Arduino modules (audio queue, printer, manual calibration driver) into controller interfaces so host tests can fake them.
- **Fortune Flow:** Controller will own fortune generation and printing queue decisions; main sketch will forward controller intents only.
- **Manual Calibration:** Controller detects long-hold finger presses and drives the calibration stages through the adapter, keeping legacy helpers as fallbacks until hardware verified.
- **Testing Approach:** Host tests simulate FAR/NEAR, printer-unavailable, finger timeouts, cooldown expiry, and manual calibration hold; future additions will cover fortune path once migrated.

## Work Log

### 20251105-0950 — Context review and planning draft
- **Result:** Success; captured current `main.cpp` responsibilities, mapped seams, and documented controller/test strategy.
- **Notes:** Identified reliance on globals and `millis()`; no time seam yet.
- **Next:** Define adapter interfaces and `DeathController` skeleton.

### 20251105-1035 — Drafted controller interface and time seam
- **Result:** Added `infra::ITimeProvider` + Arduino adapter, and stubbed `death_controller.h` with dependency interfaces and actions.
- **Next:** Map Arduino glue responsibilities before implementation.

### 20251105-1055 — Mapped Arduino glue responsibilities
- **Result:** Documented how controller actions translate to audio, lights, servo, printer, finger sensor, and UART modules.
- **Next:** Start extracting state machine into controller.

### 20251105-1145 — Implemented controller scaffolding
- **Result:** Created `death_controller.cpp` with initial FAR/NEAR flow, audio intent queueing, and printer window logic.
- **Notes:** Manual calibration & fortune path still handled in legacy code.
- **Next:** Wire controller into `main.cpp`.

### 20251105-1240 — Added unit tests and Arduino adapter scaffolds
- **Result:** Introduced `tests/unit/test_death_controller` covering FAR/NEAR flow and printer edge cases; implemented audio/fortune/printer adapters.
- **Next:** Wire controller into runtime loop.

### 20251105-1320 — Wired controller scaffolds into Arduino runtime
- **Result:** Controller now receives UART/audio/finger events; actions translated into hardware calls while legacy logic still present for comparison.
- **Next:** Expand tests (snap timeout, cooldown) and begin consuming actions.

### 20251105-1455 — Added printer flow unit coverage
- **Result:** Tests cover printer ready/not-ready flows; kept red/green discipline.
- **Next:** Consume controller actions for LEDs/servo/audio fully.

### 20251105-1545 — Began applying controller actions in runtime
- **Result:** Controller actions now drive servo, LEDs, audio, and printer via adapters; legacy state machine bypassed when controller active.
- **Next:** Cover snap timeout & cooldown, then migrate manual calibration.

### 20251105-1610 — Added cooldown coverage & config override path
- **Result:** Controller tests exercise snap timeout and cooldown-to-idle with configurable timers; runtime honours controller actions.
- **Next:** Move manual calibration into controller.

### 20251105-1715 — Finger sensor non-responsive note
- **Result:** Documented suspected hardware damage and re-enabled legacy handler for redundancy.
- **Next:** Continue refactor assuming sensor may be retired.

### 20251105-1735 — Manual calibration trigger surfaced in controller
- **Result:** Controller detects long finger holds and issues calibration intents; unit tests asserted hold behaviour via logs.
- **Next:** Move full calibration sequence into controller.

### 20251105-1800 — Manual calibration sequence migrated
- **Result:** Controller now drives the entire manual calibration pipeline via the manual calibration adapter; legacy helpers removed; tests simulate the full cycle.
- **Next:** Migrate fortune flow helpers out of `main.cpp` and retire remaining globals.

### 20251105-1835 — Fortune loading refactor groundwork
- **Result:** Controller now owns fortune template loading using candidate lists gathered at startup; fortunes are generated/printed through controller actions while legacy helpers short-circuit for backward compatibility.
- **Next:** Delete the legacy fortune helpers and global flags once remaining references are replaced with controller calls.

### 20251105-2015 — Eliminated legacy state machine from Arduino loop
- **Result:** Success; pruned `DeathState` enum, legacy transition helpers, and fortune globals from `src/main.cpp`, routing loop updates entirely through `DeathController` while preserving breathing animation and printer fault indicators.
- **Notes:** Rewired UART command path to rely on controller handling and removed unused legacy helpers; ensured controller Idle state gates breathing jaw pulses.
- **Next:** Extend unit coverage to prove fortune flow survives missing audio assets.

### 20251105-2055 — Fortune flow regression test & controller fix
- **Result:** Success after red/green cycle; new unit test exposed lost print actions when fortune preamble audio is absent, prompting `DeathController::transitionTo` to retain immediate print requests. Verified by intentionally breaking and rerunning the test before restoring the fix.
- **Notes:** Harness updates asserted intermediate states and adjusted trigger timing to avoid debounce; documented red/green/break discipline.
- **Next:** Run firmware build to close regression task and capture results.

### 20251105-2120 — PlatformIO esp32dev build
- **Result:** Success; `pio run -e esp32dev` completed once `handleUARTCommand` forward declaration was restored, confirming firmware compiles after refactor.
- **Notes:** Toolchain bootstrap downloaded during build; observed existing third-party deprecation warnings unchanged.
- **Next:** Prepare summary of remaining cleanup and handoff considerations.

### 20251105-2145 — UART override decision
- **Result:** Alignment reached to retain Matter-driven state override commands (`[PLAY_WELCOME]`, `[SNAP_WITH_FINGER]`, etc.) alongside `FAR`/`NEAR` text triggers; they now drive `DeathController` directly and remain valuable for Home app debugging.
- **Notes:** No code change required; overrides still bypass normal guards by design, so flag as deliberate tooling.
- **Next:** Document override usage expectations once controller documentation pass begins.

### 20251105-2220 — Hardware sanity pass
- **Result:** Success; flashed current firmware to the ESP32 rig and confirmed FAR→NEAR flow, Matter-driven overrides, fortune printing, and idle breathing all behave as expected.
- **Notes:** Finger sensor remains physically non-responsive (see sensor issue log) but controller tolerates it; no new anomalies observed.
- **Next:** Ready for handoff; future follow-up limited to documentation polish or sensor hardware fix.
