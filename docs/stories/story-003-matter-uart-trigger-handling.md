# Story: Matter UART Trigger Handling

**Status**: To Do

---

## Related Requirement
[docs/spec.md §2 Modes, Triggers, and Behavior](../spec.md#2-modes-triggers-and-behavior)

## Alignment with Design
[docs/spec.md Appendix A — UART Constants](../spec.md#appendix-a--uart-constants-from-your-poc)

## Acceptance Criteria
- Skull firmware receives `TRIGGER_FAR` and `TRIGGER_NEAR` commands over UART1 at 115200 8N1 using the defined frame structure.
- Busy policy enforces dropping triggers while any skit or snap/print phase is active, with 2 s debounce for duplicates.
- Commands translate into state machine transitions exactly as defined for IDLE, PLAY_WELCOME, and FORTUNE_FLOW paths.
- [ ] User must sign off on functionality before story can be marked complete.

## Tasks
- [ ] Implement UART frame parsing with CRC-8 validation per PoC constants.
- [ ] Wire trigger handling into the runtime state machine, enforcing debounce and busy-drop logic.
- [ ] Add serial console diagnostics for received commands and rejected triggers.
- [ ] Bench-test with the Matter node PoC or simulated frames to confirm behavior.

## Notes
- Coordinate UART pin assignments with the thermal printer to avoid conflicts.
- Ensure logging verbosity is safe for production once validation completes.
