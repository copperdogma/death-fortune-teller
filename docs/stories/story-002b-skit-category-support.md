# Story: Skit Category Support & Directory Structure

**Status**: Done

---

## Related Requirement
[docs/spec.md §4 Assets & File Layout](../spec.md#4-assets--file-layout) - Audio directory structure
[docs/spec.md §2 Modes, Triggers, and Behavior](../spec.md#2-modes-triggers-and-behavior) - Welcome vs Fortune flows

## Alignment with Design
[docs/spec.md §11 Bring-Up Plan — Steps 2 & 3](../spec.md#11-bring-up-plan-mvp-in-this-order)

## Acceptance Criteria
- System supports separate skit categories: `/audio/welcome/` and `/audio/fortune/` directories
- Welcome sequence uses weighted random selection from welcome skits (no immediate repeat)
- Fortune flow uses weighted random selection from fortune skits (no immediate repeat)
- State machine properly integrates with skit category selection
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [x] **Update State Machine**: Replace hardcoded file paths with skit category selection
- [x] **Implement Directory Structure**: Support `/audio/welcome/` and `/audio/fortune/` directories
- [x] **Weighted Random Selection**: Add weighted random selection per category with no immediate repeat
- [x] **Test Integration**: Verify welcome/fortune skit selection works with state machine

## Technical Implementation Details

### Skit Category Requirements

**Directory Structure:**
The system must support organized skit directories on the SD card:
- `/audio/welcome/` - Welcome skits for FAR sensor triggers
- `/audio/fortune/` - Fortune skits for NEAR sensor triggers  
- `/audio/general/` - General skits for backward compatibility

**Skit Selection Behavior:**
- Random selection from category-specific directories
- Support for single or multiple skit files per category
- Fallback behavior when category-specific skits unavailable

**File Format Support:**
- WAV audio files for playback
- TXT timing files for jaw synchronization
- Support for numbered naming convention (e.g., welcome_01.wav, welcome_02.wav)
- Handle missing timing files gracefully

### State Machine Integration

**Welcome Sequence:**
- Replace hardcoded welcome file paths with category-based selection
- Select from `/audio/welcome/` directory using weighted random
- Maintain existing audio playback and jaw sync behavior
- Handle empty welcome directory gracefully

**Fortune Flow:**
- Replace hardcoded fortune file paths with category-based selection
- Select from `/audio/fortune/` directory using weighted random
- Maintain existing fortune flow timing and behavior
- Handle empty fortune directory gracefully

**State Transitions:**
- Integrate with existing state machine without breaking current flow
- Maintain all existing timing and trigger behavior
- Preserve busy policy and debounce logic
- Support all existing state transitions

### Backward Compatibility

**Legacy Support:**
- Maintain support for existing skit system
- Preserve existing file naming conventions
- Support gradual migration from hardcoded to category-based selection
- Handle mixed old/new skit systems during transition

### Error Handling and Fallbacks

**Empty Directories:**
- Detect when skit categories have no available files
- Fall back to general category or default behavior
- Log warnings for missing skit categories
- Continue system operation with available skits

**File Access Errors:**
- Handle SD card access issues gracefully
- Retry file operations when appropriate
- Log file access errors for debugging
- Continue with available skits when possible


### Integration Points

**SD Card Manager:**
- Integrate with existing SD card file operations
- Use existing file discovery and access methods
- Maintain SD card performance and reliability
- Handle SD card mount/unmount scenarios

**Audio Player:**
- Integrate with existing audio playback system
- Maintain existing audio quality and timing
- Support existing audio file formats and codecs
- Preserve existing audio error handling

**State Machine:**
- Integrate with existing state transitions
- Maintain existing timing and trigger behavior
- Support existing busy policy and debounce logic
- Preserve existing error handling and recovery

## Notes
- **2025 MVP Limitation**: For this year, we're only doing a SINGLE audio file per skit folder (welcome_01.wav, fortune_01.wav). Code should support multiple files for future expansion, but don't over-engineer for this year's timeline.
- Maintain backward compatibility with existing skit system
- Ensure proper error handling when skit directories are empty
- Test with actual skit files once they're recorded
- Consider fallback behavior when category-specific skits aren't available
- Coordinate with Story 003 (UART triggers) to ensure proper integration
- **Status**: Core functionality is implemented - directory structure and category-based selection are working

## Dependencies
- Story 002 (SD Audio Playback & Jaw Synchronization) - Base skit system
- Story 003 (Matter UART Trigger Handling) - State machine integration
- Actual skit recordings (external dependency)

## Testing Strategy
1. **Unit Testing**: Test `SkitCategoryManager` with mock SD card data
2. **Integration Testing**: Verify state machine triggers correct skit categories
3. **End-to-End Testing**: Test with actual recorded skit files
4. **Fallback Testing**: Test behavior when skit directories are empty or missing

## Implementation Log
- 2025-10-28 — Added `AudioDirectorySelector` for weighted, no-repeat selection per audio category, integrated welcome and fortune flows with the new selector, and added boot-time self-tests to detect immediate repeats; PlatformIO build (`pio run -e esp32dev`) succeeded. ✅
- 2025-10-28 — IRL verification on hardware confirmed welcome and fortune skit selection without immediate repeats; functionality signed off. ✅
