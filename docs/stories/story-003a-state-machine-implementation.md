# Story: State Machine Implementation

**Status**: To Do

---

## Related Requirement
[docs/spec.md §3 State Machine — Runtime](../spec.md#3-state-machine-runtime)

## Alignment with Design
[docs/spec.md §2 Modes, Triggers, and Behavior](../spec.md#2-modes-triggers-and-behavior) - State machine controls system behavior

## Acceptance Criteria
- Complete state machine implementation with all states from spec.md §3
- State transitions triggered by UART commands and internal events
- Busy policy enforced (drop all commands while active)
- Debounce logic prevents duplicate triggers within 2 seconds
- Timer management for finger detection, snap delay, and timeouts
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Implement State Machine**: Create complete state machine with all states and transitions
- [ ] **Add State Transitions**: Implement UART command handling and internal event transitions
- [ ] **Implement Busy Policy**: Drop all commands while system is active
- [ ] **Add Debounce Logic**: Prevent duplicate triggers within 2 seconds
- [ ] **Add Timer Management**: Handle finger detection, snap delay, and timeout timers
- [ ] **Add State Logging**: Log all state transitions for debugging
- [ ] **Test Integration**: Verify state machine works with UART triggers and finger detection

## Technical Implementation Details

### State Machine Requirements

**Main States:**
- IDLE: System ready for triggers
- PLAY_WELCOME: Playing welcome skit (FAR sensor)
- WAIT_FOR_NEAR: Waiting for NEAR sensor after welcome
- PLAY_FINGER_PROMPT: Playing "put finger in my mouth" skit
- MOUTH_OPEN_WAIT_FINGER: Mouth open, waiting for finger
- FINGER_DETECTED: Finger detected, waiting for snap delay
- SNAP_WITH_FINGER: Snap with finger (play finger snap skit)
- SNAP_NO_FINGER: Snap without finger (play no finger skit)
- FORTUNE_FLOW: Fortune flow sequence
- FORTUNE_DONE: Fortune complete, play goodbye skit
- COOLDOWN: Post-fortune cooldown period

**Fortune Flow Sub-States:**
- PLAY_FORTUNE_PREAMBLE: Playing fortune preamble
- CHOOSE_TEMPLATE: Randomly choose fortune template
- PLAY_TEMPLATE_SKIT: Play skit for chosen template
- GENERATE_FORTUNE: Generate random fortune from template
- PLAY_FORTUNE_TOLD: Play "fortune told" skit
- PRINT_FORTUNE: Print fortune while talking
- FORTUNE_COMPLETE: Fortune flow complete

### Matter Controller State Mapping

**Requirement**: Death's state machine must align with Matter controller's command codes and state definitions.

**Command Code to State Mapping**:
- `0x01` (CMD_FAR_MOTION) → Trigger FAR_MOTION_DETECTED event
- `0x02` (CMD_NEAR_MOTION) → Trigger NEAR_MOTION_DETECTED event  
- `0x03` (CMD_PLAY_WELCOME) → Force transition to PLAY_WELCOME state
- `0x04` (CMD_WAIT_FOR_NEAR) → Force transition to WAIT_FOR_NEAR state
- `0x05` (CMD_PLAY_FINGER_PROMPT) → Force transition to PLAY_FINGER_PROMPT state
- `0x06` (CMD_MOUTH_OPEN_WAIT_FINGER) → Force transition to MOUTH_OPEN_WAIT_FINGER state
- `0x07` (CMD_FINGER_DETECTED) → Force transition to FINGER_DETECTED state
- `0x08` (CMD_SNAP_WITH_FINGER) → Force transition to SNAP_WITH_FINGER state
- `0x09` (CMD_SNAP_NO_FINGER) → Force transition to SNAP_NO_FINGER state
- `0x0A` (CMD_FORTUNE_FLOW) → Force transition to FORTUNE_FLOW state
- `0x0B` (CMD_FORTUNE_DONE) → Force transition to FORTUNE_DONE state
- `0x0C` (CMD_COOLDOWN) → Force transition to COOLDOWN state

**State Forcing Behavior**:
- Matter controller can force Death into any state via UART command
- State forcing commands (0x03-0x0C) may override busy state (Matter controller has priority)
- Trigger commands (0x01-0x02) respect busy state and debounce logic
- Death must validate forced state transitions (e.g., don't allow invalid sequences)

### State Transition Logic

**Trigger Handling:**
- FAR trigger (0x01): Only accepted in IDLE state, starts welcome sequence
- NEAR trigger (0x02): **CRITICAL** - Only accepted in WAIT_FOR_NEAR state, starts finger prompt
  - This means after FAR_MOTION_DETECTED → PLAY_WELCOME, system must be in WAIT_FOR_NEAR before accepting NEAR_MOTION_DETECTED
  - Any NEAR_MOTION_DETECTED received outside WAIT_FOR_NEAR state must be dropped
- State forcing (0x03-0x0C): Matter controller can force transitions even when busy
- Busy policy: Drop trigger commands while system is active (but allow forced state transitions)
- Debounce: 2-second debounce for duplicate trigger commands

**State Transitions:**
- IDLE → FAR → PLAY_WELCOME → (complete) → WAIT_FOR_NEAR
- WAIT_FOR_NEAR → NEAR → PLAY_FINGER_PROMPT → (complete) → MOUTH_OPEN_WAIT_FINGER
- MOUTH_OPEN_WAIT_FINGER: Finger detected → FINGER_DETECTED → SNAP_WITH_FINGER → FORTUNE_FLOW
- MOUTH_OPEN_WAIT_FINGER: No finger (timeout) → SNAP_NO_FINGER → FORTUNE_FLOW
- FORTUNE_FLOW: Complete fortune sequence with template selection
- FORTUNE_COMPLETE → FORTUNE_DONE → (goodbye skit) → COOLDOWN → IDLE

**Critical State Constraints:**
- NEAR_MOTION_DETECTED can ONLY be processed in WAIT_FOR_NEAR state
- This enforces the proper sequence: FAR → WELCOME → WAIT_FOR_NEAR → NEAR → FINGER_PROMPT
- Any NEAR_MOTION_DETECTED received in other states (IDLE, PLAY_WELCOME, etc.) must be dropped

### Timer Management

**Required Timers:**
- Finger wait timeout: 6 seconds maximum
- Snap delay: Random 1-3 seconds after finger detection
- Cooldown period: 12 seconds after fortune complete

**Timer Behavior:**
- Finger wait: Start when mouth opens, timeout if no finger detected
- Snap delay: Start when finger detected, random duration
- Cooldown: Start when fortune complete, prevent new triggers

### Integration Requirements

**UART Integration:**
- Handle all 12 command codes from Matter controller
- Support both trigger-based (0x01-0x02) and state-forcing (0x03-0x0C) commands
- Implement busy policy with exceptions for state forcing
- Log all state transitions for debugging
- Map Matter controller command codes to Death's internal state enum

**Finger Detection Integration:**
- Monitor capacitive sensor during MOUTH_OPEN_WAIT_FINGER state
- Trigger state transitions based on finger detection
- Handle timeout scenarios

**Audio Integration:**
- Play appropriate skits for each state
- Continue jaw movement during printing
- Handle audio completion events

**Hardware Integration:**
- Control servo for mouth open/close
- Control LED for visual feedback
- Control thermal printer for fortune output

## Existing Code to Update

**File: `src/main.cpp`**
- Line 87-92: `DeathState` enum needs expansion from 4 states to 11+ states
- Line 453-473: `handleUARTCommand()` needs to handle all 12 command codes, not just FAR/NEAR
- Line 455-458: Busy policy needs to allow state forcing commands even when busy
- Line 422-442: State machine switch statement needs cases for all states
- Line 534-557: `handleFortuneFlow()` is incomplete stub, needs full implementation

**File: `src/uart_controller.h`**
- Line 6-12: `UARTCommand` enum needs expansion for all states

## Notes
- **State Machine**: Complete implementation of all states from spec.md §3
- **Busy Policy**: Drop trigger commands while system is active, but allow state forcing commands
- **Debounce**: 2-second debounce for duplicate trigger commands (0x01-0x02)
- **Timer Management**: Handle all timing requirements
- **Integration**: Works with UART triggers and finger detection
- **Logging**: Comprehensive state transition logging for debugging
- **Critical Constraint**: NEAR_MOTION_DETECTED can ONLY be processed in WAIT_FOR_NEAR state (per spec.md §3)
- **Matter Controller Alignment**: All state names and command codes must match Matter controller's canonical definitions

## Dependencies
- Story 003 (Matter UART Trigger Handling) - UART command integration
- Story 004 (Capacitive Finger Detection) - Finger detection integration
- Story 005 (Thermal Printer Fortune Output) - Fortune printing integration
- Story 001a (Configuration Management) - Timer configuration values

## **📋 State Flow (Actual Flow)**

1. **IDLE** → FAR_MOTION_DETECTED → **PLAY_WELCOME** → (complete) → **WAIT_FOR_NEAR**
2. **WAIT_FOR_NEAR** → NEAR_MOTION_DETECTED → **PLAY_FINGER_PROMPT** → (complete) → **MOUTH_OPEN_WAIT_FINGER**
3. **MOUTH_OPEN_WAIT_FINGER**:
   - Finger detected → **FINGER_DETECTED** → (snap delay) → **SNAP_WITH_FINGER** → (play finger snap skit) → **FORTUNE_FLOW**
   - No finger (timeout) → **SNAP_NO_FINGER** → (play no finger skit) → **FORTUNE_FLOW**
4. **FORTUNE_FLOW**: PLAY_FORTUNE_PREAMBLE → CHOOSE_TEMPLATE → PLAY_TEMPLATE_SKIT → GENERATE_FORTUNE → PLAY_FORTUNE_TOLD → PRINT_FORTUNE → FORTUNE_COMPLETE
5. **FORTUNE_COMPLETE** → **FORTUNE_DONE** → (play goodbye skit) → **COOLDOWN** → **IDLE**

## Testing Strategy
1. **State Transitions**: Test all state transitions with UART commands
2. **Busy Policy**: Verify commands are dropped while system is busy
3. **Debounce**: Test duplicate trigger handling
4. **Timer Management**: Test all timing requirements
5. **Integration**: Test with actual hardware components
