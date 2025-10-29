# Story: Capacitive Finger Detection & Snap Flow

**Status**: Done

---

## Related Requirement
[docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime)

## Alignment with Design
[docs/hardware.md §Current Pin Assignments](../hardware.md#current-pin-assignments) — GPIO4 (Touch0) on ESP32-WROVER for the capacitive foil
[docs/spec.md §8 Config.txt Keys — Cap Sense & Timing](../spec.md#8-configtxt-keys)
[proof-of-concept-modules/finger-detector-test](../../proof-of-concept-modules/finger-detector-test/README.md) — Baseline touch implementation that currently works on hardware

## Acceptance Criteria
- Capacitive sensor calibrates on boot while the mouth is open/quiet and detects a solid finger presence for ≥120 ms (configurable) before triggering the snap timer. Detection logic matches the existing proof-of-concept (normalized delta against baseline, 0.2 % default threshold) on ESP32-WROVER GPIO4.
- Mouth LED starts OFF, jumps to BRIGHT as the mouth opens, then transitions into a slow pulse while waiting for the finger; it turns OFF immediately when the snap fires (finger or timeout).
- Fortune flow aborts cleanly after the 6 s timeout with jaw closure, LED off, and state reset to IDLE.
- Snap action drives the servo to the closed position and immediately kicks off fortune generation/printing while audio continues.
- [x] User must sign off on functionality before story can be marked complete (2025-10-28 “Nice! It's working.”)

## Tasks
- [x] **Implement Baseline/Detection Flow**: Pull normalized delta logic from the finger detector PoC so ESP32-WROVER detects touches reliably with the copper foil on GPIO4.
- [x] **Implement Snap Action**: Add servo snap (mouth closure) and fortune generation.
- [x] **Add Timeout Handling**: Implement 6-second timeout with mouth closure and state reset.
- [x] **Integrate Mouth LED**: Add solid-bright → slow pulse → off sequence driven by state transitions.
- [x] **Add Configuration**: Surface threshold, debounce duration, timeout, and snap delay via `config.txt`.
- [x] **Test Hardware**: Validate with the existing copper foil electrode on ESP32-WROVER hardware.

## Technical Implementation Details

### Touch Hardware Notes

- Production hardware uses the ESP32-WROVER Touch0 pad (GPIO4) tied to a copper foil electrode via shielded coax with the shield grounded.
- Calibrate after boot while the mouth remains open and the electronics are quiet, adopting the PoC baseline smoothing approach.
- Normal ESP32 touch values decrease on contact; the PoC already accounts for ESP32-S3 increasing behavior if we ever migrate, but the MVP targets WROVER.
- Handle calibration failures gracefully with sensible defaults and log warnings.

### Finger Detection Requirements

**Detection Logic:**
- Require solid finger presence for ≥120 ms (configurable) before triggering the snap timer
- Implement debounce to prevent false triggers from brief touches
- Use normalized delta `(baseline - filtered) / baseline` for ESP32 touch input (per PoC) with a default 0.2 % threshold configurable via `config.txt`
- Handle sensor noise and environmental interference

**Timing Requirements:**
- 6-second maximum wait time for finger detection
- Random snap delay of 1-3 seconds after finger detection
- Snap occurs even if finger is withdrawn during delay
- Clean timeout handling with mouth closure

### Snap Action Requirements

**Servo Control:**
- Snap action triggers servo to closed position (servo minimum degrees)
- Ensure servo safety limits are respected during snap
- Maintain servo position after snap until fortune flow completes
- Handle servo failures gracefully without system crash

**Fortune Generation:**
- Generate fortune immediately after snap action
- Print fortune while continuing audio playback
- Handle fortune generation failures gracefully
- Maintain audio continuity during printing process

### Mouth LED Requirements

**LED Behavior:**
- Mouth LED OFF before the prompt plays
- BRIGHT as the mouth opens for the prompt
- Slow pulse (e.g., 1 Hz fade) while waiting for finger detection
- LED OFF during snap delay, snap, and the rest of the fortune flow
- Configurable LED brightness/pulse timing (default matches PoC guidance)

**State Integration:**
- LED behavior synchronized with state machine transitions
- LED off during non-fortune states
- Handle LED control failures without affecting main flow
- Support LED brightness configuration

### Configuration Requirements

**Configurable Parameters:**
- Capacitive sensor threshold (`cap_threshold`)
- Finger detection duration (`finger_detect_ms`)
- Finger wait timeout (`finger_wait_ms`)
- Snap delay range (`snap_delay_min_ms` / `snap_delay_max_ms`)
- LED brightness and pulse timing (`mouth_led_bright`, `mouth_led_pulse_min`, `mouth_led_pulse_max`, `mouth_led_pulse_period_ms`)

**Hardware-Specific Settings:**
- Pin assignments hardcoded in firmware (not configurable)
- Threshold values configurable per skull hardware
- Timing values configurable for different use cases
- Default values for all configurable parameters

### Error Handling and Recovery

**Sensor Failures:**
- Handle capacitive sensor hardware failures
- Fall back to timeout behavior when sensor fails
- Log sensor errors for debugging and maintenance
- Continue system operation with reduced functionality

**Servo Failures:**
- Handle servo hardware failures gracefully
- Log servo errors for debugging
- Continue audio flow without servo motion
- Maintain system operation with audio-only mode

**Timeout Handling:**
- Clean timeout after 6-second finger wait
- Close mouth and reset state on timeout
- Log timeout events for debugging
- Return to IDLE state after timeout

### Integration Requirements

**State Machine Integration:**
- Integrate with existing state machine transitions
- Maintain existing timing and trigger behavior
- Support all existing state transitions
- Preserve busy policy and debounce logic

**Hardware Integration:**
- Integrate with existing servo control system
- Integrate with existing LED control system
- Integrate with existing thermal printer system
- Maintain existing hardware performance and reliability

**Audio Integration:**
- Maintain audio continuity during snap action
- Support concurrent audio and printing operations
- Handle audio timing during fortune flow
- Preserve existing audio quality and synchronization

## Notes
- **Critical**: ESP32 touch behavior changes across MCU families. Our current build uses ESP32-WROVER (values drop on touch); the PoC accommodates ESP32-S3 (values rise) if needed later.
- Coordinate with Story 005 (Thermal Printer) for fortune generation
- Test with actual copper electrodes on ESP32-WROVER hardware

## Implementation Log
- 2025-10-28 14:20 PDT — Documented WROVER touch hardware assumptions, LED behavior sequence, and PoC reference; refreshed acceptance criteria/tasks for story scope. (Success)
- 2025-10-28 16:05 PDT — Refactored `finger_sensor` with PoC-style baseline/normalized delta logic, added config-driven debounce, and wired new mouth LED pulse controls plus state-machine updates. (Success)
- 2025-10-28 16:35 PDT — Built `esp32dev` and `esp32dev_ota` environments via `pio run`; OTA target warned about missing `upload_port` (expected) but both builds succeeded. (Success)
- 2025-10-28 17:05 PDT — Added `f*` tuning CLI commands (threshold/debounce/stream) with live sensor streaming (`fon`/`foff`) mirroring the PoC workflow. (Success)
- 2025-10-28 17:22 PDT — Updated `help` output to highlight finger tuning commands; attempted USB/OTA uploads via `pio run -t upload`, but both failed (serial port busy / OTA args missing). (Partial)
- 2025-10-28 17:40 PDT — Reworked finger sensor stream/status output to show raw and normalized deltas alongside thresholds; rebuilt firmware (`pio run`). (Success)
- 2025-10-28 17:55 PDT — Added multi-sample averaging for finger sensor (32 samples) and higher precision telemetry for better field tuning; rebuilt firmware (`pio run`). (Success)
- 2025-10-28 18:25 PDT — Expanded threshold command parsing to accept both `thresh` and `fthresh` forms; rebuilt firmware (`pio run`). (Success)
- 2025-10-28 18:35 PDT — Disabled jaw “breathing” animation while finger sensor streaming is active to keep serial output continuous; rebuilt firmware (`pio run`). (Success)
- 2025-10-28 18:55 PDT — Exposed finger sensor tuning controls (`fcycles`, `falpha`, `fdrift`, `fmultisample`) and added runtime status fields for touchSetCycles, smoothing, drift, and multisample count. (Success)
- 2025-10-28 19:20 PDT — Hooked finger tuning parameters into `config.txt`, widened threshold range checks, and applied new defaults during initialization. (Success)
- Consider fallback behavior when finger sensor fails
- Ensure proper servo safety limits during snap action
- Capture empirical threshold values for different hardware builds in config guidance
- Ensure timers reuse the shared scheduler/loop primitives to avoid drift

## Dependencies
- Story 005 (Thermal Printer Fortune Output) - Fortune generation and printing
- ESP32-S3 hardware with copper electrodes
- Actual fortune content (external dependency)

## Testing Strategy
1. **Hardware Testing**: Validate ESP32-S3 touch sensor behavior with copper electrodes
2. **Integration Testing**: Test complete fortune flow with snap action
3. **Timeout Testing**: Verify 6-second timeout behavior
4. **LED Testing**: Confirm mouth LED synchronization with states
