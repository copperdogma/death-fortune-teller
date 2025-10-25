# Story: SD Audio Playback & Jaw Synchronization

**Status**: Complete

---

## Related Requirement
[docs/spec.md §11 Bring-Up Plan — Steps 2 & 3](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Alignment with Design
[docs/spec.md §4 Assets & File Layout](../spec.md#4-assets--file-layout)

## Acceptance Criteria
- WAV assets load from the SD card and stream reliably to the paired Bluetooth speaker via A2DP.
- Jaw servo motion aligns with audio energy analysis, producing natural speech motion with no stalls.
- The system prevents immediate repeat of the same skit file across consecutive plays.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [x] Port or implement SD card mounting and audio playback modules from the TwoSkulls base.
- [x] Integrate FFT-based jaw animation tuned for single-servo hardware.
- [x] Populate test assets on the SD card and validate playback loop end-to-end.
- [x] Document asset naming expectations and any calibration parameters discovered.
- [x] Implement SkitSelector to prevent immediate repeat of same skit file.

## Notes
- Confirm playback robustness while preparing for simultaneous printer activity in later stories.
- Capture any servo safety considerations (e.g., motion limits) gathered during tuning.
