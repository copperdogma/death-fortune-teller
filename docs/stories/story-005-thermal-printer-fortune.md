# Story: Thermal Printer Fortune Output

**Status**: To Do

---

## Related Requirement
[docs/spec.md §11 Bring-Up Plan — Step 8](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Alignment with Design
[docs/spec.md §4 Assets & File Layout — Printer](../spec.md#4-assets--file-layout)

## Acceptance Criteria
1. Validate JSON file(s) on SD card in `/fortunes` directory on startup (see `little_kid_fortunes.json` for current format). There may be more files in the future, but for now we just have this one.
2. Generate fortune from template when called, filling it in mad-libs style by selecting random words from the wordlists.
3. Print header image + fortune to printer while skeleton keeps talking to entertain kids during printing process. This should occur without audio underruns or noticeable latency spikes.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [ ] Validate and parse JSON fortune files from SD card (`/fortunes` directory) on startup.
- [ ] Implement fortune generator that fills templates in mad-libs style by selecting random words from wordlists.
- [ ] Implement printer driver integration, including header image/logo bitmap handling and fortune text formatting.
- [ ] Hook fortune generation and printing into the FORTUNE_FLOW sequence after the snap event.
- [ ] Test simultaneous printing and audio playback to ensure no audio underruns or latency spikes.
- [ ] Document SD card fortune file requirements and maintenance workflow.

## Notes
- Monitor printer power draw; record any mitigation (caps, wiring) required for reliable output.
- Plan for future mode-specific fortune sets once JSON format stabilizes.
