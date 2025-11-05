# Story: Death State Controller Extraction

**Status**: To Do

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
- [ ] **Sketch Controller Shape** — define dependency struct, controller interface, and timer abstraction to replace direct `millis()`/globals.
- [ ] **Extract State Machine** — move state enums, trigger handling, fortune flow, and printing coordination into the controller with injected collaborators.
- [ ] **Introduce Time/Test Seams** — add `ITimeProvider` (or similar) so controller can advance simulated time in host tests.
- [ ] **Wire Arduino Entry Points** — update `setup()`/`loop()` to instantiate dependencies, register the controller, and delegate updates.
- [ ] **Author Unit Tests** — create host tests verifying debounced triggers, busy-drop behavior, fortune fallback, printer unavailability, and manual calibration entry/exit where feasible.
- [ ] **Regression Build** — ensure `pio test -e native` and `pio run -e esp32dev` remain green; document any manual validation performed.

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

## Work Log
_No work logged yet; start entries once implementation begins._
