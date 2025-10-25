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
- LED brightness and timing are configurable via config.txt
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Implement LED Control System**: Create LED control system with brightness and timing control
- [ ] **Add State-Based LED Behavior**: Integrate LED behavior with state machine transitions
- [ ] **Add Fault Indicators**: Implement LED fault indicators for system errors
- [ ] **Add Configuration**: Make LED settings configurable via config.txt
- [ ] **Test Integration**: Verify LED behavior works with state machine and fault detection

## Technical Implementation Details

### LED Control Requirements

**Mouth LED Behavior:**
- Solid LED during finger prompt phase (maximum brightness)
- Pulsing LED while waiting for finger detection (brightness cycling)
- LED off during snap delay and fortune flow
- LED off during non-fortune states
- Configurable LED brightness levels and pulse timing

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

### Configuration Requirements

**Configurable Parameters:**
- LED brightness levels (dim, medium, bright, maximum)
- Pulse timing (pulse frequency, pulse duration)
- Fault indicator patterns (blink count, blink timing)
- LED pin assignments (hardcoded in firmware)
- Default LED behavior settings

**Hardware-Specific Settings:**
- Pin assignments hardcoded in firmware (not configurable)
- LED brightness levels configurable per hardware build
- Default values for all LED parameters
- Support for different LED types and configurations

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
- **Configuration**: LED settings should be configurable for different hardware builds and use cases

## Dependencies
- Story 003a (State Machine Implementation) - State machine integration
- Story 005 (Thermal Printer Fortune Output) - Fault detection integration
- Story 001a (Configuration Management) - Configuration system integration

## Testing Strategy
1. **LED Behavior Testing**: Test LED behavior during all state transitions
2. **Fault Indicator Testing**: Test LED fault indicators for various system errors
3. **Configuration Testing**: Test LED configuration via config.txt
4. **Integration Testing**: Test LED integration with state machine and fault detection
5. **Error Handling Testing**: Test LED system behavior during hardware failures
