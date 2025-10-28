# Story: Thermal Printer Fortune Output

**Status**: In Progress

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
- [x] **Integrate Components**: Connect FortuneGenerator and ThermalPrinter to main.cpp
- [x] **Implement Snap Action**: Add fortune generation and printing to handleFortuneFlow()
- [ ] **Implement Bitmap Logo**: Add bitmap logo printing from PoC implementation
- [x] **Add Configuration**: Load fortune file path from config.txt
- [ ] **Test Integration**: Verify complete fortune flow with snap action
- [ ] **Document Requirements**: SD card fortune file requirements and maintenance workflow

## Technical Implementation Details

### Component Integration Requirements

**Fortune Generator Integration:**
- Load fortune templates from JSON file on SD card
- Validate JSON format and required fields on startup
- Generate random fortunes from templates using word lists
- Handle fortune generation failures gracefully
- Support configurable fortune file path via config.txt

**Thermal Printer Integration:**
- Initialize printer with correct baud rate and settings
- Handle printer initialization failures gracefully
- Support concurrent printing and audio playback
- Maintain printer performance during extended use
- Handle printer hardware failures without system crash

**Snap Action Integration:**
- Trigger fortune generation immediately after snap action
- Print fortune while continuing audio playback
- Maintain audio continuity during printing process
- Handle printing failures gracefully
- Complete fortune flow regardless of printing success

### Fortune Generation Requirements

**JSON File Validation:**
- Validate fortune file format on system startup
- Check for required fields: version, templates, wordlists
- Validate all template tokens exist in wordlists
- Handle malformed JSON files gracefully
- Provide meaningful error messages for validation failures

**Fortune Generation Logic:**
- Randomly select template from available templates
- Fill template tokens with random words from wordlists
- Generate unique fortunes for each session
- Handle empty or missing wordlists gracefully
- Support multiple fortune templates per session

**Template Processing:**
- Parse template strings for token replacement
- Replace {{token}} placeholders with random words
- Handle missing tokens gracefully
- Ensure generated fortunes are readable and appropriate
- Support complex template structures

### Thermal Printer Requirements

**Printer Initialization:**
- Initialize printer with correct baud rate and settings
- Handle printer connection failures gracefully
- Support printer reconnection if connection lost
- Maintain printer settings across system restarts
- Log printer initialization status for debugging

**Printing Operations:**
- Print logo before fortune text
- Print fortune text with proper formatting
- Handle paper out and other printer errors
- Support concurrent printing and audio operations
- Maintain print quality during extended use

**Bitmap Logo Support:**
- Load bitmap logo from SD card
- Support 1-bit bitmap format (384px wide)
- Handle missing logo files gracefully
- Fall back to text logo if bitmap unavailable
- Maintain logo print quality and alignment

### Audio Integration Requirements

**Concurrent Operations:**
- Maintain audio playback during printing operations
- Prevent audio underruns during printing
- Support jaw movement during printing
- Handle audio timing during fortune flow
- Preserve audio quality during concurrent operations

**Timing Coordination:**
- Ensure skit duration covers printing time
- Coordinate audio and printing completion
- Handle timing mismatches gracefully
- Support variable printing times
- Maintain system responsiveness during printing

### Error Handling and Recovery

**Printer Failures:**
- Handle printer hardware failures gracefully
- Continue audio flow without printing
- Log printer errors for debugging
- Support printer recovery and reconnection
- Maintain system operation with audio-only mode

**Fortune Generation Failures:**
- Handle JSON parsing errors gracefully
- Fall back to default fortune if generation fails
- Log fortune generation errors for debugging
- Continue system operation with fallback content
- Support manual fortune entry if needed

**File Access Errors:**
- Handle SD card access issues gracefully
- Retry file operations when appropriate
- Log file access errors for debugging
- Continue with available content when possible
- Support offline operation with cached content

### Configuration Requirements

**Configurable Parameters:**
- Fortune file path (configurable via config.txt)
- Logo file path (configurable via config.txt)
- Printer baud rate (configurable via config.txt)
- Print quality settings (configurable via config.txt)
- Audio timing coordination (configurable via config.txt)

**Hardware-Specific Settings:**
- Pin assignments hardcoded in firmware (not configurable)
- Printer settings configurable per hardware build
- Default values for all configurable parameters
- Support for different printer models and settings

### Integration Requirements

**State Machine Integration:**
- Integrate with existing state machine transitions
- Maintain existing timing and trigger behavior
- Support all existing state transitions
- Preserve busy policy and debounce logic

**Hardware Integration:**
- Integrate with existing servo control system
- Integrate with existing audio playback system
- Integrate with existing SD card management
- Maintain existing hardware performance and reliability

**Audio Integration:**
- Maintain audio continuity during printing
- Support concurrent audio and printing operations
- Handle audio timing during fortune flow
- Preserve existing audio quality and synchronization

## Notes
- **2025 MVP**: Single fortune file (`little_kid_fortunes.json`) only
- **Hardware**: Use GPIO4/5 per [hardware.md](hardware.md) - no configuration needed
- **Logo**: Implement bitmap logo from PoC, fallback to text logo if file missing
- **Error Handling**: Ignore printer failures, continue with audio-only
- **Audio Timing**: Ensure skit is long enough to cover printing time (test during integration)
- Monitor printer power draw; record any mitigation (caps, wiring) required for reliable output
- Plan for future mode-specific fortune sets once JSON format stabilizes
## Current Progress

- Fortune generation now loads mad-libs templates from `/fortunes/little_kid_fortunes.json`, validates required wordlists, and outputs fully populated fortunes (see story-003a build log dated 2025-10-28).
- Thermal printer is integrated with the state machine; fortunes print during `FORTUNE_FLOW` and the serial console echoes the complete text.
- Remaining scope for this story focuses on richer printer handling (bitmap logo, diagnostics, broader testing) and documentation of the SD fortune workflow.
