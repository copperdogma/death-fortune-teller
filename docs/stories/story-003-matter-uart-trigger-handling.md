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
- Every UART command received from the Matter controller is printed to the serial console for diagnostics
- Commands translate into state machine transitions exactly as defined in story-003a
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
 - [x] **Update UART Command Enum**: Expand `UARTCommand` enum in `src/uart_controller.h` to include all 12 states (currently only has FAR_MOTION_DETECTED, NEAR_MOTION_DETECTED, SET_MODE, PING)
 - [x] **Update Command Code Mapping**: Update `commandFromByte()` in `src/uart_controller.cpp` to map all 12 command codes (currently only maps 0x05, 0x06, 0x02, 0x04)
 - [x] **Add State Forcing Support**: Support direct state forcing (commands 0x03-0x0C) in addition to trigger commands (0x01-0x02)
 - [x] **Wire UART Processing**: Ensure `handleUARTCommand()` is called from main loop (currently `uartController->update()` is called but commands aren't processed)
 - [x] **Implement Frame Parsing**: Verify frame-based protocol parsing (0xA5 start byte, CRC-8 validation) handles all command types
 - [x] **Add Command Validation**: Validate that received command codes match Matter controller's canonical values
 - [x] **Add Serial Diagnostics**: Serial console logging for received commands and rejected triggers

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
- Coordinate UART pin assignments with thermal printer to avoid conflicts (currently TX=21, RX=22)
- Ensure logging verbosity is safe for production once validation completes
- Busy policy must handle both trigger commands and state forcing commands appropriately

---

## Build Log

### 2025-10-27 — Initial assessment
- `UARTController` exists but only exposes four commands (`SET_MODE`, `FAR_MOTION_DETECTED`, `NEAR_MOTION_DETECTED`, `PING`) and hard-codes non-canonical byte values (`src/uart_controller.h`), so the Matter controller’s 0x01–0x0C command map is not represented.
- `commandFromByte()` handles only the same four codes (0x02/0x04/0x05/0x06) and defaults everything else to `NONE`, so state forcing commands and other triggers are silently discarded (`src/uart_controller.cpp`).
- The main loop never calls `uartController->update()` or routes decoded commands into `handleUARTCommand`, so incoming UART frames are currently ignored (`src/main.cpp`).
- `handleUARTCommand` still assumes the two-trigger model and the `DeathState` enum only defines four high-level states, so the Matter-aligned 12-state workflow from story-003a is not implemented (`src/main.cpp`).
- Busy/debounce logic only guards the trigger path (`IDLE` + 2 s cooldown) and does not cover the broader busy policy required once state forcing is supported.

### 2025-10-27 — UART command scaffolding
- Canonical Matter opcodes (0x01–0x0C) are now represented in `UARTCommand`, with helpers for human-readable names and legacy slots preserved for diagnostics (`src/uart_controller.h`).
- `commandFromByte()` maps the new opcode set, logs each received frame to the serial console, and flags unknown values; UART bytes are now read with explicit `Serial1.available()` bounds (`src/uart_controller.cpp`).
- The setup sequence initializes the UART peripheral, and the main loop polls `uartController->update()`, routing decoded commands through `handleUARTCommand` (`src/main.cpp`).
- `handleUARTCommand` now logs the command name, keeps the legacy FAR/NEAR trigger handling, and rejects other commands with warnings pending the full 12-state implementation (`src/main.cpp`).

### 2025-10-27 — UART debugging instrumentation
- Added verbose diagnostics to log raw UART reads, CRC failures, incomplete frames, and missing start bytes so we can correlate Matter controller transmissions with what the skull receives (`src/uart_controller.cpp`).
- Hex sampling of the first few bytes now prints whenever a buffer fails to parse, helping confirm wire activity and frame format during bench tests (`src/uart_controller.cpp`).

### 2025-10-27 — Pin alignment fix
- Updated the firmware and hardware docs to use ESP32-WROVER `GPIO21` (TX) and `GPIO22` (RX) for the Matter UART because this dev board does not expose `GPIO20`; the C3 keeps its canonical `GPIO20/21` pair (`src/uart_controller.h`, `docs/hardware.md`).

### 2025-10-27 — Matter trigger smoke test
- Verified that all Matter controller triggers issued from the Home app propagate through the C3 and are logged on the WROVER’s serial console, confirming the UART link and diagnostics are working end-to-end.
- Confirmed the servo subsystem still operates normally with the new UART routing; thermal printer remains untested until it is connected.

### 2025-10-27 — State machine integration
- Expanded `DeathState` to the full 12-step flow, wired trigger handling with strict busy/debounce policy, and added support for Matter state-forcing commands that can override the busy guard when necessary (`src/main.cpp`).
- Implemented timer-driven finger wait, snap delay, and cooldown management sourced from config values, plus servo/LED updates for each state transition; logging now records every transition and drop reason.
- Kept UART parsing on the single-frame assumption from the controller while adding guard rails so missing audio assets skip forward rather than stalling the flow.

#### Work Checklist
- [x] Extend `UARTCommand` (and related constants) to cover all 12 canonical Matter commands with correct opcode values.
- [x] Expand `commandFromByte()` and frame parsing so every canonical command produces the correct enum, including validation/error logging for unknown values.
- [x] Integrate `UARTController::update()` into the main loop and dispatch decoded commands through `handleUARTCommand`.
- [x] Update `handleUARTCommand` and the state machine to mirror the 12-state flow defined in story-003a, including trigger vs. direct-state handling.
- [x] Implement the required busy/duplicate policy (2 s debounce, drop while active skits/print) once the full state machine is in place.
- [x] Add serial diagnostics around received/rejected commands to aid Matter link debugging.
