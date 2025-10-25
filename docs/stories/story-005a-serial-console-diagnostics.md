# Story: Serial Console & Diagnostics

**Status**: To Do

---

## Related Requirement
[docs/spec.md §10 Test Console & Diagnostics](../spec.md#10-test-console--diagnostics) - Serial test console commands
[docs/spec.md §14 Acceptance Checklist](../spec.md#14-acceptance-checklist-mvp) - Serial test console functionality

## Alignment with Design
[docs/spec.md §10 Test Console & Diagnostics](../spec.md#10-test-console--diagnostics) - Baseline console commands

## Acceptance Criteria
- Serial console provides all baseline test commands from spec.md §10
- Remote debug console (telnet) provides comprehensive system diagnostics
- Console commands work reliably for testing and troubleshooting
- LED fault indicators provide visual feedback for system errors
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Add Missing Serial Commands**: Implement remaining baseline commands from spec.md
- [ ] **Enhance Remote Debug Console**: Add missing diagnostic commands to telnet console
- [ ] **Add Hardware Test Commands**: Implement hardware testing commands for servo, LED, printer
- [ ] **Add State Machine Commands**: Add commands for testing state machine transitions
- [ ] **Test Console Integration**: Verify all console commands work with actual hardware

## Technical Implementation Details

### Current Implementation Status

**Already Implemented (Remote Debug Console):**
- `status` - System status (WiFi, OTA, streaming, log buffer)
- `wifi` - WiFi connection information
- `ota` - OTA status and configuration
- `log` - Rolling log buffer dump
- `startup` - Startup log buffer dump
- `head N` / `tail N` - Log entry viewing
- `stream on|off` - Live streaming toggle
- `bluetooth on|off|status` - Bluetooth controller management
- `reboot` - System restart
- `help` - Command help

**Missing Baseline Commands (from spec.md §10):**
- `play welcome` - Test welcome skit playback
- `play fortune` - Test fortune skit playback
- `mouth open` / `mouth close` - Test servo control
- `print test` - Test thermal printer
- `calibrate cap` - Test capacitive sensor calibration
- `set mode <0..3>` - Test mode switching (future)
- `show status` - System status display

### Serial Console Requirements

**Baseline Test Commands:**
- **Audio Testing**: Commands to test welcome and fortune skit playback
- **Hardware Testing**: Commands to test servo, LED, and printer hardware
- **Sensor Testing**: Commands to test capacitive sensor calibration and operation
- **State Testing**: Commands to test state machine transitions and timing
- **System Testing**: Commands to display system status and diagnostics

**Command Implementation:**
- All commands available via both serial console and remote debug (telnet)
- Commands provide clear feedback and error messages
- Commands handle hardware failures gracefully
- Commands support testing without affecting normal operation
- Commands provide detailed diagnostic information

### Hardware Test Commands

**Servo Control Testing:**
- `mouth open` - Open mouth to maximum position
- `mouth close` - Close mouth to minimum position
- `mouth <angle>` - Set mouth to specific angle
- `servo test` - Test servo range and responsiveness
- Handle servo failures gracefully with error messages

**LED Control Testing:**
- `led on` - Turn LED on at maximum brightness
- `led off` - Turn LED off
- `led <brightness>` - Set LED to specific brightness
- `led pulse` - Test LED pulsing behavior
- `led fault` - Test fault indicator patterns

**Printer Testing:**
- `print test` - Print test fortune and logo
- `print <text>` - Print custom text
- `printer status` - Check printer connection and status
- Handle printer failures gracefully with error messages

**Capacitive Sensor Testing:**
- `calibrate cap` - Perform capacitive sensor calibration
- `cap test` - Test capacitive sensor responsiveness
- `cap threshold <value>` - Set sensor threshold
- `cap status` - Display sensor status and readings

### State Machine Testing

**State Control Commands:**
- `state <state>` - Force state machine to specific state
- `trigger far` - Simulate FAR sensor trigger
- `trigger near` - Simulate NEAR sensor trigger
- `state status` - Display current state and timing
- `state reset` - Reset state machine to IDLE

**Flow Testing Commands:**
- `test welcome` - Test complete welcome flow
- `test fortune` - Test complete fortune flow
- `test timeout` - Test fortune flow timeout
- `test snap` - Test snap action and fortune generation

### System Diagnostics

**Status Commands:**
- `show status` - Comprehensive system status display
- `hardware status` - Hardware component status
- `audio status` - Audio system status and configuration
- `config status` - Configuration values and validation
- `memory status` - Memory usage and performance metrics

**Diagnostic Commands:**
- `test all` - Run comprehensive system test
- `test audio` - Test audio playback and synchronization
- `test hardware` - Test all hardware components
- `test integration` - Test component integration
- `test performance` - Test system performance and timing

### Error Handling and Recovery

**Command Error Handling:**
- Handle invalid commands gracefully with helpful error messages
- Handle hardware failures during testing without system crash
- Provide clear feedback for command success and failure
- Support command cancellation and recovery
- Log all command execution for debugging

**System Error Handling:**
- Detect and report system errors during testing
- Handle component failures gracefully during diagnostics
- Provide fallback behavior when components are unavailable
- Support system recovery after testing errors
- Maintain system stability during extended testing

### Integration Requirements

**Console Integration:**
- Integrate with existing remote debug system
- Support both serial and telnet console access
- Maintain existing console functionality and commands
- Add new commands without breaking existing functionality
- Support console access during all system states

**Hardware Integration:**
- Integrate with existing hardware control systems
- Support testing without affecting normal operation
- Handle hardware failures gracefully during testing
- Provide comprehensive hardware diagnostics
- Support hardware recovery and reconnection

**State Machine Integration:**
- Integrate with existing state machine system
- Support state testing without breaking normal flow
- Handle state machine errors gracefully
- Provide comprehensive state diagnostics
- Support state machine recovery and reset

## Notes
- **Console Access**: Commands available via both serial console and remote debug (telnet)
- **Testing Safety**: All test commands designed to be safe and non-destructive
- **Hardware Protection**: Commands include safety limits and error handling
- **Diagnostic Value**: Commands provide comprehensive system diagnostics and troubleshooting
- **Integration**: New commands integrate with existing remote debug system

## Dependencies
- Story 003a (State Machine Implementation) - State machine testing commands
- Story 004 (Capacitive Finger Detection) - Sensor testing commands
- Story 005 (Thermal Printer Fortune Output) - Printer testing commands
- Story 004a (LED Control System) - LED testing commands
- Story 001a (Configuration Management) - Configuration testing commands

## Testing Strategy
1. **Command Testing**: Test all console commands with actual hardware
2. **Integration Testing**: Test console integration with all system components
3. **Error Handling Testing**: Test command error handling and recovery
4. **Performance Testing**: Test console performance during system operation
5. **Diagnostic Testing**: Test diagnostic capabilities and troubleshooting value
