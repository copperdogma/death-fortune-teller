# Story: Capacitive Finger Detection & Snap Flow

**Status**: To Do

---

## Related Requirement
[docs/spec.md §3 State Machine — Fortune Flow](../spec.md#3-state-machine-runtime)

## Alignment with Design
[docs/hardware.md §Current Pin Assignments](../hardware.md#current-pin-assignments) - GPIO4 for capacitive sensor
[docs/spec.md §8 Config.txt Keys — Cap Sense & Timing](../spec.md#8-configtxt-keys)

## Acceptance Criteria
- Capacitive sensor calibrates on boot and detects solid finger presence for ≥120ms before triggering snap timer
- Mouth LED behavior matches prompt/pulse sequence, snap occurs 1–3s after detection even if finger withdraws
- Fortune flow aborts cleanly after 6s timeout with mouth closure and state reset to IDLE
- Snap action triggers servo closure and fortune generation/printing
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Fix ESP32-S3 Compatibility**: Update detection logic for ESP32-S3 (values increase when touched)
- [ ] **Implement Snap Action**: Add servo snap (mouth closure) and fortune generation
- [ ] **Add Timeout Handling**: Implement 6-second timeout with mouth closure and state reset
- [ ] **Integrate Mouth LED**: Add LED pulse during finger wait, sync with states
- [ ] **Add Configuration**: Make thresholds configurable via config.txt
- [ ] **Test Hardware**: Validate with actual ESP32-S3 and copper electrodes

## Technical Implementation Details

### ESP32-S3 Compatibility Requirements

**Touch Sensor Behavior:**
- ESP32-S3 capacitive touch values INCREASE when touched (opposite of ESP32)
- Detection logic must be inverted from ESP32 implementation
- Threshold calibration must account for ESP32-S3 behavior
- Boot-time calibration must work with ESP32-S3 touch characteristics

**Calibration Requirements:**
- Perform calibration with mouth open and electronics quiet
- Establish baseline threshold for each hardware build
- Store calibration values for consistent operation
- Handle calibration failures gracefully with sensible defaults

### Finger Detection Requirements

**Detection Logic:**
- Require solid finger presence for ≥120ms before triggering snap timer
- Implement debounce to prevent false triggers from brief touches
- Support configurable detection threshold per skull hardware
- Handle sensor noise and environmental interference

**Timing Requirements:**
- 6-second maximum wait time for finger detection
- Random snap delay of 1-3 seconds after finger detection
- Snap occurs even if finger is withdrawn during delay
- Clean timeout handling with mouth closure

### Snap Action Requirements

**Servo Control:**
- Snap action triggers servo to closed position (servo minimum degrees)
- Ensure servo safety limits are respected during snap
- Maintain servo position after snap until fortune flow completes
- Handle servo failures gracefully without system crash

**Fortune Generation:**
- Generate fortune immediately after snap action
- Print fortune while continuing audio playback
- Handle fortune generation failures gracefully
- Maintain audio continuity during printing process

### Mouth LED Requirements

**LED Behavior:**
- Solid LED during finger prompt phase
- Pulsing LED while waiting for finger detection
- LED off during snap delay and fortune flow
- Configurable LED brightness and pulse timing

**State Integration:**
- LED behavior synchronized with state machine transitions
- LED off during non-fortune states
- Handle LED control failures without affecting main flow
- Support LED brightness configuration

### Configuration Requirements

**Configurable Parameters:**
- Capacitive sensor threshold (tunable per skull)
- Finger detection duration (minimum solid detection time)
- Finger wait timeout (maximum wait time)
- Snap delay range (minimum and maximum delay)
- LED brightness and pulse timing

**Hardware-Specific Settings:**
- Pin assignments hardcoded in firmware (not configurable)
- Threshold values configurable per skull hardware
- Timing values configurable for different use cases
- Default values for all configurable parameters

### Error Handling and Recovery

**Sensor Failures:**
- Handle capacitive sensor hardware failures
- Fall back to timeout behavior when sensor fails
- Log sensor errors for debugging and maintenance
- Continue system operation with reduced functionality

**Servo Failures:**
- Handle servo hardware failures gracefully
- Log servo errors for debugging
- Continue audio flow without servo motion
- Maintain system operation with audio-only mode

**Timeout Handling:**
- Clean timeout after 6-second finger wait
- Close mouth and reset state on timeout
- Log timeout events for debugging
- Return to IDLE state after timeout

### Integration Requirements

**State Machine Integration:**
- Integrate with existing state machine transitions
- Maintain existing timing and trigger behavior
- Support all existing state transitions
- Preserve busy policy and debounce logic

**Hardware Integration:**
- Integrate with existing servo control system
- Integrate with existing LED control system
- Integrate with existing thermal printer system
- Maintain existing hardware performance and reliability

**Audio Integration:**
- Maintain audio continuity during snap action
- Support concurrent audio and printing operations
- Handle audio timing during fortune flow
- Preserve existing audio quality and synchronization

## Notes
- **Critical**: ESP32-S3 touch behavior is opposite to ESP32 - values INCREASE when touched
- Coordinate with Story 005 (Thermal Printer) for fortune generation
- Test with actual copper electrodes and ESP32-S3 hardware
- Consider fallback behavior when finger sensor fails
- Ensure proper servo safety limits during snap action
- Capture empirical threshold values for different hardware builds in config guidance
- Ensure timers reuse the shared scheduler/loop primitives to avoid drift

## Dependencies
- Story 005 (Thermal Printer Fortune Output) - Fortune generation and printing
- ESP32-S3 hardware with copper electrodes
- Actual fortune content (external dependency)

## Testing Strategy
1. **Hardware Testing**: Validate ESP32-S3 touch sensor behavior with copper electrodes
2. **Integration Testing**: Test complete fortune flow with snap action
3. **Timeout Testing**: Verify 6-second timeout behavior
4. **LED Testing**: Confirm mouth LED synchronization with states
