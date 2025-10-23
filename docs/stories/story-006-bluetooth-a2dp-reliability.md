# Story: Bluetooth A2DP Reliability & Testing

**Status**: To Do

---

## Related Requirement
[docs/spec.md §13 Risks & Mitigations — Audio Reliability](../spec.md#13-risks--mitigations)

## Alignment with Design
[docs/spec.md §4 Assets & File Layout — Audio](../spec.md#4-assets--file-layout)

## Acceptance Criteria
- Bluetooth speaker pairs and reconnects automatically after disconnections, honoring the 5 s retry policy.
- Audio playback remains glitch-free during concurrent operations (printing, servo motion) for at least a 10-minute endurance run.
- Logging captures connection state transitions to aid field diagnostics without flooding the console.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [ ] Harden the A2DP controller with retry, state reporting, and error handling aligned to prior PoCs.
- [ ] Conduct soak tests covering pairing, disconnection, and reconnection scenarios.
- [ ] Tune buffering and PSRAM usage (if needed) to avoid underruns under load.
- [ ] Summarize reliability findings and recommended speaker settings in project docs.

## Notes
- Coordinate test cases with stories covering printer and finger flow to replicate real-world load.
- Consider automating stress tests via serial console commands for regression coverage.
