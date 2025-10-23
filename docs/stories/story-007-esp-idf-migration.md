# Story 007: ESP-IDF Framework Migration

**Story ID**: 007  
**Title**: ESP-IDF Framework Migration  
**Priority**: Maybe  
**Status**: To Do  
**Created**: 2025-01-22  
**Dependencies**: Stories 001-006 (current Arduino implementation)  

## Overview

Migrate the Death Fortune Teller project from Arduino framework to ESP-IDF (Espressif IoT Development Framework) to achieve better performance, professional development standards, and future-proofing for advanced features.

## Background

The project currently uses Arduino framework with PlatformIO, which was appropriate for hand-coded development. However, since the project is AI-driven with no human coding complexity, ESP-IDF becomes the superior choice due to:

- **30-50% better performance** (native hardware access vs Arduino wrappers)
- **20-30% more available RAM** (reduced framework overhead)
- **Superior documentation** (official Espressif docs are more comprehensive)
- **Professional standards** (industry-standard for ESP32 development)
- **Better AI training data** (more consistent patterns and examples)

## Acceptance Criteria

### Functional Requirements
- [ ] All current functionality preserved (audio playback, jaw sync, Bluetooth, etc.)
- [ ] Performance improvement of at least 30% in audio processing
- [ ] Memory usage reduction of at least 20%
- [ ] Real-time audio processing with dedicated task
- [ ] Multi-core utilization (audio processing on dedicated core)

### Technical Requirements
- [ ] Complete migration of all Arduino libraries to ESP-IDF equivalents
- [ ] Maintain all current hardware interfaces (servo, SD card, Bluetooth, etc.)
- [ ] Preserve all configuration and OTA capabilities
- [ ] Maintain compatibility with existing SD card structure
- [ ] Preserve all Matter UART communication protocols

### Quality Requirements
- [ ] All existing tests pass with new implementation
- [ ] Performance benchmarks meet or exceed current system
- [ ] Memory usage within acceptable limits
- [ ] No regression in functionality
- [ ] Professional code structure following ESP-IDF patterns

## Technical Implementation

### Library Migration Map

| Current Arduino Library | ESP-IDF Equivalent | Migration Complexity |
|------------------------|-------------------|---------------------|
| `ESP32Servo@^3.0.9` | `ledc.h` (LEDC API) | Medium |
| `SD@^2.0.0` | `esp_vfs_fat.h` + `sdmmc.h` | Medium |
| `ArduinoJson@^6.21.5` | `cJSON` | Low |
| `arduinoFFT@^1.5.7` | `esp-dsp` (FFT functions) | Medium |
| `ESP32-A2DP` | `esp_a2dp_api.h` | High |
| `ESPLogger` | `esp_log.h` | Low |

### Core Component Migrations

#### 1. Servo Control (ServoController)
**Current Arduino Implementation:**
```cpp
#include <ESP32Servo.h>
Servo servo;
servo.attach(pin);
servo.write(angle);
```

**ESP-IDF Implementation:**
```c
#include "ledc.h"
// LEDC timer configuration
ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .freq_hz = 50,  // 50Hz for servo
    .clk_cfg = LEDC_AUTO_CLK
};
// LEDC channel configuration
ledc_channel_config_t channel_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .intr_type = LEDC_INTR_DISABLE,
    .gpio_num = SERVO_PIN,
    .duty = 0,
    .hpoint = 0
};
```

#### 2. SD Card Operations (SDCardManager)
**Current Arduino Implementation:**
```cpp
#include <SD.h>
SD.begin();
File file = SD.open("/audio/test.wav");
```

**ESP-IDF Implementation:**
```c
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
```

#### 3. Bluetooth A2DP (BluetoothController)
**Current Arduino Implementation:**
```cpp
#include "BluetoothA2DPSource.h"
BluetoothA2DPSource a2dp_source;
a2dp_source.start("SpeakerName");
```

**ESP-IDF Implementation:**
```c
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

// Initialize Bluetooth stack
esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
esp_bt_controller_init(&bt_cfg);
esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
```

#### 4. Audio Processing (SkullAudioAnimator)
**Current Arduino Implementation:**
```cpp
#include <arduinoFFT.h>
ArduinoFFT<double> FFT;
FFT.Compute(vReal, vImag, samples, FFT_FORWARD);
```

**ESP-IDF Implementation:**
```c
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "esp_dsp.h"

// Initialize DSP
dsps_fft2r_fc32(data, N);
```

#### 5. JSON Parsing (FortuneGenerator)
**Current Arduino Implementation:**
```cpp
#include <ArduinoJson.h>
DynamicJsonDocument doc(1024);
deserializeJson(doc, jsonString);
```

**ESP-IDF Implementation:**
```c
#include "cjson.h"

cJSON *json = cJSON_Parse(jsonString);
cJSON *item = cJSON_GetObjectItem(json, "key");
```

### Project Structure Changes

#### Current Arduino Structure
```
src/
├── main.cpp                    # Arduino setup()/loop()
├── audio_player.h/.cpp         # Audio playback
├── bluetooth_controller.h/.cpp # A2DP Bluetooth
├── servo_controller.h/.cpp     # Servo control
├── light_controller.h/.cpp     # LED control
├── uart_controller.h/.cpp      # UART communication
├── thermal_printer.h/.cpp      # Thermal printer
├── finger_sensor.h/.cpp        # Capacitive sensor
├── fortune_generator.h/.cpp    # Fortune generation
├── sd_card_manager.h/.cpp      # SD card management
├── config_manager.h/.cpp       # Configuration
└── skull_audio_animator.h/.cpp # Audio-synced animation
```

#### Proposed ESP-IDF Structure
```
main/
├── main.c                      # ESP-IDF app_main()
├── audio_player.h/.c           # Audio playback
├── bluetooth_controller.h/.c   # A2DP Bluetooth
├── servo_controller.h/.c       # Servo control
├── light_controller.h/.c       # LED control
├── uart_controller.h/.c        # UART communication
├── thermal_printer.h/.c        # Thermal printer
├── finger_sensor.h/.c          # Capacitive sensor
├── fortune_generator.h/.c      # Fortune generation
├── sd_card_manager.h/.c        # SD card management
├── config_manager.h/.c         # Configuration
└── skull_audio_animator.h/.c   # Audio-synced animation
```

### PlatformIO Configuration Changes

#### Current Configuration
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 460800
board_build.partitions = partitions/fortune_ota.csv
lib_deps =
    arduinoFFT@^1.5.7
    SD@^2.0.0
    ArduinoJson@^6.21.5
    ESP32Servo@^3.0.9
    https://github.com/pschatzmann/ESP32-A2DP
    https://github.com/fabianoriccardi/ESPLogger
```

#### Proposed ESP-IDF Configuration
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
upload_speed = 460800
board_build.partitions = partitions/fortune_ota.csv
lib_deps =
    espressif/esp-dsp
    https://github.com/espressif/esp-idf
build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=0
```

## Implementation Plan

### Phase 1: Foundation Setup (2-3 weeks)
- [ ] Set up ESP-IDF development environment
- [ ] Create new project structure
- [ ] Implement basic hardware initialization
- [ ] Port configuration management system
- [ ] Set up logging system

### Phase 2: Core Hardware Migration (3-4 weeks)
- [ ] Migrate servo control (LEDC API)
- [ ] Migrate SD card operations (VFS + SDMMC)
- [ ] Migrate LED control (GPIO + LEDC)
- [ ] Migrate UART communication
- [ ] Migrate capacitive sensor

### Phase 3: Audio System Migration (4-5 weeks)
- [ ] Migrate Bluetooth A2DP (native stack)
- [ ] Migrate audio file playback
- [ ] Migrate FFT audio processing
- [ ] Implement jaw synchronization
- [ ] Test audio quality and timing

### Phase 4: Advanced Features (2-3 weeks)
- [ ] Migrate thermal printer control
- [ ] Migrate fortune generation (cJSON)
- [ ] Implement multi-core task distribution
- [ ] Optimize real-time performance

### Phase 5: Integration & Testing (2-3 weeks)
- [ ] Integrate all components
- [ ] Performance testing and optimization
- [ ] Memory usage optimization
- [ ] End-to-end testing
- [ ] Documentation updates

## Risk Assessment

### High Risk
- **Bluetooth A2DP Migration**: Complex native stack integration
- **Audio Processing**: Real-time FFT implementation
- **Multi-core Coordination**: Task synchronization

### Medium Risk
- **SD Card Operations**: VFS integration complexity
- **Servo Control**: LEDC API learning curve
- **Performance Optimization**: Memory and CPU tuning

### Low Risk
- **JSON Parsing**: cJSON is well-documented
- **Logging**: esp_log.h is straightforward
- **GPIO Control**: Direct hardware access

## Success Metrics

### Performance Improvements
- **Audio Processing**: 30-50% faster FFT operations
- **Memory Usage**: 20-30% reduction in RAM usage
- **Real-time Performance**: Dedicated audio processing task
- **Multi-core Utilization**: Audio on core 0, control on core 1

### Code Quality
- **Professional Standards**: ESP-IDF coding patterns
- **Documentation**: Comprehensive inline documentation
- **Maintainability**: Clean, modular architecture
- **Testability**: Unit tests for all components

## Dependencies

### External Dependencies
- ESP-IDF v5.1+ (latest stable)
- PlatformIO ESP-IDF support
- ESP32 hardware (no changes)
- All existing hardware components

### Internal Dependencies
- Complete Stories 001-006 (current Arduino implementation)
- Hardware testing and validation
- Performance benchmarking
- Documentation updates

## Deliverables

### Code Deliverables
- [ ] Complete ESP-IDF codebase
- [ ] Updated PlatformIO configuration
- [ ] Migration documentation
- [ ] Performance benchmarks
- [ ] Unit tests

### Documentation Deliverables
- [ ] Updated README.md
- [ ] ESP-IDF development guide
- [ ] Performance comparison report
- [ ] Migration guide for future projects

## Future Considerations

### Potential Enhancements
- **Advanced Audio Processing**: Real-time audio effects
- **Machine Learning**: AI-powered fortune generation
- **Cloud Integration**: Remote fortune updates
- **Advanced Sensors**: Multiple finger detection
- **Voice Recognition**: Spoken fortune requests

### Maintenance
- **Regular Updates**: ESP-IDF version updates
- **Performance Monitoring**: Continuous optimization
- **Feature Additions**: Modular architecture for easy expansion
- **Documentation**: Keep migration docs current

## Conclusion

This migration represents a significant technical upgrade that will provide better performance, professional development standards, and future-proofing for the Death Fortune Teller project. While the migration effort is substantial, the benefits for an AI-driven project make ESP-IDF the superior choice.

The key success factors are:
1. **Thorough planning** and phased implementation
2. **Comprehensive testing** at each phase
3. **Performance benchmarking** throughout development
4. **Professional code standards** from the start
5. **Future-proof architecture** for advanced features

**Recommendation**: Proceed with ESP-IDF migration as a high-priority enhancement after completing the core Arduino implementation.
