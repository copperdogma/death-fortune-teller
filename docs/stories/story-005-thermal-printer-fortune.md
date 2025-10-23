# Story: Thermal Printer Fortune Output

**Status**: To Do

---

## Related Requirement
[docs/spec.md §11 Bring-Up Plan — Step 8](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Alignment with Design
[docs/spec.md §4 Assets & File Layout — Printer](../spec.md#4-assets--file-layout)

## Acceptance Criteria
- Printer receives UART commands at the configured baud rate and prints the logo followed by a generated fortune during the snap flow.
- Fortune generator validates `fortunes_littlekid.json` on boot and fills templates without runtime errors.
- Printing occurs while audio playback continues without underruns or noticeable latency spikes.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [ ] Implement printer driver integration, including logo bitmap handling and fortune text formatting.
- [ ] Hook fortune generation into the FORTUNE_FLOW sequence after the snap event.
- [ ] Stress-test simultaneous printing and audio playback to confirm power and timing stability.
- [ ] Document SD card printer asset requirements and maintenance workflow.

## Notes
- Monitor printer power draw; record any mitigation (caps, wiring) required for reliable output.
- Plan for future mode-specific fortune sets once JSON format stabilizes.
