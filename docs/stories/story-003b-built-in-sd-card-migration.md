# Story 003b: Built-in SD Card Migration

**Priority**: High  
**Status**: To Do  
**Estimated Effort**: 2-4 hours  
**Dependencies**: Story 002 (SD Audio Playback & Jaw Synchronization)

## Overview

Migrate from the external FZ0829-micro-sd-card-module to the built-in MicroSD card slot on the FREENOVE ESP32-WROVER development board. This discovery eliminates the need for external SD card hardware, frees up 4 GPIO pins, and simplifies the overall hardware design.

## Background

During hardware review, it was discovered that the FREENOVE ESP32-WROVER development board includes a built-in MicroSD card slot on the underside of the board. The project was previously using an external SD card module connected via SPI, which:

- Consumes 4 GPIO pins (CS, SCK, MOSI, MISO)
- Creates pin conflicts with other peripherals
- Adds unnecessary hardware complexity
- Requires additional power connections

## Research Findings

### Built-in SD Card Capabilities

The FREENOVE ESP32-WROVER development board features:
- **Built-in MicroSD card slot** on the underside
- **SDMMC interface** (faster than SPI)
- **Hardware-level integration** (no external wiring needed)
- **3.3V power supply** from the board

### Technical Advantages

1. **Performance**: SDMMC interface provides higher data transfer rates than SPI
2. **Reliability**: Built-in connections are more robust than external modules
3. **Pin Availability**: Frees up GPIO 5, 18, 19, 23 for other uses
4. **Power Efficiency**: No additional power consumption from external module
5. **Design Simplicity**: Reduces component count and wiring complexity

### Pin Conflict Resolution

Current external SD card pins that will be freed:
- **GPIO 5**: SD Card CS → **Available** (was conflicting with thermal printer)
- **GPIO 18**: SD Card SCK → **Available** (was conflicting with thermal printer)
- **GPIO 19**: SD Card MISO → **Available** (was conflicting with thermal printer)
- **GPIO 23**: SD Card MOSI → **Available**

## Implementation Plan

### Phase 1: Code Migration

#### 1.1 Update SD Card Manager
- Replace `#include "SD.h"` with `#include "SD_MMC.h"`
- Change `SD.begin()` to `SD_MMC.begin()`
- Verify all existing file operations work identically

#### 1.2 Update PlatformIO Configuration
- Remove external SD card module dependencies
- Update build flags if needed
- Verify SDMMC library is available

#### 1.3 Test Audio Playback
- Verify existing audio files load correctly
- Test skit parsing and playback
- Ensure jaw synchronization still works

### Phase 2: Hardware Documentation Updates

#### 2.1 Update hardware.md
- Remove external SD card module from Bill of Materials
- Update pin assignments to reflect freed GPIO pins
- Remove SD card SPI interface section
- Update power requirements (remove SD card module current draw)
- Update wiring diagrams to remove SD card connections

#### 2.2 Update construction.md
- Remove FZ0829-micro-sd-card-module from BOM
- Update power distribution notes
- Remove SD card module wiring instructions

#### 2.3 Update Other Documentation
- Search for and update any other references to external SD card module
- Update troubleshooting sections
- Update pin conflict analysis

### Phase 3: Hardware Verification

#### 3.1 Physical Testing
- Insert MicroSD card into built-in slot
- Verify card detection and mounting
- Test file read/write operations
- Verify audio playback functionality

#### 3.2 Pin Availability Verification
- Confirm GPIO 5, 18, 19, 23 are available for other uses
- Update pin assignments for thermal printer or other peripherals
- Test that freed pins work correctly

## Code Changes Required

### SD Card Manager Updates

**Current Implementation:**
```cpp
#include "SD.h"

bool SDCardManager::begin() {
    if (!SD.begin()) {
        LOG_ERROR(TAG, "Mount failed");
        return false;
    }
    LOG_INFO(TAG, "Mounted successfully");
    return true;
}
```

**Updated Implementation:**
```cpp
#include "SD_MMC.h"

bool SDCardManager::begin() {
    if (!SD_MMC.begin()) {
        LOG_ERROR(TAG, "Mount failed");
        return false;
    }
    LOG_INFO(TAG, "Mounted successfully");
    return true;
}
```

### PlatformIO Configuration Updates

**Current platformio.ini:**
```ini
lib_deps =
    SD@^2.0.0
```

**Updated platformio.ini:**
```ini
lib_deps =
    # SD_MMC is built into ESP32 Arduino Core
```

## Hardware Changes

### Bill of Materials Updates

**Remove:**
- FZ0829-micro-sd-card-module for audio storage

**Keep:**
- MicroSD card with audio files and configuration (now goes in built-in slot)

### Pin Assignment Updates

**Freed GPIO Pins:**
- GPIO 5: Now available for thermal printer or other peripherals
- GPIO 18: Now available for thermal printer or other peripherals  
- GPIO 19: Now available for thermal printer or other peripherals
- GPIO 23: Now available for other peripherals

### Power Requirements Updates

**Current Power Budget:**
- SD Card Module: 200mA peak at 5V

**Updated Power Budget:**
- Built-in SD Card: ~50mA peak at 3.3V (powered from ESP32 board)

**Net Power Savings:** ~150mA reduction in 5V rail requirements

## Testing Strategy

### Unit Tests
1. **SD Card Detection**: Verify card mounts successfully
2. **File Operations**: Test read/write operations
3. **Audio Loading**: Verify skit files load correctly
4. **Performance**: Compare read speeds vs external module

### Integration Tests
1. **Audio Playback**: Test complete audio playback pipeline
2. **Jaw Synchronization**: Verify servo control still works
3. **System Stability**: Long-term operation test
4. **Pin Availability**: Verify freed pins work for other peripherals

### Hardware Tests
1. **Card Insertion/Removal**: Test physical slot reliability
2. **Power Consumption**: Measure actual power usage
3. **Temperature**: Verify no overheating during operation

## Risk Assessment

### Low Risk
- **Code Migration**: Minimal changes required
- **File Operations**: Identical API between SD and SD_MMC libraries
- **Audio Playback**: No changes to audio processing pipeline

### Medium Risk
- **Card Compatibility**: Some SD cards may not work with SDMMC interface
- **Power Supply**: Built-in slot powered from 3.3V rail (should be sufficient)

### Mitigation Strategies
- **Card Testing**: Test with multiple SD card types/sizes
- **Fallback Plan**: Keep external module as backup if issues arise
- **Power Monitoring**: Monitor 3.3V rail during operation

## Success Criteria

### Functional Requirements
- [ ] SD card mounts successfully using built-in slot
- [ ] All existing audio files load and play correctly
- [ ] Jaw synchronization works with built-in SD card
- [ ] File read/write operations work identically
- [ ] GPIO pins 5, 18, 19, 23 are available for other uses

### Performance Requirements
- [ ] Audio playback latency ≤ current implementation
- [ ] File loading speed ≥ current implementation
- [ ] Power consumption ≤ current implementation

### Documentation Requirements
- [ ] hardware.md updated with new pin assignments
- [ ] construction.md updated with new BOM
- [ ] All code comments updated
- [ ] Troubleshooting guide updated

## Implementation Timeline

### Day 1: Code Migration (2-3 hours)
- Update SDCardManager to use SD_MMC
- Test basic SD card operations
- Verify audio file loading

### Day 2: Hardware Updates (1-2 hours)
- Update hardware.md documentation
- Update construction.md documentation
- Test freed GPIO pins

### Day 3: Integration Testing (1-2 hours)
- Full system test with built-in SD card
- Performance comparison
- Long-term stability test

## Future Benefits

### Immediate Benefits
- Simplified hardware design
- Reduced component count
- Eliminated pin conflicts
- Lower power consumption

### Long-term Benefits
- Easier maintenance (no external module to fail)
- More reliable connections
- Cleaner PCB layout
- Reduced manufacturing cost

## Related Stories

- **Story 002**: SD Audio Playback & Jaw Synchronization (prerequisite)
- **Story 005**: Thermal Printer Fortune Output (benefits from freed pins)
- **Story 004**: Capacitive Finger Detection & Snap Flow (benefits from freed pins)

## Notes

This migration represents a significant hardware simplification that was discovered during routine hardware review. The built-in SD card slot was present on the development board but not utilized, representing an opportunity for immediate improvement with minimal effort.

The migration should be completed before implementing other hardware features that could benefit from the freed GPIO pins, particularly the thermal printer and capacitive sensor systems.
