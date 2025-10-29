# Story: Non-Blocking Runtime Refactor

**Status**: To Do  
**Owner**: TBD  
**Priority**: High

---

## Related Requirements
- [docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime)
- [docs/spec.md §10 Test Console & Diagnostics](../spec.md#10-test-console--diagnostics)
- [docs/spec.md §11 Bring-Up Plan](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Problem Statement

The current firmware retains several legacy, blocking control paths that periodically stall the main `loop()` thread. These pauses break real-time behaviours (finger sensor streaming, diagnostics, animation timing) and become visible as 2–3 s “freeze” windows, especially noticeable every time the idle breathing routine runs or A2DP retries fire. We need to convert the runtime-critical routines to non-blocking, state-driven logic so the loop stays responsive.

## Blocking Call Inventory (must be converted)

1. **`breathingJawMovement()` → `ServoController::smoothMove()` (src/main.cpp:1345 & src/servo_controller.cpp:173)**  
   - `smoothMove` performs a tight `while` with `delay(20)` until the motion finishes (≈2 s).  
   - **Approach**: Replace with a time-sliced jaw animator that advances the servo towards the target on each `loop()` pass (reuse or extend `SkullAudioAnimator`’s smoothing). Emit a non-blocking scheduler hook (e.g., `ServoController::requestSmoothMove(target, duration)` plus `ServoController::update()`).

2. **LightController blink helpers (src/light_controller.cpp:107–146)**  
   - `blinkEyes`, `blinkMouth`, and `blinkLights` execute `delay(100)` loops.  
   - **Approach**: Convert to async blink sequences tracked via timestamps inside the controller (`update()` already exists). Queue LED animations and let `LightController::update()` step them.

3. **Bluetooth manual control wait loops (src/bluetooth_controller.cpp:324–339, 447–505)**  
   - Blocking `while` loops with `delay(10)` while waiting for disconnect/controller disable. They can stall the loop up to ~1.5 s.  
   - **Approach**: Track deadlines and let `update()` poll ESP-IDF status instead of sleeping. For controller/Bluedroid disable, schedule state machine transitions rather than spinning.

4. **`remote_debug_manager.cpp` reboot path (line 227)**  
   - Uses `delay(1000)` before `esp_restart()`.  
   - **Approach**: set a restart timestamp and return; call `esp_restart()` once the deadline passes (without blocking loop).

5. **Diagnostic helpers (`testSkitSelection`, `testCategorySelection`, servo initialization sweeps)**  
   - Contain `delay(...)` loops that run when invoked from CLI. These should at least yield control via async tasks so diagnostics don’t stall sensor streaming.  
   - **Approach**: wrap in scheduler jobs or require explicit disabling of real-time streams before running; document if left blocking.

> Note: The ESP32 A2DP library’s `start()` call is intrinsically blocking. We cannot change the library internals, but we can isolate it on a separate FreeRTOS task (or ensure retries only run when Bluetooth is enabled and real-time diagnostics are paused).

## Acceptance Criteria

- [ ] Servo breathing animation and servo smooth moves complete without calling `delay()` inside tight loops; the main loop cadence remains <5 ms during idle streaming.  
- [ ] Light blinking routines no longer block; LED sequences run via controller `update()`.  
- [ ] Manual Bluetooth enable/disable paths never stall the loop for more than 20 ms.  
- [ ] Remote Debug commands (`reboot`, `bluetooth off/on`) return promptly while operations finish asynchronously.  
- [ ] Finger sensor streaming (`fon`) maintains consistent 10 Hz output with Bluetooth enabled or disabled and with breathing animation active.  
- [ ] Unit/integration tests (or bench script) confirm no higher-than-expected loop latency spikes after refactor.

## Tasks

1. **Audit & Instrumentation**
   - Add optional loop latency tracking (e.g., `micros()` delta histogram) to validate improvements.
   - Confirm all remaining `delay()` or blocking loops are catalogued; update this story if new ones are found.

2. **Servo Controller Refactor**
   - Introduce non-blocking smooth-move API with internal progress tracking.
   - Update `breathingJawMovement()` and any other callers to schedule moves rather than block.

3. **Light Controller Animation Queue**
   - Implement mouth/eye blink sequences as timed jobs inside `LightController::update()`.
   - Ensure startup blink routines use the new mechanism.

4. **Bluetooth State Machine Updates**
   - Replace blocking disconnect/disable waits with state polling in `update()`.
   - Consider moving A2DP retry logic onto a FreeRTOS task if necessary to avoid loop stalls.

5. **Remote Debug & Diagnostics**
   - Convert `reboot` and any CLI/RemoteDebug commands that currently call `delay()` into deferred actions.
   - Decide on behaviour for CLI test utilities (`testSkitSelection`, servo sweeps); either make them async or document required preconditions.

6. **Testing & Verification**
   - Bench test with `fon` streaming + Bluetooth enabled and disabled; confirm no periodic stalls.
   - Run full fortune flow to ensure animations and LED behaviour remain correct with new async logic.
   - Update docs and CLI help to reflect any behavioural changes.

## Risks & Mitigations

- **Complexity increase**: Converting straight-line scripts into state machines can introduce bugs. *Mitigation*: Build small update helpers with clear state enums; add debug logs during transition.
- **ESP-IDF constraints**: Some BT APIs remain blocking. *Mitigation*: confine them to moments when the system is known idle or offload to secondary tasks.
- **Regression in animation timing**: Non-blocking moves must maintain existing motion smoothness. *Mitigation*: reuse existing smoothing math and validate motion on hardware.

## Definition of Done

- Loop latency metric shows no >50 ms stalls during idle streaming.
- `fon` output remains continuous (≈10 Hz) for at least 60 seconds with Bluetooth enabled.
- Code merged with updated documentation and story log entries.
- User sign-off on responsiveness after hardware validation.
