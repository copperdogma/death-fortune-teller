# Story: Detailed Fortune Flow Implementation

**Status**: Future Work (2026)
**Priority**: Medium

---

## Related Requirement
[docs/spec.md §3 State Machine — Runtime](../spec.md#3-state-machine-runtime) - Fortune flow sub-states
[docs/spec.md §2 Modes, Triggers, and Behavior](../spec.md#2-modes-triggers-and-behavior) - Fortune generation and template selection

## Alignment with Design
This story extends the 2025 MVP state machine with detailed fortune flow sub-states that were deferred due to time constraints.

## Acceptance Criteria
- FORTUNE_FLOW state expanded into detailed sub-states
- Template selection and skit playback integration
- Fortune generation and printing during audio playback
- Complete fortune flow sequence with proper timing
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Expand FORTUNE_FLOW State**: Break down into detailed sub-states
- [ ] **Implement Template Selection**: Randomly choose fortune template from available templates
- [ ] **Add Template Skit Playback**: Play skit corresponding to chosen template
- [ ] **Integrate Fortune Generation**: Generate random fortune from selected template
- [ ] **Add Fortune Told Audio**: Play "fortune told" skit after generation
- [ ] **Implement Concurrent Printing**: Print fortune while playing fortune told audio
- [ ] **Add Fortune Complete State**: Handle completion of entire fortune sequence
- [ ] **Test Integration**: Verify complete fortune flow works end-to-end

## Technical Implementation Details

### Fortune Flow Sub-States

**Required Sub-States:**
- PLAY_FORTUNE_PREAMBLE: Playing fortune preamble audio
- CHOOSE_TEMPLATE: Randomly choose fortune template from available templates
- PLAY_TEMPLATE_SKIT: Play skit corresponding to chosen template
- GENERATE_FORTUNE: Generate random fortune from selected template
- PLAY_FORTUNE_TOLD: Play "fortune told" skit after generation
- PRINT_FORTUNE: Print fortune while playing fortune told audio
- FORTUNE_COMPLETE: Fortune flow complete, ready for transition to FORTUNE_DONE

### Template Selection Requirements

**Template Management:**
- Load fortune templates from `/audio/fortune_templates/` directory
- Support multiple template files (e.g., `template_01.json`, `template_02.json`)
- Random selection from available templates
- Template validation and error handling
- Fallback behavior when no templates available

**Template Structure:**
- Each template contains template text with placeholders
- Support for wordlists and token replacement
- Integration with existing `FortuneGenerator` class
- Template-specific audio skits

### Fortune Generation Integration

**Generation Process:**
- Use selected template to generate fortune text
- Apply random word selection from wordlists
- Validate generated fortune length and content
- Handle generation errors gracefully
- Support multiple fortune formats

**Concurrent Operations:**
- Generate fortune while playing template skit
- Print fortune while playing fortune told audio
- Maintain jaw movement during printing
- Handle audio/printing timing coordination

### Audio Integration

**Template Skits:**
- Play skit corresponding to selected template
- Support template-specific audio files
- Handle missing template skits gracefully
- Maintain audio quality and timing

**Fortune Told Audio:**
- Play "fortune told" stinger after generation
- Coordinate with fortune printing
- Support multiple fortune told variations
- Handle audio completion events

### Printing Integration

**Thermal Printer Control:**
- Print generated fortune during fortune told audio
- Support formatted fortune output
- Handle printer errors and retries
- Maintain print quality and timing

**Concurrent Printing:**
- Print while playing fortune told audio
- Continue jaw movement during printing
- Handle printing completion events
- Coordinate with audio playback timing

### State Machine Integration

**State Transitions:**
- FORTUNE_FLOW → PLAY_FORTUNE_PREAMBLE → CHOOSE_TEMPLATE → PLAY_TEMPLATE_SKIT → GENERATE_FORTUNE → PLAY_FORTUNE_TOLD → PRINT_FORTUNE → FORTUNE_COMPLETE
- Maintain existing state machine structure
- Preserve all existing timing and trigger behavior
- Support state forcing commands for debugging

**Error Handling:**
- Handle missing templates gracefully
- Handle generation errors
- Handle printing errors
- Continue flow with fallback behavior

### Configuration Requirements

**Template Configuration:**
- Make template directory configurable
- Support custom template file naming
- Handle template validation
- Provide sensible defaults

**Timing Configuration:**
- Configurable timing for each sub-state
- Support for template skit duration
- Configurable printing timing
- Handle timing coordination

## Dependencies
- Story 003a (State Machine Implementation) - Base state machine
- Story 005 (Thermal Printer Fortune Output) - Printing functionality
- Story 002b (Skit Category Support) - Audio directory structure
- Fortune templates and audio files (external dependency)

## Notes
- **2026 Enhancement**: This extends the 2025 MVP with detailed fortune flow
- **Backward Compatibility**: Must maintain compatibility with existing state machine
- **Performance**: Ensure fortune generation doesn't block audio playback
- **Error Handling**: Robust error handling for all sub-states
- **Testing**: Comprehensive testing of complete fortune flow sequence

## Testing Strategy
1. **Sub-State Transitions**: Test all fortune flow sub-state transitions
2. **Template Selection**: Test random template selection and validation
3. **Fortune Generation**: Test fortune generation from templates
4. **Concurrent Operations**: Test printing during audio playback
5. **Error Handling**: Test error scenarios and fallback behavior
6. **End-to-End**: Test complete fortune flow from start to finish
7. **Performance**: Test timing and performance under load
8. **Integration**: Test with actual hardware components

## Future Considerations
- Support for multiple fortune languages
- Dynamic template loading from SD card
- Template customization via web interface
- Advanced fortune generation algorithms
- Integration with external fortune APIs
