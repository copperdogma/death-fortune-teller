# Story: LED Control System & Fault Indicators

**Status**: To Do

---

## Related Requirement
[docs/spec.md §7 Peripherals & Signals](../spec.md#7-peripherals--signals) - Mouth LED behavior and fault indicators
[docs/spec.md §10 Test Console & Diagnostics](../spec.md#10-test-console--diagnostics) - LED fault cues

## Alignment with Design
[docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime) - LED behavior during fortune flow

## Acceptance Criteria
- Mouth LED provides visual feedback during fortune flow (solid during prompt, pulse during finger wait)
- LED fault indicators provide clear visual feedback for system errors (3 quick blinks for printer faults)
- LED control integrates seamlessly with state machine transitions
- Capacative sensor tuning adjustments
  - Blink Twice: have the mouth LED blink twice any time it detects a finger. This will help us see if the sensor is working properly. If it's just blinking twice all the time with no finger, it needs to be recalibrated.
  - Manual recalibration: if I tap the capacative sensor twice quickly, it will blink the mouth LED three times, wait 5 seconds, then calibrate, then blink the mouth LED four times to say it's done.
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Implement LED Control System**: Create LED control system
- [ ] **Add State-Based LED Behavior**: Integrate LED behavior with state machine transitions
- [ ] **Add Fault Indicators**: Implement LED fault indicators for system errors
- [ ] **Add Capacitive Sensor Tuning**: Implement LED feedback for capacitive sensor testing and calibration
- [ ] **Test Integration**: Verify LED behavior works with state machine and fault detection

## Technical Implementation Details

### LED Control Requirements

**Mouth LED Behavior:**
- Solid LED during finger prompt phase (maximum brightness)
- Pulsing LED while waiting for finger detection (brightness cycling)
- LED off during snap delay and fortune flow
- LED off during non-fortune states

**LED State Integration:**
- LED behavior synchronized with state machine transitions
- LED off during IDLE, COOLDOWN, and non-fortune states
- LED active only during FORTUNE_FLOW states
- Handle LED control failures without affecting main flow
- Support LED brightness configuration per state

### Fault Indicator Requirements

**Printer Fault Indicators:**
- 3 quick blinks for printer hardware failures
- 3 quick blinks for printer communication errors
- 3 quick blinks for printer paper out conditions
- Clear visual distinction from normal LED behavior
- Fault indicators override normal LED behavior

**System Fault Indicators:**
- LED patterns for different system error types
- Visual feedback for hardware failures
- Clear error indication without audio dependency
- Fault indicators persist until system reset or error cleared
- Support for multiple fault types with different patterns

### Capacitive Sensor Tuning Requirements

**Sensor Detection Feedback:**
- 2 quick blinks whenever capacitive sensor detects a finger
- Immediate visual feedback for sensor testing and debugging
- Clear indication that sensor is responding to touch
- Helps identify false positives or sensor calibration issues

**Manual Calibration Mode:**
- Triggered by double-tap on capacitive sensor (two quick touches)
- 3 quick blinks to indicate calibration mode start
- 5-second wait period for sensor stabilization
- Automatic sensor recalibration process
- 4 quick blinks to indicate calibration completion
- Clear visual feedback for calibration success/failure

**Calibration Integration:**
- Integrate with existing capacitive sensor calibration system
- Maintain existing sensor sensitivity and threshold settings
- Support calibration without affecting normal operation
- Handle calibration failures gracefully with appropriate LED feedback


### Error Handling and Recovery

**LED Control Failures:**
- Handle LED hardware failures gracefully
- Continue system operation without LED feedback
- Log LED control errors for debugging
- Support LED recovery and reconnection
- Maintain system operation with reduced visual feedback

**Fault Detection:**
- Detect system faults and trigger appropriate LED patterns
- Clear fault indicators when errors are resolved
- Handle multiple simultaneous faults gracefully
- Log fault events for debugging
- Support fault clearing via serial console

### Integration Requirements

**State Machine Integration:**
- Integrate with existing state machine transitions
- Maintain existing timing and trigger behavior
- Support all existing state transitions
- Preserve busy policy and debounce logic

**Hardware Integration:**
- Integrate with existing GPIO control system
- Integrate with existing fault detection system
- Integrate with existing configuration management
- Maintain existing hardware performance and reliability

**Audio Integration:**
- LED behavior coordinated with audio playback
- Visual feedback during audio operations
- Handle LED timing during concurrent operations
- Preserve existing audio quality and synchronization

## Notes
- **LED Pin Assignments**: All pin assignments are hardcoded in firmware and not configurable
- **Fault Indicators**: LED fault indicators provide visual feedback when audio is not available
- **State Synchronization**: LED behavior must be perfectly synchronized with state machine transitions
- **Error Recovery**: LED system must handle failures gracefully without affecting main system operation

## Dependencies
- Story 003a (State Machine Implementation) - State machine integration
- Story 005 (Thermal Printer Fortune Output) - Fault detection integration
- Story 001a (Configuration Management) - Configuration system integration

## Testing Strategy
1. **LED Behavior Testing**: Test LED behavior during all state transitions
2. **Fault Indicator Testing**: Test LED fault indicators for various system errors
3. **Integration Testing**: Test LED integration with state machine and fault detection
4. **Error Handling Testing**: Test LED system behavior during hardware failures
