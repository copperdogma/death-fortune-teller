# Story: Testability Foundations & Unit Suite

**Status**: To Do

---

## Problem Statement
The firmware ships without automated tests, and most modules reach directly into singletons, global state, and Arduino/ESP-IDF primitives. That tight coupling blocks fast feedback, makes regressions invisible until hardware trials, and slows every story that needs confidence. We need a deliberate testability refactor plus a sustainable way to run C/C++ tests locally and in CI.

## Related Requirement
[docs/spec.md §14 Acceptance Checklist (MVP)](../spec.md#14-acceptance-checklist-mvp) — Every listed behavior needs a repeatable verification path before hand-off.

## Alignment with Design
[docs/spec.md §3 State Machine](../spec.md#3-state-machine-runtime) — Core flow logic must stay authoritative, so we will introduce seams rather than rewriting behavior.

## Acceptance Criteria
- `pio test -e native` runs a deterministic host-side test suite that exercises fortune selection, state machine decisions, and config parsing without hardware.
- Firmware builds still succeed for `esp32dev` with `UNIT_TEST` toggles compiled out; production binary stays under current flash/RAM budgets.
- Fortune generation, skit selection, and config validation each have unit tests that cover success paths plus failure modes (missing files, corrupt JSON, empty directories).
- Hardware-adjacent modules expose thin interfaces (clock, RNG, filesystem, UART, LEDs, printer) so logic can be mocked in tests.
- README gains a contributor section with instructions for running tests, adding new ones, and the expectations for coverage going forward.
- A CI hook (GitHub Actions or `scripts/run_tests.sh`) runs `pio test -e native` and fails on regression.

## Tasks
- [x] **Audit & Prioritize** — Map each module by risk/complexity; pick first-wave targets (fortune flow, state machine, config).
- [x] **Create Test Harness** — Add `platformio.ini` `env:native` with Unity, stub Arduino shims, and a `tests/` scaffold.
- [ ] **Abstract Hardware Seams** — Introduce interfaces (e.g., `ITimeProvider`, `IAudioOutput`, `IFileSystem`) and refactor modules to depend on them.
- [ ] **Refactor Globals** — Replace global singletons with injected collaborators or service locators guarded by `UNIT_TEST`.
- [ ] **Author Baseline Tests** — Implement unit tests for fortune generation, directory selection, state machine transitions, and config validation.
- [ ] **Document Workflow** — Update README with run instructions, coding standards for tests, and a checklist for adding new modules to the suite.
- [ ] **Add CI/Script Hook** — Wire `pio test -e native` into automation so regressions fail fast.

## First-Wave Module Plans

### ConfigManager
- [x] Wrap SD card access behind `IFileSystem` and ensure constructor accepts dependency instead of singleton access.
- [ ] Extract parsing logic into pure functions (line tokenization, validation) for direct unit testing.
- [x] Add host tests covering happy path, invalid ranges, and secret masking behavior.
- [x] Provide fake filesystem fixture (in-memory config.txt) for tests and document setup in harness.

### FortuneGenerator
- [x] Inject `IFileSystem` + `IRandomSource` so fortunes load and random selection are deterministic under test.
- [x] Add tests for version/key validation, token replacement, fallback messages, and missing categories.
- [x] Supply fixture JSON files (`tests/fixtures/fortunes_*`) to exercise parsing/error paths.
- [x] Ensure logging routes through `ILogSink` so tests can assert error reporting without ESP logging.

### SkitSelector
- [ ] Convert `millis()` usage to `ITimeProvider` to remove direct Arduino dependency.
- [ ] Inject `IRandomSource` so selection randomness is controllable; add tests verifying no-repeat pool behavior and weight calculations.
- [ ] Cover edge cases: single skit, empty list, and selection pool size capping.
- [ ] Document relationship with forthcoming state coordinator to avoid double counting randomness.

## Execution Roadmap
1. **Harness Bootstrap**
   - Create `env:native` in `platformio.ini`, add Arduino shim scaffolding, and commit initial `tests/unit_host_entry.cpp`.
   - Validate `pio test -e native` compiles a smoke test that only asserts harness wiring.
2. **ConfigManager Refactor + Tests**
   - Introduce filesystem seam adapters, migrate singleton access to injection, and land unit tests using fixture configs.
   - Update production code to fetch through new interfaces while keeping runtime behavior unchanged.
3. **FortuneGenerator Refactor + Tests**
   - Add RNG + filesystem injection, write positive/negative tests, and ensure log messages reach sink abstraction.
4. **SkitSelector Refactor + Tests**
   - Swap `millis()`/`random()` usage for seam interfaces and verify weighted selection logic via host tests.
5. **State Coordinator Extraction (Prereq Work)**
   - Outline approach for peeling state machine logic out of `main.cpp`; schedule follow-up story if scope inflates.
6. **Documentation & Contributor Guidance**
   - Update README/testing section and add brief `tests/README.md` covering shims, fixtures, and how to add new test suites.
7. **Automation Hook**
   - Implement `scripts/run_tests.sh` and wire into GitHub Actions (or local CI) to run `pio test -e native`.
8. **Stretch: Additional Modules / HIL**
   - Evaluate extending tests to printer/logging seamed modules or on-device `pio test -e esp32dev` smoke checks.

## Technical Implementation Details

### Phase 0 — Assessment & Guard Rails
- Inventory current globals (`main.cpp` owns almost everything) and note which components already have header separations we can leverage.
- Freeze existing behavior by logging current runtime traces (audio queues, state transitions) to compare after refactors.

### Phase 1 — Test Harness & Tooling
- Add a `native` PlatformIO environment compiling against `unity` with `-DUNIT_TEST` to expose alternate constructors and seam implementations.
- Provide minimal Arduino compatibility shims (e.g., `millis`, `String`, logging macros) for host builds; keep them inside `tests/support/`.
- Create `tests/unit/` suites grouped by module and a `tests/helpers/` folder for reusable fakes.

#### Proposed Core Interfaces (Seams)
- **`ITimeProvider`** — Exposes `millis()`, `delay()`, and timestamp helpers; production adapter wraps Arduino timing, while tests inject deterministic counters and manual advancement.
- **`IRandomSource`** — Supplies `uint32_t next()` plus range helpers; adapters bridge to `esp_random`/`random` and allow seeding predictable generators (e.g., `std::mt19937`) during tests.
- **`IFileSystem` / `IFileHandle`** — Mirrors the subset of `fs::FS` used (`open`, `exists`, `read`, `readString`, directory iteration); production wraps `SD_MMC`, tests swap in in-memory files or host filesystem fixtures.
- **`ILogSink`** — Abstracts LoggingManager usage for modules that only need to emit logs; production sink routes to existing manager, test sink can collect messages for assertions.
- **`ISerialChannel` / `IUART`** — (future) Encapsulates printer/UART writes so state-machine logic can be exercised without ESP32 drivers; start by defining interface even if mocks land later.
- **`IAsyncScheduler`** (optional stretch) — Represents delayed callbacks/state machine timers, enabling host-driven advancement instead of real-time waits once coordinator extraction happens.

#### Native Test Harness Layout
- **PlatformIO `env:native`**
  - `platform = native`, `build_flags = -DUNIT_TEST -std=c++20`, link with `unity` and `pthread` as needed.
  - `src_filter = +<../tests/unit_host_entry.cpp>` to avoid building firmware entry point; expose production sources via `lib_ignore`/`lib_ldf_mode` so only seam-friendly files compile.
  - Adds `build_src_filter = +<../src>` once modules are made host-safe; initially limit to refactored targets (fortune/config/state coordinator).
- **Directory Structure**
  - `tests/unit/` — Unity test suites (`test_fortune_generator.cpp`, `test_config_manager.cpp`, etc.).
  - `tests/support/` — Arduino shim headers (`arduino_shim.h/.cpp` providing `millis`, `delay`, `String`, `HardwareSerial` stubs), fake filesystem, fake RNG/time providers, log sinks.
  - `tests/fixtures/` — JSON/config/audio fixture files for parsing tests; host builds load via native filesystem wrapper.
- **Entry Points**
  - `tests/unit_host_entry.cpp` exposes `int main()` that initializes shims, registers Unity tests, and runs them (required because firmware has `setup()/loop()`).
- **Automation**
  - `scripts/run_tests.sh` invokes `pio test -e native` and surfaces failures with non-zero exit code.
  - Future CI step reuses script to keep local + remote parity.
- **Guard Rails**
  - Wrap Arduino-only includes behind `#ifndef UNIT_TEST` or provide shim equivalents with same API surface.
  - Document per-module opt-in via `UNIT_TEST` constructors to avoid leaking test-only hooks into production builds.

### Phase 2 — Dependency Injection Seams
- Introduce lightweight C++ interfaces: `ITimer`, `IRandomSource`, `IStorage`, `IAudioQueue`, `ISerialLogger`, etc.
- Wrap ESP-IDF/Arduino constructs behind concrete adapters in `src/infra/` (production) and mocks/fakes in `tests/support/`.
- Refactor modules (e.g., `FortuneGenerator`, `SkitSelector`, `FingerSensor`) to accept these interfaces via constructors or setters.
- Gate Arduino-only code with `#ifndef UNIT_TEST` when direct seams aren’t possible (e.g., `hw_timer_t *` usage).

### Phase 3 — Baseline Unit Coverage
- Fortune flow: test template substitution, failure modes when JSON tokens are missing, and randomness boundaries via injected RNG.
- Skit selection: verify directory discovery, exclusion of non-WAV files, and debounce/no-repeat rules using fake filesystem content.
- State machine: extract decision logic into a pure coordinator class so we can simulate triggers, timers, and printer availability.
- Config manager: validate fallback defaults, error paths, and reload behavior.
- Logging/remote debug: add smoke tests ensuring command parsing works without serial dependencies (using string streams).

### Phase 4 — Integration & Regression Hooks
- Build a “logic integration” test that runs a near-trigger flow end-to-end using fake collaborators, confirming fortune printing requests and cooldown timing.
- Add optional hardware smoke tests under `tests/hil/` that can run on-device (`pio test -e esp32dev`) when hardware is attached.
- Provide coverage targets (e.g., 70% statements for pure logic modules) and document how to run `gcovr`/`llvm-cov` when tooling is available.

### Documentation & Onboarding
- Extend README with: testing philosophy, quickstart commands (`pio test -e native`, `pio test -e esp32dev`), guidance for writing fakes, and CI expectations.
- Add a `CONTRIBUTING-testing.md` or README subsection listing required seams for new hardware modules.
- Capture known gaps (e.g., Bluetooth stack still mocked) in a “Future Work” note so future stories can expand coverage.

## Dependencies
- Story 003a (State Machine Implementation) — Need final state definitions before locking tests.
- Story 005a (Serial Console & Diagnostics) — Shares command parsing seams; coordinate refactors.
- Story 004b (Non-Blocking Runtime Refactor) — Testability work should align with event loop changes.

## Testing Strategy
1. **Unit Tests (Host)** — Run on developer machines/CI via `pio test -e native` with mocks.
2. **Logic Integration Tests (Host)** — Simulate full flows with fake peripherals to guard regression.
3. **Hardware-in-the-Loop (Optional)** — On physical ESP32 via `pio test -e esp32dev` for smoke checks.
4. **Static Checks** — Encourage `clang-tidy`/`cppcheck` integration once seams exist.
5. **Regression Checklist** — Document manual hardware checks that remain until coverage expands.

## Notes
- Keep production behavior identical while introducing seams; prefer adapter classes over widespread `#ifdef`.
- Start with the modules that already have minimal dependencies to win early confidence before tackling Bluetooth/SDIO heavy code.
- Align new interfaces with Arduino-style naming to keep onboarding friction low.

## Work Log

### 20251105-1200 — Initialized Story Log & Checklist Review
- **Result:** Success; verified `## Tasks` section exists with actionable checkboxes, no edits required.
- **Notes:** Added `## Work Log` section to establish running record per Build Story command.
- **Next:** Begin Phase 0 module audit and capture findings here.

### 20251105-1235 — Completed Phase 0 source audit
- **Result:** Success; mapped key modules and their testability blockers.
- **Notes:** 
  - `main.cpp` centralizes the state machine, CLI, and all hardware lifecycles via globals; extracting a pure coordinator is top priority to unlock host tests.
  - `ConfigManager`, `FortuneGenerator`, and `SkitSelector` are logic-heavy with limited hardware touch (SD + randomness); they can be first refactor targets by injecting filesystem/RNG seams.
  - Hardware-bound classes (`AudioPlayer`, `BluetoothController`, `FingerSensor`, `ThermalPrinter`, `RemoteDebugManager`) depend on ESP32/Arduino APIs and FreeRTOS primitives; they will require adapter interfaces and are candidates for later phases.
  - `LoggingManager` and other singletons (`OTAManager`, `ConfigManager`) appear throughout; will need an injectable facade or service locator guarded by `UNIT_TEST`.
  - Identified potential seam inventory: clock (`millis`), random source, filesystem, UART, WiFi/Bluetooth, logging sink.
- **Next:** Draft interface plan for timers, RNG, filesystem, and logging to feed Phase 1 test harness design.

### 20251105-1315 — Defined core seam interfaces & updated task status
- **Result:** Success; documented proposed interfaces section and marked Audit task complete.
- **Notes:** Added `ITimeProvider`, `IRandomSource`, `IFileSystem`, `ILogSink`, and future `IUART`/`IAsyncScheduler` seams under Phase 1 to guide adapter design; these align with modules identified in audit. Marked the **Audit & Prioritize** task as done to reflect completed Phase 0.
- **Next:** Outline test harness architecture (native PlatformIO env, Arduino shims layout) and record required files/scripts before implementation.

### 20251105-1420 — Sketched native test harness architecture
- **Result:** Success; added detailed plan for `env:native`, directory layout, entry points, and automation hooks.
- **Notes:** Specified use of `unity`, shim directories, fixture storage, and `scripts/run_tests.sh` parity with CI. Highlighted guard rails for Arduino-only code to keep host build clean. Task remains open until environment and files exist.
- **Next:** Identify first modules for refactor-in-place (ConfigManager, FortuneGenerator, SkitSelector) and list required seams/tests for each.

### 20251105-1505 — Outlined first-wave refactor checklists
- **Result:** Success; added module-specific action lists for ConfigManager, FortuneGenerator, and SkitSelector.
- **Notes:** Checklists emphasize seam injection, deterministic RNG/time handling, and targeted unit tests with fixtures. These will guide implementation sequencing once harness is ready.
- **Next:** Break down story-level tasks into actionable milestones (e.g., harness bootstrap, ConfigManager refactor) and prepare execution order.

### 20251105-1545 — Drafted execution roadmap
- **Result:** Success; sequenced harness bootstrap, module refactors, documentation, and CI follow-up.
- **Notes:** Roadmap clarifies dependencies (harness before module refactors) and highlights stretch goals (state coordinator extraction, HIL smoke tests). Provides a hand-off friendly flow for future implementers.
- **Next:** Proceed to harness implementation when ready or await further directives.

### 20251105-1625 — Bootstrapped native test harness skeleton
- **Result:** Success; added `env:native` to `platformio.ini`, created `tests/unit/test_sanity/test_main.cpp`, and verified `pio test -e native` passes.
- **Notes:** Configured `test_dir = tests`, disabled project source builds for now, and resolved a stale `.pio/build/native` directory to allow Unity runner to initialize. Harness currently runs a simple sanity test; Arduino shims and module-specific suites still outstanding before marking **Create Test Harness** complete.
- **Next:** Stand up Arduino shim scaffolding (`tests/support/arduino_shim.*`) and migrate first module (`ConfigManager`) to use injected filesystem for host tests.

### 20251105-1705 — Added filesystem seam interfaces
- **Result:** Success; created `src/infra/filesystem.h` and `src/infra/sd_mmc_filesystem.{h,cpp}` and confirmed `pio run -e esp32dev` builds cleanly.
- **Notes:** Implemented `IFile`/`IFileSystem` abstractions with SD-MMC adapter to prep ConfigManager refactor. Adjusted interface signatures after initial const mismatch during build. No runtime wiring yet; injection work remains.
- **Next:** Update `ConfigManager` to accept an `IFileSystem` dependency (with production default) and craft fake filesystem for host tests.

### 20251105-1745 — Refactored ConfigManager for injectable filesystem
- **Result:** Success; `ConfigManager` now accepts `infra::IFileSystem` via `setFileSystem`, uses SD adapter only outside `UNIT_TEST`, and production build still succeeds.
- **Notes:** Added guard to require explicit filesystem during tests to prevent silent SD_MMC coupling. Next hurdle is providing Arduino/Logging shims so host builds can compile ConfigManager without the ESP-IDF stack.
- **Next:** Design `UNIT_TEST` stubs for `Arduino.h` (String, FILE_READ) and `LoggingManager` to unblock fake filesystem tests.

### 20251105-1845 — Brought ConfigManager under native test harness
- **Result:** Success; implemented Arduino/logging stubs, fake filesystem, and `unit/test_config_manager` suite. `pio test -e native` now runs both sanity and config manager cases.
- **Notes:** Created shim `String` wrapper and minimal `FS.h` to satisfy Arduino dependencies; caught build issues (const correctness, missing `constrain`) while wiring. Marked harness task complete and checked ConfigManager checklist items for seams, fixtures, and baseline testing.
- **Next:** Extend ConfigManager tests for failure paths (invalid ranges, missing keys) and start planning FortuneGenerator refactor.

### 20251105-1935 — Added regression test for config reload defaults
- **Result:** Success; wrote `test_reload_clears_missing_keys` (red first), then cleared `m_config` on reload so missing keys fall back to defaults. Native test suite now covers reload behaviour.
- **Notes:** Demonstrated red/green cycle (`pio test -e native` failed before fix, passes after). Ensures successive loads can't retain stale entries when SD config changes mid-run.
- **Next:** Expand ConfigManager tests to cover invalid numeric bounds and continue prepping FortuneGenerator seam injection.

### 20251105-2010 — Locked config defaults to spec expectations
- **Result:** Success; added tests for spec-aligned defaults (cap threshold, fortunes path) and invalid servo bounds. Tests failed (cap default 0.1) until `getCapThreshold` and `getFortunesJson` were corrected; full suite now green.
- **Notes:** Red/green confirmed via `pio test -e native`. Config defaults now match runtime constants used elsewhere (e.g., `/printer/fortunes_littlekid.json`).
- **Next:** Extend numeric validation coverage (finger timings, LED bounds) and pivot to FortuneGenerator seam design.

### 20251105-2040 — Added timing/LED bound regression tests
- **Result:** Success; new unit cases cover finger timing, snap delays, cooldown, and mouth LED bounds. Existing implementation already returned defaults, so tests passed on first run (green).
- **Notes:** No production changes required; reaffirms clamping logic is in place before deeper refactors.
- **Next:** Begin FortuneGenerator seam extraction (filesystem + RNG) so we can craft deterministic parsing tests.

### 20251105-2120 — FortuneGenerator seams + deterministic test
- **Result:** Success; introduced filesystem/random interfaces, Arduino adapter, and fake implementations. Added unit test that injects fake filesystem + RNG to validate parsing and deterministic fortune output. Tests failed red until seam refactor was complete; now `pio test -e native` and `pio run -e esp32dev` both pass (deprecation warnings noted from ArduinoJson).
- **Notes:** Created `infra/random_source` abstraction, SD helper reuse, and updated String shim for substring support. FortuneGenerator still needs negative-path coverage and fixture-driven tests.
- **Next:** Add failure-mode tests (missing tokens, empty wordlists) and consider migrating to ArduinoJson v7 APIs to eliminate deprecation warnings.

### 20251105-2145 — FortuneGenerator failure-path coverage
- **Result:** Success; added unit tests for missing `version` and absent token wordlists. Existing validation logic behaved correctly (green). Full host suite remains green.
- **Notes:** Tests currently inline JSON strings; fixtures remain future enhancement. ArduinoJson deprecation warnings still present.
- **Next:** Migrate inline JSON fixtures to dedicated files and audit logging seam requirements.

### 20251105-2210 — Moved fortune fixtures to shared files
- **Result:** Success; created `tests/fixtures/fortune_*.json`, added `fixture_loader` helper, and updated fortune generator tests to consume them. Tests failed to link until loader became header-only; now `pio test -e native` and `pio run -e esp32dev` pass.
- **Notes:** Fixtures simplify future negative-case additions; logging seam still outstanding. ArduinoJson still emits deprecation warnings, queued for later cleanup.
- **Next:** Replace direct LoggingManager usage in fortune generator with `ILogSink` adapter and consider upgrading JSON usage to silence warnings.

### 20251105-2250 — Introduced log sink seam for FortuneGenerator
- **Result:** Success; added `infra/log_sink`, default LoggingManager adapter, and fake log sink for tests. Verified error paths emit expected logs before marking task complete.
- **Notes:** Updated native build filter to compile the sink, plus tuned tests to assert on error-level entries. ArduinoJson deprecation warnings remain pending.
- **Next:** Evaluate sharing the log sink across additional modules and plan ArduinoJson API clean-up.

### 20251105-2315 — Logging adapter wired into firmware entry point
- **Result:** Success; `main.cpp` now registers the shared log sink so runtime logs flow through the same interface used in tests. Added compiler guards around `DynamicJsonDocument` to silence host warnings while keeping ESP32 compatibility.
- **Notes:** Native + ESP32 builds remain green; ArduinoJson upgrade still outstanding but warnings suppressed for now.
- **Next:** Explore migrating remaining modules to the shared log sink before tackling the JSON API bump.

### 20251105-2335 — ConfigManager now logs via shared seam
- **Result:** Refactored ConfigManager to use `infra::log_sink`, added injectable sink for tests, and introduced a warning assertion for invalid speaker volume. RemoteDebug now emits via the same seam as well. All native/ESP32 builds pass.
- **Notes:** Config logging no longer depends on `logging_stub`; tests reset the fake sink in `setUp()` to avoid cross-test bleed. RemoteDebug continues to access `LoggingManager` for log buffers but emits through the seam.
- **Next:** Continue rolling the seam across other modules or move to SkitSelector refactor.

### 20251106-0005 — Extended log seam to SD/UART subsystems
- **Result:** Updated `sd_card_manager` and `uart_controller` to use `infra::emitLog`, eliminating direct macro usage. Firmware & host tests still pass.
- **Notes:** Modules now respect whichever log sink is registered (LoggingManager in runtime, fake sink in tests). No additional tests yet; future work could assert UART warning paths.
- **Next:** Evaluate remaining modules (e.g., main loop helpers) and proceed with ArduinoJson API upgrade.
