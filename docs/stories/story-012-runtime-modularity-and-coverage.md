# Story: Runtime Modularity & Coverage Roadmap

> **Reverted — 2025-11-06:** All work captured below was intentionally rolled back after we decided the runtime refactor/testing scope exceeded the needs of this project. See Story 014 for the new, lighter modular-runtime plan; this document is retained only for historical reference.

**Status**: Reverted

---

## Problem Statement
`src/main.cpp` has grown into a 1,500+ line monolith that wires every subsystem through globals and Arduino singletons. Critical services such as audio buffering, Bluetooth recovery, WiFi/OTA management, and SD-based content selection embed state machines that we cannot exercise outside the device. This design blocks reuse for future props, hides regressions until hardware smoke tests, and makes the existing unit harness cap out at ConfigManager/FortuneGenerator/DeathController coverage.

## Related Requirements
- [docs/spec.md §3 State Machine Runtime](../spec.md#3-state-machine-runtime) — Requires predictable sequencing and observability across modes.
- [docs/stories/story-011-testability-foundations.md](story-011-testability-foundations.md) — Establishes the testing harness; this story extends coverage to runtime subsystems.
- [docs/stories/story-004b-non-blocking-runtime.md](story-004b-non-blocking-runtime.md) — Calls for modular host-loop code; this story provides prerequisite structure.

## Alignment with Design
By introducing explicit module boundaries and dependency injection, we retain the current Arduino runtime while enabling host-side validation. The same seams will let us port behavior to new animatronics that need different I/O stacks without rewriting the state logic or audio/printer orchestration.

## Acceptance Criteria
- A new `AppController` (or equivalent) owns setup/loop orchestration; `src/main.cpp` shrinks to bootstrapping that wires dependencies.
- Runtime collaborators (audio, Bluetooth, WiFi, OTA, SD directory selection, printer status, LED calibration) implement lightweight interfaces that can be substituted in tests.
- `pio test -e native` gains deterministic unit tests for:
  - Audio queue/ring-buffer edge cases (mute, wrap-around, start/end callbacks).
  - Bluetooth reconnection/pause timers and OTA deferral behavior.
  - SD directory selection weighting and cache invalidation.
  - Manual calibration adapter sequencing (blink → wait → calibrate → completion).
- Adapter layer no longer caches state implicitly; tests assert expectations using fakes/mocks.
- README (or CONTRIBUTING) documents how to run the expanded suite and outlines expectations for adding coverage when new runtime logic lands.
- New story tasks land checks or static asserts preventing regressions in dependency wiring (e.g., link-time enforcement that `AppController` is constructed in one place).

## Tasks
- [x] **Decompose Entry Point**
  - [x] Extract `SystemContext`/`AppController` that encapsulates singletons and loop phases.
  - [x] Migrate remaining CLI/debug handlers (`legacyProcessCLI`, Serial printers, validation helpers) out of `AppController` into dedicated modules with unit coverage; rely solely on `CliCommandRouter` and shared UNIT_TEST stubs.
- [ ] **Interface Extraction**
  - [x] Define `IBluetoothLink`, `INetworkLink`, `IOTAUpdater` seams and wire runtime to use interface pointers with RAII ownership.
  - [ ] Define `IAudioOutput`, `IContentSelector`, and `IPrinterStatus` seams.
- [ ] **Runtime Loop Modularization**
  - [x] Split `AppController::setup` responsibilities (SD card summary, fortune candidate gathering, printer fault handling) into injectable services so wiring stays declarative.
  - [ ] Refactor `AppController::loop` to delegate UART, breathing animation, and CLI polling to focused helpers that can be host-tested.
  - [ ] Add host-side coverage for `AppController` orchestration using stubbed services to confirm sequencing (SD mount → config load → controller init).
- [ ] **Audio Buffer Refactor**
  - [x] Pull ring-buffer and event signaling logic into a pure helper (`CircularAudioBuffer`) with unit tests for wrap-around and mute paths.
- [ ] **Bluetooth/WiFi/OTA State Tests**
  - [x] Add adapter-level host tests covering Bluetooth retry toggles, WiFi callbacks, and OTA event plumbing via stubs.
  - [ ] Introduce injectable clock/backoff configuration for `BluetoothController` and cover retry, pause, and deferred resume sequences.
  - [ ] Provide host-side tests for WiFi reconnect handling and OTA enable/disable flows using deterministic fakes.
- [x] **Content Selection Coverage**
  - [x] Update AudioDirectorySelector and SkitSelector to accept filesystem + RNG abstractions and add host tests verifying fairness, cache refresh, and empty-directory handling.
- [ ] **Adapter Verification**
  - [x] Provide fake LightController/ThermalPrinter/AudioPlayer implementations to validate ManualCalibrationAdapter, FortuneServiceAdapter, and AudioPlannerAdapter behaviors.
- [ ] **Documentation & Enforcement**
  - [ ] Update README/testing docs with new modules, commands, and contribution expectations.
  - [ ] Add CI or script updates ensuring the new tests run by default.

## Milestones
1. **Bootstrap** — `AppController` scaffold lands; main.cpp reduced to context wiring.
2. **Audio & Content Seams** — Audio buffer helper plus selector abstractions covered by host tests.
3. **Connectivity Seams** — Bluetooth/WiFi/OTA interfaces refactored with deterministic tests.
4. **Adapter Assurance** — Manual calibration and fortune/printing adapters validated and documented.

## Risks & Mitigations
- **Risk**: Arduino build size may grow due to indirection.  
  **Mitigation**: Use header-only interfaces with `constexpr` hints; monitor `pio run -t size` during refactor.
- **Risk**: Splitting `main.cpp` could destabilize runtime ordering.  
  **Mitigation**: Add integration smoke tests (possibly under `tests/unit/test_death_controller_integration`) and instrument logging to confirm boot sequence.
- **Risk**: Host tests may require additional shims.  
  **Mitigation**: Reuse existing `tests/support` Arduino headers; extend only when a new abstraction is introduced.

## Definition of Done
- Story tasks complete with code merged on mainline.
- New tests pass locally and in automation; coverage for targeted modules documented.
- README/stories updated to reflect the modular architecture and expanded test expectations.
- Follow-up stories (if needed) logged for any deferred modules discovered during implementation.

## Implementation Plan
### Core Architecture
- **SystemContext**: Responsible for constructing hardware-facing implementations (LightController, ServoController, SDCardManager, AudioPlayer, BluetoothController, UARTController, ThermalPrinter, FingerSensor, FortuneGenerator, WiFiManager, OTAManager, RemoteDebugManager, SkitSelector, AudioDirectorySelector, adapters). Exposes references plus lifecycle helpers (`init()`, `shutdown()` where applicable).
- **AppController**: Owns high-level phases (`setup()`, `loop()`, `handleCommand()`), consumes `SystemContext` and injected abstractions (`ITimeProvider`, `IRandomSource`, `ILogSink`). Coordinates configuration load, dependency wiring, Matter UART bridge, CLI dispatch, and DeathController state updates.
- **Arduino Main Glue**: `setup()` constructs `SystemContext`, passes references into `AppController`, registers hardware callbacks, and delegates. `loop()` defers to `AppController::loop()` to keep Arduino entry points minimal.

### Interface Boundaries
- Define abstract interfaces for:
  - `IAudioOutput` (queue audio, query playing, register callbacks).
  - `IBluetoothLink` (initialize, update, pause/resume, connection state callbacks).
  - `INetworkLink` (begin, update, status hooks).
  - `IOTAUpdater` (begin, update, state queries).
  - `IContentSelector` (select/reset clips, refresh catalog).
  - `IPrinterStatus` (already present; ensure injection path).
  - `IManualCalibrationDriver` (present; relocate under new module namespace).
- Provide Arduino implementations living under `src/runtime/` or similar, constructed by `SystemContext`.

### Sequencing & Loop
- `AppController::setup()` order:
  1. Initialize logging sink + diagnostics.
  2. Mount SD card (via context helper).
  3. Load configuration (config manager).
  4. Initialize audio/Bluetooth/WiFi/OTA subsystems with config data.
  5. Wire DeathController dependencies and prime initial state.
  6. Optionally schedule initialization audio / CLI prompts.
- `AppController::loop()`:
  - Update sensor readings, WiFi/OTA/Bluetooth state, audio player.
  - Drive DeathController update with latest finger readout + timers.
  - Process pending actions (mouth, LEDs, printer, remote debug) via injected interfaces.
  - Handle CLI command queue if available.

### Testing Strategy
- Host-only tests instantiate `AppController` with mocked `SystemContext`/interfaces, verifying:
  - Bootstrapping sequences configuration loads before enabling subsystems.
  - Loop reacts correctly to mocked DeathController actions (e.g., audio queue, printer commands).
  - Error paths (missing SD, failed config) propagate logs and safe states.
- Unit tests for interface implementations:
  - `CircularAudioBuffer` (ring buffer) with deterministic data pushes/pops.
  - `AudioDirectorySelector`/`SkitSelector` with fake filesystem + RNG verifying rotation.
  - `BluetoothController` update loop using fake clock verifying retry/OTA defer logic.

### Migration Steps
1. Introduce new files: `app_controller.h/cpp`, `system_context.h/cpp`, `runtime_interfaces.h`.
2. Move existing global helper functions into appropriate classes (e.g., CLI commands into `AppController` private methods).
3. Provide transitional adapters so existing modules (DeathController adapters, etc.) compile during refactor.
4. Trim `src/main.cpp` to instantiate context, controller, and forward Arduino lifecycle.
5. Update build to include new files; ensure tests compile.

### Dependency Notes
- Execute **Decompose Entry Point** first to establish `SystemContext`/`AppController` scaffolding; subsequent interface extraction work should target the seams those classes expose.
- While `CircularAudioBuffer` and selector refactors can progress in parallel, Bluetooth/WiFi/OTA work relies on the new interface definitions—schedule after interface contracts stabilize.
- Documentation/CI updates should occur after new tests are in place to avoid stale guidance.

### CLI Service Plan
- Implement a reusable `CliService` responsible for buffering serial input, tokenizing commands, and dispatching handlers.
- Construct `CliService` inside `SystemContext`, passing handler registration callbacks supplied by `AppController`.
- Have `AppController::loop()` poll `CliService` for pending commands to maintain deterministic loop timing.
- Document the `CliService` API so future hardware apps can adopt the same component without duplication.

### Detailed Plan — Interface Extraction
1. **Interface Definitions**
   - `IAudioOutput`: `void queueAudio(const std::string&)`, `bool isPlaying() const`, `void setPlaybackCallbacks(PlaybackStart, PlaybackEnd)`.
   - `IBluetoothLink`: lifecycle (`void begin(const BluetoothConfig&)`, `void update(uint32_t nowMs)`), media control (`void notifyAudioActive(bool)`), OTA hooks (`void pauseForOta()`, `void resumeAfterOta()`), state query (`bool isConnected() const`).
   - `INetworkLink`: `bool begin(const NetworkConfig&)`, `void update(uint32_t nowMs)`, connection callbacks registration.
   - `IOTAUpdater`: `bool begin(const OTAConfig&)`, `void update(uint32_t nowMs)`, observers for start/end/progress/error.
   - `IContentSelector`: `String selectClip(const char* directory, const char* description)`, `void resetStats(const char* directory)`, `void refresh(const char* directory)`.
2. **Adapter Strategy**
   - Wrap existing `AudioPlayer`, `BluetoothController`, `WiFiManager`, `OTAManager`, `AudioDirectorySelector` with thin adapter classes implementing the new interfaces without altering core logic.
   - Expose configuration structs (`BluetoothConfig`, `NetworkConfig`, etc.) derived from `ConfigManager`.
3. **Injection Points**
   - `SystemContext` constructs concrete implementations and stores them behind interface pointers.
   - `AppController` receives interface references in its constructor, enabling substitution during tests.
4. **Transitional Steps**
   - Maintain backward compatibility for modules still using concrete classes (e.g., DeathController adapters) by providing accessors returning both interface and concrete pointer during migration.
   - Update unit tests to use mock/fake implementations for newly abstracted interfaces; leverage existing fake log sink/time provider patterns.

### Detailed Plan — Audio Buffer Refactor
1. **Analysis**
   - Map responsibilities inside `AudioPlayer` (`fillBuffer`, `provideAudioFrames`, buffer indices).
   - Identify ISR vs. task contexts to ensure new helper remains IRQ-safe.
2. **Helper Design**
   - Create `CircularAudioBuffer` class encapsulating buffer array, read/write pointers, counters, and start/end markers.
   - Provide methods: `size_t write(const uint8_t* data, size_t bytes)`, `size_t read(uint8_t* dest, size_t bytes)`, `void reset()`, `bool empty() const`, plus event triggers for thresholds.
   - Parameterize buffer size for host tests.
3. **Integration**
   - Refactor `AudioPlayer` to delegate buffer operations to `CircularAudioBuffer`.
   - Maintain existing FreeRTOS critical sections; ensure helper exposes ISR-safe API or is wrapped appropriately.
4. **Testing**
   - Add host-side tests verifying:
     - Wrap-around correctness with varied read/write batch sizes.
     - Behavior when requests exceed available data (zero-fill logic).
     - Start/end event markers trigger at correct offsets.
     - Mute handling zero-fills output while leaving buffer state intact.
5. **Performance Validation**
   - Run `pio run -e esp32dev -t size` to confirm no significant increase.
   - Exercise on hardware (manual step) to ensure audio playback unaffected; document requirement in story follow-up.

### Detailed Plan — Bluetooth/WiFi/OTA State Tests
1. **Clock Abstraction**
   - Extend `infra::ITimeProvider` or introduce `ITickSource` to supply deterministic `nowMs()` for host tests.
   - Update Bluetooth/WiFi/OTA controllers to accept injected time source instead of calling `millis()` directly.
2. **Bluetooth Controller Tests**
   - Mock `IBluetoothDriver` interactions (connection state callbacks, media start requests).
   - Cover scenarios: automatic retry scheduling, OTA pause/resume deferral, media start deadline handling, manual disable.
3. **WiFi Manager Tests**
   - Simulate connection attempts and statuses via stubbed `WiFi` shim; assert retry interval respects configuration and resets after success.
   - Validate disconnection callback firing and attempt counter reset after max retries.
4. **OTA Manager Tests**
   - Introduce test double for `ArduinoOTA` callbacks capturing start/progress/end/error events.
   - Verify password gating, OTA disabled state when password missing, progress logging cadence thresholds.
5. **Test Harness Updates**
   - Add new fake modules under `tests/support` (WiFi stub, OTA stub, Bluetooth stub) with optional compile guards.
   - Ensure `pio test -e native` registers new suites and runs them deterministically.

### Detailed Plan — Content Selection Coverage
1. **Abstraction Work**
   - Modify `AudioDirectorySelector` to accept an `infra::IFileSystem` reference plus `IRandomSource` at construction.
   - Introduce `ITimeProvider` usage instead of direct `millis()` calls for weighting.
2. **Test Fixtures**
   - Reuse `FakeFileSystem` to populate directory structures via fixture JSON or builder helpers.
   - Create deterministic `FakeRandomSource` capable of returning scripted sequences to cover tie-breakers.
3. **Test Cases**
   - Validate weighting logic (recently played clips receive lower priority) across multiple plays.
   - Ensure empty directory returns warning and empty string without crashing.
   - Confirm cache invalidation occurs when files are removed between refreshes.
   - Verify max pool size enforcement and avoidance of immediate repeats when alternatives exist.
4. **Skit Selector Alignment**
   - Apply same abstractions to `SkitSelector`; ensure host tests align with audio selector coverage (single-skit, multi-skit, fairness).
5. **Regression Guard**
   - Add log sink assertions verifying warnings emit for missing directories, helping catch filesystem regressions early.

### Detailed Plan — Adapter Verification
1. **Scope**
   - Target `AudioPlannerAdapter`, `FortuneServiceAdapter`, `PrinterStatusAdapter`, and `ManualCalibrationAdapter`.
   - Ensure adapters operate purely as bridges between interfaces and concrete implementations.
2. **Fakes & Mocks**
   - Create lightweight fake `AudioPlayer`, `FortuneGenerator`, `ThermalPrinter`, and `LightController` classes for host tests, exposing counters and flags.
   - Reuse/extend existing test doubles in `tests/support`.
3. **Test Coverage**
   - Verify `AudioPlannerAdapter` caches selections and respects `hasAvailableClip` semantics even when `selectClip` returns empty.
   - Confirm `FortuneServiceAdapter` avoids reloading fortunes when already loaded and logs warning on failure.
   - Ensure `PrinterStatusAdapter` simply forwards readiness state (guard against future expansions adding side effects).
   - Exercise `ManualCalibrationAdapter` blink sequencing, mouth LED control, and sensor calibration invocation order.
4. **Integration Hooks**
   - Update `AppController` to use adapters via interfaces; host tests ensure adapters are triggered when DeathController actions are emitted.
5. **Regression Checks**
   - Add log assertions where adapters emit warnings (e.g., fortune load failure) to guard behavior.

### Detailed Plan — Documentation & Enforcement
1. **Docs Update**
   - Add new section to `README.md` (or `docs/CONTRIBUTING.md` if present) describing runtime architecture, how to run `pio test -e native`, and expectations for adding module-specific tests.
   - Include quickstart instructions for injecting mock interfaces when developing new features.
2. **Story/Changelog**
   - Update `docs/stories.md` status/progress once major checkpoints are completed.
   - Append summary to `CHANGELOG.md` under upcoming release (if maintained).
3. **CI Integration**
   - Update existing CI workflow or add script in `scripts/` to execute `pio test -e native`; ensure environment variables for PlatformIO caches documented.
   - Add optional `pio run -e esp32dev -t size` check to guard flash usage (warn threshold).
4. **Enforcement**
   - Consider adding a lightweight lint or static assert verifying only one `AppController` instance is linked (e.g., using `inline constexpr` guard).
   - Document policy that new runtime logic must depend on interfaces, not Arduino globals; include checklist in PR template if available.

### Decisions
- **CLI Service Ownership:** Introduce a dedicated `CliService` shared across hardware apps; `AppController` consumes it via dependency injection.
- **Configuration Access:** Prefer dedicated config structs when they improve clarity or testability; otherwise call `ConfigManager` getters directly. Capture decision per module during implementation.
- **Resource Budget:** Monitor flash/RAM usage post-refactor; target no net growth and flag if increases exceed +10 KB flash or +2 KB RAM.
- **Logging Ownership:** Continue routing OTA/WiFi logging through `logging_manager`.
### Detailed Plan — Decompose Entry Point
1. **Responsibility Inventory**
   - Annotate `main.cpp` with ownership tags (setup-only, loop-only, utility) and record mapping in a scratch note for reference.
   - Identify shared state that must move into `SystemContext` (e.g., `lightController`, `servoController`, audio-related singletons).
2. **Define Interfaces**
   - Draft `SystemContextConfig` struct capturing references to config + hardware pins.
   - Specify `AppController` public API (`bool setup()`, `void loop()`, `void enqueueCliCommand(const String&)`, etc.).
3. **Skeleton Implementation**
   - Create `app_controller.*` and `system_context.*` with minimal logic (constructors, getters, stubbed `setup`/`loop`).
   - Update `platformio.ini` or build system if new files require inclusion.
4. **Gradual Migration**
   - Migrate configuration load + SD mount into `AppController::setup`.
   - Move CLI/diagnostic commands into private helper methods, ensuring they remain callable via serial processing.
   - Move peripheral updates (WiFi, OTA, Bluetooth, audio, finger sensor) into `AppController::loop`.
5. **Arduino Entry Glue**
   - Replace global object instantiation with `static` storage inside `main.cpp` limited to context/controller pointers.
   - Update Arduino `setup()`/`loop()` to delegate to `AppController`.
6. **Validation**
   - Build for `env:esp32dev` to ensure PlatformIO integration unchanged.
   - Run `pio test -e native` to confirm existing tests still pass.


## Work Log
### 20251105-1545 — Story intake and checklist validation
- **Result:** Success
- **Notes:** Confirmed story structure aligns with house format; task list already present with actionable checkboxes.
- **Next:** Begin decomposition planning for entry point refactor (`Decompose Entry Point` task).
### 20251105-1557 — Reviewed main.cpp structure for decomposition targets
- **Result:** Success
- **Notes:** Confirmed 1.5k-line file owns globals, CLI helpers, SD scanning, and loop orchestration; identified key clusters (global hardware singletons, CLI/diagnostics, fortune flow helpers).
- **Next:** Draft proposed `AppController`/`SystemContext` responsibilities and interface boundaries.
### 20251105-1612 — Authored initial architecture and testing plan
- **Result:** Success
- **Notes:** Added Implementation Plan detailing SystemContext/AppController responsibilities, new interface seams, loop sequencing, and unit-test strategy; defined migration steps for staged refactor.
- **Next:** Break down `Decompose Entry Point` task into actionable subtasks with owners/deliverables.
### 20251105-1618 — Expanded task checklist granularity
- **Result:** Success
- **Notes:** Converted sub-bullets under each task into checkboxes to make deliverables trackable during implementation.
- **Next:** Flesh out detailed work plan for `Decompose Entry Point` subtasks (ordering, dependencies, deliverables).
### 20251105-1626 — Outlined step-by-step plan for entry-point decomposition
- **Result:** Success
- **Notes:** Added detailed sequence covering responsibility inventory, new class skeletons, incremental migration, Arduino glue updates, and validation steps.
- **Next:** Determine dependencies between interface extraction and entry-point work; capture in plan or risk notes.
### 20251105-1632 — Captured cross-task dependencies
- **Result:** Success
- **Notes:** Added dependency notes clarifying sequencing between entry-point work, interface extraction, device-specific refactors, and documentation updates.
- **Next:** Review interface extraction task to enumerate required interfaces and potential interim adapters.
### 20251105-1639 — Detailed interface extraction approach
- **Result:** Success
- **Notes:** Documented target interfaces, adapter strategy, and transitional injection plan to support gradual refactor.
- **Next:** Scope remaining tasks starting with Audio Buffer Refactor, ensuring plan aligns with new interfaces.
### 20251105-1646 — Planned audio buffer refactor
- **Result:** Success
- **Notes:** Added design/analysis steps for extracting `CircularAudioBuffer`, integration strategy, test plan, and performance validation checkpoints.
- **Next:** Outline strategy for Bluetooth/WiFi/OTA state tests leveraging new interfaces and timing seams.
### 20251105-1653 — Defined connectivity state test approach
- **Result:** Success
- **Notes:** Documented time-source injections, stub strategies, and scenario coverage for Bluetooth retry logic, WiFi reconnects, and OTA password/stream handling.
- **Next:** Plan content selection coverage updates, focusing on filesystem/RNG abstraction.
### 20251105-1700 — Planned content selection coverage work
- **Result:** Success
- **Notes:** Outlined filesystem/time/random abstractions, test fixtures, fairness checks, and log assertions for AudioDirectorySelector/SkitSelector.
- **Next:** Flesh out adapter verification plan to ensure new interface seams are exercised.
### 20251105-1706 — Detailed adapter verification strategy
- **Result:** Success
- **Notes:** Clarified adapter test coverage, fake implementations, and integration hooks to confirm bridging logic remains thin and observable.
- **Next:** Plan documentation/CI updates and open questions before moving to execution.
### 20251105-1712 — Planned documentation and enforcement tasks
- **Result:** Success
- **Notes:** Added roadmap for README updates, CI integration, changelog touchpoints, and architectural enforcement checks.
- **Next:** Review plan holistically; identify any blocking questions or resource needs before implementation.
### 20251105-1717 — Logged outstanding questions
- **Result:** Success
- **Notes:** Captured open decisions around CLI ownership, config struct scope, resource budgets, and logging ownership to resolve before coding.
- **Next:** Await guidance on outstanding questions or proceed with assumptions after stakeholder review.
### 20251105-1726 — Documented stakeholder decisions
- **Result:** Success
- **Notes:** Updated plan with dedicated `CliService`, clarified config access choices, established flash/RAM guardrails (≤ +10 KB flash / ≤ +2 KB RAM), and confirmed OTA/WiFi logging stays within `logging_manager`.
- **Next:** Begin implementation phase starting with responsibility inventory in `main.cpp` and `CliService` scaffolding.
### 20251105-1805 — Introduced AppController wrapper and minimal Arduino entrypoint
- **Result:** Success
- **Notes:** Added `AppController` class, migrated legacy runtime into `src/app_controller.cpp`, and replaced `src/main.cpp` with a thin delegate. Verified `pio run -e esp32dev` builds successfully; flash usage 1 761 521 bytes (~89.6%), RAM 62 452 bytes (~19.1%), matching prior baseline within guardrails.
- **Next:** Continue decomposing internals (move globals/state into AppController members, introduce `SystemContext`, scaffold shared `CliService`).
### 20251105-1824 — Added reusable CliService and integrated into loop
- **Result:** Success
- **Notes:** Created `CliService` module to buffer commands, replaced ad-hoc serial parsing in `AppController::loop`, and wired instantiation during setup. Confirmed `pio run -e esp32dev` still succeeds; flash 1 762 569 bytes, RAM 62 444 bytes, staying within resource guardrails.
- **Next:** Start migrating global state into `AppController` ownership and outline `SystemContext` scaffolding.
### 20251105-1910 — Consolidated runtime globals under AppController state
- **Result:** Success
- **Notes:** Introduced `RuntimeState` backing `AppController`, replaced global variables with struct-backed references, preserved CLI service integration, and ensured `pio run -e esp32dev` passes; flash 1 762 661 bytes (~89.7%), RAM 62 444 bytes (~19.1%).
- **Next:** Continue extracting `SystemContext` class to isolate construction/wiring and prepare for interface injection.
### 20251106-0148 — Wired SystemContext + AppController refactor and rebuilt runtime
- **Result:** Success
- **Notes:** Added `SystemContext` scaffolding, refactored `AppController` to consume shared `RuntimeState`, integrated `CliService`, restored CLI command handling, and removed global singletons. Platform build (`pio run -e esp32dev`) and host tests (`pio test -e native`) both pass; flash 1 763 425 bytes (~89.7%), RAM 62 484 bytes (~19.1%).
- **Next:** Begin carving adapter seams (Audio/Bluetooth/WiFi) into dedicated interface modules and add unit coverage per story tasks.
### 20251106-0342 — Injected RNG/time seams + host tests for content selectors
- **Result:** Success
- **Notes:** Added dependency injection for `AudioDirectorySelector` and `SkitSelector` (random source, time function, optional directory enumerator), updated runtime wiring, and introduced host-side unit tests (`test_audio_directory_selector`, `test_skit_selector`). Ensured native build pulls in new sources and provided UNIT_TEST shims for logging/SD access. Verified `pio test -e native` and `pio run -e esp32dev` succeed (flash 1 764 629 bytes ~89.8%, RAM 62 484 bytes ~19.1%).
- **Next:** Continue interface extraction for Bluetooth/WiFi/OTA subsystems and expand coverage to adapter behaviors.
### 20251105-1726 — Documented stakeholder decisions
- **Result:** Success
- **Notes:** Updated plan with dedicated `CliService`, clarified config access choices, established flash/RAM guardrails (≤ +10 KB flash / +2 KB RAM), and confirmed OTA/WiFi logging stays within `logging_manager`.
- **Next:** Begin implementation phase starting with responsibility inventory in `main.cpp` and `CliService` scaffolding.
### 20251106-1608 — Converted runtime ownership to `std::unique_ptr`
- **Result:** Success
- **Notes:** Replaced raw pointers across `SystemContext::RuntimeState` with `std::unique_ptr`, updated `AppController::setup` wiring to use RAII helpers, and ensured OTA/Bluetooth/WiFi adapters operate via interfaces. Added conditional includes so UNIT_TEST builds use lightweight stubs. `pio run -e esp32dev` now reports flash 1 771 197 bytes (90.1%) and RAM 62 492 bytes (19.1%), staying within story budget.
- **Next:** Backfill host-side coverage for the new runtime adapters to guard interface behavior.
### 20251106-1627 — Added runtime adapter host tests
- **Result:** Success
- **Notes:** Created native unit suite (`tests/unit/test_runtime_adapters`) with PlatformIO filter updates and lightweight stubs for Bluetooth/WiFi/OTA managers. Tests validate A2DP retry toggles, Wi-Fi connection/disconnection callbacks, and OTA pause/resume plumbing. Updated `runtime_adapters` and `system_context` headers to include stubs under UNIT_TEST. `pio test -e native` passes across all suites.
- **Next:** Extend adapter coverage to manual calibration/audio planner fakes and begin isolating timing logic for Bluetooth/WiFi/OTA state machines.
### 20251106-1705 — Extracted CircularAudioBuffer helper with host coverage
- **Result:** Success
- **Notes:** Moved ring-buffer bookkeeping from `AudioPlayer` into templated `infra::CircularAudioBuffer`, updating event thresholds to rely on helper counters. Added native tests for wrap-around, silence padding, and forced mute paths, then reran full native suite and ESP32 build (flash 1 771 189 bytes ≈90.1%, RAM 62 492 bytes ≈19.1%). Buffer helper keeps ISR usage behind existing mutexes and eliminates duplicate memcpy logic.
- **Next:** Continue on adapter verification (ManualCalibration/Fortune/Printer) and plan timing shims for connectivity state-machine tests.
### 20251106-1812 — Added adapter fakes and native coverage
- **Result:** Success
- **Notes:** Introduced lambda-driven `FortuneServiceAdapter`/`ManualCalibrationAdapter` APIs, lightweight UNIT_TEST stubs (light controller, printer, finger sensor, audio player), and a dedicated native suite validating caching, printer readiness forwarding, and manual calibration sequencing. Updated PlatformIO native filter to include `death_controller_adapters.cpp`. Confirmed `pio test -e native` and `pio run -e esp32dev` remain green (flash 1 771 189 bytes ~90.1%, RAM 62 492 bytes ~19.1%).
- **Next:** Tackle connectivity timing seams (Bluetooth/WiFi/OTA) and expand adapter coverage to audio planner integration once timing abstractions land.
### 20251106-2105 — Assessed story progress & outstanding coverage
- **Result:** Success
- **Notes:** Reviewed README and current runtime refactor; updated checklist to reflect completed selector coverage, added modularization tasks for `AppController`, and documented missing Bluetooth/WiFi/OTA timing tests plus CLI teardown work.
- **Next:** Prioritize extracting remaining CLI handlers into dedicated modules and design injectable clock seams for connectivity controllers before adding host tests.
### 20251106-2224 — Retired legacy CLI path & unified stubs
- **Result:** Success
- **Notes:** Removed `legacyProcessCLI` and Serial helpers from `AppController`, instantiate `CliService` after router wiring, and deduplicated thermal printer stubs (`death_controller_adapter_stubs.h` now reuses `thermal_printer_stub.h`). Confirmed `pio test -e native` and `pio run -e esp32dev` remain green; native stub ODR issue resolved.
- **Next:** Continue runtime loop modularization by carving SD summary/printer fault logic into injectable helpers and plan host orchestration tests.
### 20251106-2335 — Modularized setup services & printer fault handling
- **Result:** Success
- **Notes:** Introduced `runtime_services` module with injectable SD summary, fortune catalog, and printer fault monitor; `SystemContext` now wires defaults and `AppController` delegates setup/loop responsibilities via services. Added shared `audio_paths.h` constants, removed legacy helpers, and verified builds/tests (`pio test -e native`, `pio run -e esp32dev`).
- **Next:** Extract remaining loop orchestration (UART, breathing cadence) into services and design host harness covering setup sequencing.
### 20251107-0038 — Split loop coordination & added targeted host test
- **Result:** Success
- **Notes:** Added `RuntimeLoopView` plus `DefaultDeathControllerCoordinator`/`DefaultUartCommandService` (new `runtime_loop_services.cpp`), moved AppController loop to service calls, and created `tests/unit/test_runtime_services` verifying UART dispatch with stubs. Adjusted native build filters/stubs so PlatformIO tests run cleanly (`pio test -e native`) and confirmed firmware build (`pio run -e esp32dev`).
- **Next:** Plan follow-on slice for a comprehensive AppController orchestration harness once additional services (UART, breathing, CLI polling) are extracted.
