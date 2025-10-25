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

### State Transition Logic

**Trigger Handling:**
- FAR trigger: Only accepted in IDLE state, starts welcome sequence
- NEAR trigger: Only accepted in WAIT_FOR_NEAR state, starts finger prompt
- Busy policy: Drop all triggers while system is active
- Debounce: 2-second debounce for duplicate triggers

**State Transitions:**
- IDLE → FAR → PLAY_WELCOME → (complete) → WAIT_FOR_NEAR
- WAIT_FOR_NEAR → NEAR → PLAY_FINGER_PROMPT → (complete) → MOUTH_OPEN_WAIT_FINGER
- MOUTH_OPEN_WAIT_FINGER: Finger detected → FINGER_DETECTED → SNAP_WITH_FINGER → FORTUNE_FLOW
- MOUTH_OPEN_WAIT_FINGER: No finger (timeout) → SNAP_NO_FINGER → FORTUNE_FLOW
- FORTUNE_FLOW: Complete fortune sequence with template selection
- FORTUNE_COMPLETE → FORTUNE_DONE → (goodbye skit) → COOLDOWN → IDLE

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
- Handle TRIGGER_FAR and TRIGGER_NEAR commands
- Implement busy policy and debounce logic
- Log all state transitions for debugging

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

## Notes
- **State Machine**: Complete implementation of all states from spec.md §3
- **Busy Policy**: Drop all commands while system is active
- **Debounce**: 2-second debounce for duplicate triggers
- **Timer Management**: Handle all timing requirements
- **Integration**: Works with UART triggers and finger detection
- **Logging**: Comprehensive state transition logging for debugging

## Dependencies
- Story 003 (Matter UART Trigger Handling) - UART command integration
- Story 004 (Capacitive Finger Detection) - Finger detection integration
- Story 005 (Thermal Printer Fortune Output) - Fortune printing integration
- Story 001a (Configuration Management) - Timer configuration values

## **📋 State Flow (Actual Flow)**

1. **IDLE** → TRIGGER_FAR → **PLAY_WELCOME** → (complete) → **WAIT_FOR_NEAR**
2. **WAIT_FOR_NEAR** → TRIGGER_NEAR → **PLAY_FINGER_PROMPT** → (complete) → **MOUTH_OPEN_WAIT_FINGER**
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
