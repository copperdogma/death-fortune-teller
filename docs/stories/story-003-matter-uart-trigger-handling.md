# Story: Matter UART Trigger Handling

**Status**: To Do

---

## Related Requirement
[docs/spec.md §6 UART Link](../spec.md#6-uart-link-skull-matter-node--skull-main)

## Alignment with Matter Controller
The Matter controller (https://github.com/copperdogma/death-matter-controller) defines the canonical UART command codes. Death must map these command codes to its state machine.

**Matter Controller Command Codes** (canonical):
- `0x01` - CMD_FAR_MOTION
- `0x02` - CMD_NEAR_MOTION
- `0x03` - CMD_PLAY_WELCOME
- `0x04` - CMD_WAIT_FOR_NEAR
- `0x05` - CMD_PLAY_FINGER_PROMPT
- `0x06` - CMD_MOUTH_OPEN_WAIT_FINGER
- `0x07` - CMD_FINGER_DETECTED
- `0x08` - CMD_SNAP_WITH_FINGER
- `0x09` - CMD_SNAP_NO_FINGER
- `0x0A` - CMD_FORTUNE_FLOW
- `0x0B` - CMD_FORTUNE_DONE
- `0x0C` - CMD_COOLDOWN

## Acceptance Criteria
- UART controller receives and parses frame-based protocol from Matter controller
- All 12 command codes map correctly to Death state machine states
- Busy policy enforces dropping commands while any skit or snap/print phase is active, with 2 s debounce for duplicates
- Both trigger commands (FAR/NEAR) and direct state forcing commands are supported
- Commands translate into state machine transitions exactly as defined in story-003a
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Update UART Command Enum**: Expand `UARTCommand` enum in `src/uart_controller.h` to include all 12 states (currently only has FAR_MOTION_DETECTED, NEAR_MOTION_DETECTED, SET_MODE, PING)
- [ ] **Update Command Code Mapping**: Update `commandFromByte()` in `src/uart_controller.cpp` to map all 12 command codes (currently only maps 0x05, 0x06, 0x02, 0x04)
- [ ] **Add State Forcing Support**: Support direct state forcing (commands 0x03-0x0C) in addition to trigger commands (0x01-0x02)
- [ ] **Wire UART Processing**: Ensure `handleUARTCommand()` is called from main loop (currently `uartController->update()` is called but commands aren't processed)
- [ ] **Implement Frame Parsing**: Verify frame-based protocol parsing (0xA5 start byte, CRC-8 validation) handles all command types
- [ ] **Add Command Validation**: Validate that received command codes match Matter controller's canonical values
- [ ] **Add Serial Diagnostics**: Serial console logging for received commands and rejected triggers

## Existing Code to Update

**File: `src/uart_controller.h`**
- Line 6-12: `UARTCommand` enum needs expansion to include all 12 states
- Line 24-27: Command code constants need updating to match Matter controller codes (0x01-0x0C)

**File: `src/uart_controller.cpp`**
- Line 79-87: `commandFromByte()` function needs to map all 12 command codes

**File: `src/main.cpp`**
- Line 394: `uartController->update()` is called but commands aren't processed
- Line 453-473: `handleUARTCommand()` exists but isn't called from main loop
- Line 87-92: `DeathState` enum only has 4 states, needs expansion per story-003a

## Notes
- Coordinate UART pin assignments with thermal printer to avoid conflicts (currently TX=21, RX=20)
- Ensure logging verbosity is safe for production once validation completes
- Busy policy must handle both trigger commands and state forcing commands appropriately
