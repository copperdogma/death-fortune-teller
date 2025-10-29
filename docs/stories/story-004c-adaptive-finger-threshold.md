# Story: Adaptive Finger Sensor Threshold

**Status**: In Progress

---

## Related Requirement
[docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime) — Finger detection drives snap timing  
[docs/spec.md §8 Config.txt Keys — Cap Sense & Timing](../spec.md#8-configtxt-keys)

## Alignment with Design
[docs/hardware.md §Current Pin Assignments](../hardware.md#current-pin-assignments) — Touch sensing on GPIO4  
[docs/stories/story-004-cap-sense-snap-flow.md](story-004-cap-sense-snap-flow.md) — Baseline capacitive detection story

## Acceptance Criteria
- Calibration gathers min/max (or equivalent) normalized delta during its sample window, derives a noise floor, and computes the live threshold from that noise floor and a user-selected “sensitivity” scalar.
- Sensitivity is a normalized value in the range `[0.0, 1.0]` (higher = larger safety margin / less sensitive) that maps to the post-calibration threshold; default sensitivity keeps the system equivalent to today’s ~0.2 % ratio.
- Immediately after calibration, with no finger present, the system must stay below the computed threshold (no instant “finger detected” logs) for at least 10 s in ambient testing.
- Manual recalibration flow (double-tap gesture) reuses the new adaptive threshold logic and the current sensitivity value.
- Serial console exposes commands to view and modify sensitivity, and `help` text documents the new commands; values persist until reboot or changed again.
- [ ] User sign-off required before marking this story complete.

## Tasks
- [ ] **Capture Calibration Noise Metrics**: Track min/max (or aggregated percentile) normalized deltas during calibration to estimate background noise.
- [ ] **Implement Sensitivity Mapping**: Convert the `[0.0, 1.0]` sensitivity setting into a normalized threshold derived from the noise measurement with sane floor/ceiling clamps.
- [ ] **Integrate With Calibration Flow**: Apply the computed threshold at the end of both boot-time and manual calibration; ensure config/CLI overrides cooperate with the adaptive value.
- [ ] **Update Serial Console**: Add `fsens` (or similar) command plus status output so operators can inspect and tweak sensitivity, and document it in `help`.
- [ ] **Telemetry & Logging**: Log calibration stats (baseline, noise delta, resulting threshold, sensitivity) and surface the values in `fstatus`.
- [ ] **Hardware Validation**: Confirm default sensitivity avoids false positives while still detecting a finger entering the mouth; adjust default mapping as needed.

## Technical Implementation Details

### Calibration Noise Capture
- Extend the 1 s calibration loop to track peak normalized delta or an averaged magnitude that reflects ambient noise.  
- Ignore obvious outliers (e.g., clamp extreme spikes or use a rolling percentile) so accidental touches during calibration do not explode the computed threshold.  
- Store the derived noise metric for later inspection (CLI/status).

### Sensitivity Mapping
- Define a function `threshold = clamp(noiseFloor * mapSensitivity(sensitivity), minThreshold, maxThreshold)` so the operator chooses how far above noise the trigger should be.  
- Ensure a minimum threshold exists even if noiseFloor≈0 to avoid division-by-zero issues.  
- Default sensitivity should replicate the previous 0.002 normalized ratio when the environment is quiet.

### CLI & Runtime Control
- Add a `fsens <0.0-1.0>` command alongside existing `f*` tuning commands; include validation and confirmation output.  
- Show current sensitivity and adaptive threshold in both `fstatus` and `fingerSensor->printSettings()`.  
- Update the `help` output with a brief description of the new command and what the sensitivity value means.

### Manual Recalibration Integration
- Manual calibration gesture (double tap) should honor the updated threshold logic without extra branching.  
- Ensure any latched states (e.g., `highTouchLatch`) reset appropriately so the recalibration uses fresh noise data.  
- After recalibration, verify that previously scheduled thresholds/logs update to the new value.

### Config & Defaults
- Keep SD card config keys (`cap_threshold`, etc.) functional as fallbacks; allow sensitivity to override only when set (or adopt a new config key later).  
- For now, hardcode a default sensitivity constant that matches field-tested behavior; future work may expose it via `config.txt`.

### Logging & Diagnostics
- Calibration completion log must now report baseline, noise measurement, sensitivity, and computed threshold ratio.  
- Add serial diagnostics showing both the configured sensitivity and the adaptive threshold so field techs can validate the behavior quickly.

## Notes
- Adaptive thresholds reduce false positives in noisy environments without sacrificing responsiveness when the finger is truly present.  
- Keep the existing CLI tuning commands operational; sensitivity becomes another tool rather than a wholesale replacement.  
- Consider persisting sensitivity in non-volatile storage in a future story if operators find themselves retuning after each reboot.
- Manual calibration now requires a 3 s sustained “strong touch” (≈10× threshold). If the sensor self-triggers and stays above that level, it will re-enter calibration automatically—an accidental but helpful self-healing behavior worth preserving or formalizing.

## Dependencies
- Story 004 (Capacitive Finger Detection & Snap Flow) — foundational finger detection behavior  
- Story 005a (Serial Console & Diagnostics) — broader CLI work that will eventually fold in these controls

## Testing Strategy
1. **Bench Calibration Tests**: Run boot calibration in several environments (quiet desk, near power supplies) to ensure the adaptive threshold avoids self-triggering.  
2. **Sensitivity Sweep**: Test low, medium, and high sensitivity values to verify finger detection response and ensure no false triggers at default.  
3. **Manual Recalibration**: Perform the double-tap sequence multiple times to confirm thresholds recalc correctly each run.  
4. **Regression**: Verify existing CLI commands (`fstatus`, `fthresh`, etc.) still behave (or gracefully report redundancy) to avoid breaking current workflows.

## Implementation Log
- 2025-10-29 — Implemented adaptive noise tracking, post-calibration settle window, sensitivity CLI (`fsens`), and updated documentation/config semantics. Awaiting hardware validation.
- 2025-10-29 — Final cleanup: 3 s hold-to-calibrate gesture, restored SD-configurable sensitivity (default 0.1), trimmed telemetry, and documented the self-healing recalibration behavior.
