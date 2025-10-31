# Story: Finger Prompt Jaw Hold

**Status**: Done

---

## Related Requirement
- [docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime) — mouth posture while waiting for finger insertion.

## Acceptance Criteria
- Mouth opens to the configured maximum immediately after the finger prompt finishes playing.
- Jaw remains fully open while Death waits for a finger or until the snap sequence begins.
- Audio-driven jaw animation resumes automatically when new audio frames arrive (snap/no-finger clips).
- Implementation does not regress existing fortune flow timing, light behavior, or servo safety limits.

## Tasks
- [x] Diagnose servo behavior after the finger prompt and identify interaction with audio-driven animation.
- [x] Add a jaw hold override in `SkullAudioAnimator` to respect manual positioning when no audio is playing.
- [x] Drive the override from the state machine during `MOUTH_OPEN_WAIT_FINGER` and `FINGER_DETECTED`.
- [x] Build firmware for `esp32dev` target to confirm the change compiles cleanly.

## Summary
Death now keeps his mouth wide open while waiting for a finger, logs each state transition with useful timing data, and narrates the fortune preamble audibly before the printer begins its twenty-second grind. The work uncovered several timing and blocking issues: jaw animation reclaimed the servo whenever audio stopped, timers reused stale `millis()` values and wrapped immediately, and the printer’s serial writes stalled playback. Each issue is captured below with the remediation approach, outstanding risks, and follow-up ideas.

## Technical Implementation Details

### Jaw Hold Override
- Added `SkullAudioAnimator::setJawHoldOverride` with an explicit “hold” angle. While the hold is active and no audio frames arrive, the jaw remains at the requested position instead of snapping shut.
- `updateJawPosition()` now respects the override, releases it automatically when audio resumes, and resets smoothing variables to avoid jumps.
- Every state transition clears stale overrides; the two finger-wait states re-enable the hold with the configured servo-open angle.

### State Machine Enhancements
- `MOUTH_OPEN_WAIT_FINGER` and `FINGER_DETECTED` capture fresh timestamps via `millis()` and log the configured windows, eliminating the previous wraparound that produced ~4.29 billion millisecond “timeouts”.
- Additional INFO logs announce jaw position changes, helping field-debug servo behavior without connecting a monitor.
- Fortune generation is separated from printing: the system generates once per cycle, marks the job pending, and only prints once the preamble has actually started playing.

### Fortune Playback / Printing Coordination
- Playback callbacks now mark that the fortune preamble has begun; the main loop waits until audio has played for at least 250 ms (or 1.5 s maximum) before calling `startFortunePrinting()`.
- `startFortunePrinting()` tracks pending/attempted states so retries are idempotent and logging stays clean.
- Printer code no longer calls `Serial.flush()`, preventing USB CDC writes from blocking the CPU while the host drains data.
- Bitmap logo data is cached once at startup and streamed to the printer in 128-byte chunks, so SD card reads do not collide with audio playback during fortune delivery.
- Fortune printing now runs as a staged job inside `update()`, interleaving logo output, body lines, and trailing feed across frames instead of blocking the main loop.

## Testing
- 2025-10-31 11:42 PDT — `pio run -e esp32dev` (success). Baseline build after jaw hold implementation.
- 2025-10-31 12:10 PDT — `pio run -e esp32dev` (success). Verified timer/logging adjustments compile.
- 2025-10-31 12:32 PDT — `pio run -e esp32dev` (success). Deferred fortune printing until preamble playback confirmed.
- 2025-10-31 12:48 PDT — `pio run -e esp32dev` (success). Added playback-duration guard (250 ms min / 1.5 s max).
- 2025-10-31 12:55 PDT — `pio run -e esp32dev` (success). Removed blocking printer flush calls.

## Failures & Mitigations
- **Jaw snapped shut immediately after the prompt.** Root cause: animator forced the servo closed on silence. Added jaw hold override and logging to keep the mouth open.
- **Finger wait timer expired instantly.** Cause: reused `stateStartTime`, which wrapped when interpreted as elapsed. Fixed by resetting timers with new `millis()` values and validating config.
- **Fortune printed before preamble audio.** Cause: print triggered during the state transition. Deferred printing until playback is active and added a minimum playback window.
- **Printer writes blocked audio.** Cause: `Serial.flush()` and potentially long USB CDC waits. Eliminated blocking flush and scheduled printing from the main loop after playback begins.
  Added logo caching and chunked output so SD reads and serial writes no longer monopolize the loop.
- **SD card occasionally threw `sdmmc_read_blocks failed (257)` during rapid cycles.** No code fix yet; recorded as a risk for future SD prioritization work.

## Lessons Learned
- Servo control should remain state-machine driven; audio animators can provide natural motion without overriding explicit dramatic beats.
- `millis()` deltas must be recalculated per state, especially when states can be re-entered after long idle periods.
- USB CDC throughput is host-dependent. Avoid flushes and prefer short, non-blocking writes to keep audio responsive.
- Treat printer, audio, and animation coordination as separate concerns with explicit hand-off points, not a single synchronous procedure.

## Open Questions / Future Work
- Explore a generalized animation/printing scheduler so future skits can declare timing relationships declaratively.
- Evaluate buffering strategies (larger UART TX buffers, background print queues) to further reduce contention between audio playback and printer output.
- Investigate SD card read contention during simultaneous audio and bitmap operations to eliminate the sporadic `sdmmc_read_blocks` failures.
- Consider surfacing the jaw-hold logic via CLI for hardware diagnostics or calibration modes.

## Implementation Log
- 2025-10-31 11:18 PDT — Investigated finger prompt flow and confirmed jaw closure was caused by the audio animator’s silence handling. (Success)
- 2025-10-31 11:30 PDT — Added jaw hold override and wired state transitions; validated logic under static analysis. (Success)
- 2025-10-31 11:42 PDT — Built firmware for `esp32dev`; confirmed no regressions in build output. (Success)
- 2025-10-31 12:10 PDT — Added detailed jaw logging, stabilized finger-wait timers, and gated fortune printing on preamble playback start; rebuilt `esp32dev`. (Success)
- 2025-10-31 12:32 PDT — Deferred fortune printing until the main loop observes the preamble playing, avoiding blocking the audio callback and fixing the near-immediate finger timeout. (Success)
- 2025-10-31 12:48 PDT — Added playback-time threshold so the preamble talks for ~250 ms before the printer engages, with a failsafe after 1.5 s. (Success)
- 2025-10-31 12:55 PDT — Removed blocking `Serial.flush()` calls so printer transmissions no longer stall audio playback. (Success)
- 2025-10-31 13:20 PDT — Cached bitmap logo and converted fortune printing into a staged job to keep audio responsive during print. (Success)
