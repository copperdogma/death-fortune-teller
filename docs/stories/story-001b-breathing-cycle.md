# Story 001b: Breathing Cycle Implementation

**Priority:** High  
**Status:** Done  
**Story Type:** Feature Enhancement

**Note:** Implementation complete with known limitation: servo motion exhibits visible stepping/jerky behavior with ESP32Servo library compared to TwoSkulls reference implementation. See [issues.md](../issues.md#001-servo-stepping-motion) for details.

## Overview

Implement the breathing cycle idle animation from the TwoSkulls project to make Death appear alive and engaging when not actively performing. This breathing effect opens and closes the jaw periodically when no audio is playing.

## Background

The TwoSkulls implementation had a breathing cycle feature that animated the servo jaw every 7 seconds when idle. This creates a more lifelike, engaging presence for the skull when it's waiting for user interaction.

## Current State Analysis

### Already Implemented ✅
- `ServoController::smoothMove()` - Smooth movement function for gradual servo transitions
- `ServoController::interruptMovement()` - Ability to interrupt ongoing movements
- `shouldInterruptMovement` flag tracking mechanism
- Audio-driven animation interrupts breathing automatically via `SkullAudioAnimator::updateJawPosition()`

### Missing Implementation ❌
- Breathing cycle constants (`BREATHING_INTERVAL`, `BREATHING_JAW_ANGLE`, `BREATHING_MOVEMENT_DURATION`)
- `breathingJawMovement()` function implementation
- Loop integration to check timing and trigger breathing

## Requirements

### Functional Requirements
1. Jaw should open to 30 degrees over 2 seconds
2. Jaw should pause briefly (100ms) at open position
3. Jaw should close to 0 degrees over 2 seconds
4. Breathing cycle should repeat every 7 seconds when idle
5. Breathing should only occur when no audio is playing
6. Audio-driven jaw movement should interrupt breathing smoothly

### Technical Requirements
1. Use existing `smoothMove()` function for smooth animation
2. Respect the `interruptMovement()` mechanism for audio interruptions
3. Add timing-based checks in the main loop
4. Maintain non-blocking behavior (use existing delay mechanisms in `smoothMove`)

## Implementation Plan

### Step 1: Add Global Variables
Location: `src/main.cpp` (near line 90, with other global variables)

```cpp
// Breathing cycle state
unsigned long lastJawMovementTime = 0;
const unsigned long BREATHING_INTERVAL = 7000;        // 7 seconds between breaths
const int BREATHING_JAW_ANGLE = 30;                   // 30 degrees opening
const int BREATHING_MOVEMENT_DURATION = 2000;         // 2 seconds per movement
```

### Step 2: Implement Breathing Function
Location: `src/main.cpp` (before `loop()` function, around line 355)

```cpp
void breathingJawMovement() {
    if (!audioPlayer->isAudioPlaying()) {
        servoController.smoothMove(BREATHING_JAW_ANGLE, BREATHING_MOVEMENT_DURATION);
        delay(100); // Short pause at open position
        servoController.smoothMove(0, BREATHING_MOVEMENT_DURATION);
    }
}
```

### Step 3: Integrate into Main Loop
Location: `src/main.cpp` (end of `loop()` function, before `delay(1)`, around line 430)

```cpp
// Check if it's time to move the jaw for breathing
if (currentTime - lastJawMovementTime >= BREATHING_INTERVAL && !audioPlayer->isAudioPlaying()) {
    breathingJawMovement();
    lastJawMovementTime = currentTime;
}
```

## Testing Strategy

### Unit Testing
- Verify breathing cycle timing (7 second intervals)
- Verify breathing only occurs when audio is not playing
- Verify smooth movement duration (2 seconds per phase)

### Integration Testing
- Test that audio playback interrupts breathing smoothly
- Test that breathing resumes after audio ends (after 7 seconds)
- Test breathing during various state machine states (IDLE, COOLDOWN, etc.)

### Visual Testing
- Observe jaw movement smoothness and natural appearance
- Verify breathing feels lifelike and not mechanical
- Check that breathing enhances the skull's presence when idle

## Acceptance Criteria

- [ ] Jaw performs breathing animation when no audio is playing
- [ ] Breathing cycle repeats every 7 seconds
- [ ] Breathing opens to 30 degrees over 2 seconds
- [ ] Breathing pauses 100ms at open position
- [ ] Breathing closes to 0 degrees over 2 seconds
- [ ] Audio playback immediately interrupts breathing
- [ ] Breathing resumes 7 seconds after audio ends
- [ ] No performance degradation or blocking behavior
- [ ] Implementation matches TwoSkulls behavior

## Implementation Notes

### Design Decisions
1. **Non-blocking approach**: Using existing `smoothMove()` with built-in delays ensures non-blocking behavior
2. **Simple state tracking**: Single `lastJawMovementTime` variable is sufficient for timing
3. **Explicit audio check**: Only breathe when explicitly not playing audio
4. **Natural timing**: 7-second intervals, 2-second movements feel natural and lifelike

### Reference Implementation
The TwoSkulls implementation in `TwoSkulls/TwoSkulls.ino` serves as the reference:
- Lines 73-76: Constants definition
- Lines 177-185: Breathing function implementation
- Lines 503-508: Loop integration

## Related Stories
- Story 002: SD Audio Playback & Jaw Synchronization (audio-driven jaw movement)
- Story 004a: LED Control System & Fault Indicators (eye brightness during breathing)

## Dependencies
- Requires working servo controller (Story 001)
- Requires audio player for state checking (Story 002)
- Works independently of other features

## Future Enhancements (Out of Scope)
- Configurable breathing parameters via config file
- Variable breathing rhythm (breath hold, uneven timing)
- Breathing intensity variations based on idle duration
- Optional breathing during audio fadeouts
