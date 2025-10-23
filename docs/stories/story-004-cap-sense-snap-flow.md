# Story: Capacitive Finger Detection & Snap Flow

**Status**: To Do

---

## Related Requirement
[docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime)

## Alignment with Design
[docs/spec.md §8 Config.txt Keys — Cap Sense & Timing](../spec.md#8-configtxt-keys)

## Acceptance Criteria
- Capacitive sensor calibrates on boot and detects a solid finger presence for ≥120 ms before triggering the snap timer.
- Mouth LED behavior matches the prompt/pulse sequence, and snap occurs 1–3 s after detection even if the finger withdraws.
- Fortune flow aborts cleanly after a 6 s timeout with mouth closure and state reset to IDLE.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [ ] Integrate capacitive sensing driver with calibration and threshold controls sourced from config.
- [ ] Implement WAIT_FINGER logic, including the randomized snap delay and timeout handling.
- [ ] Synchronize mouth servo and LED cues with finger detection states.
- [ ] Verify behavior through hardware tests and adjust thresholds as needed.

## Notes
- Capture empirical threshold values for different hardware builds in config guidance.
- Ensure timers reuse the shared scheduler/loop primitives to avoid drift.
