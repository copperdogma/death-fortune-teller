# Changelog

## [2025-01-25] - Hardware Milestone: ESP32-WROVER Fully Operational

### Milestone Achievement
**Hardware System Successfully Bootstrapped!**

Successfully deployed and tested complete ESP32-WROVER hardware system with:
- **ESP32-WROVER Breakout Board**: Main control board with WROVER module
- **SD Card Reader**: Functional SD card mounting and file reading
- **LED System**: Eye LED (GPIO 32) and mouth LED (GPIO 33) operational
- **ESP32 Super Mini Breakout**: Acting as 5V power distribution board
- **Bluetooth A2DP**: Full audio streaming to external speaker
- **Audio Playback**: WAV file playback from SD card working end-to-end

### Verified Working Features
- ✅ Eye LED startup flash sequence
- ✅ SD card mounting and content loading
- ✅ Audio file enqueuing (`initialized.wav`)
- ✅ Bluetooth speaker connection and pairing
- ✅ Audio playback with synchronized streaming
- ✅ All initialization sequences complete without crashes

### Fixed
- **SkitSelector Crash**: Fixed LoadProhibited crash when SkitSelector initialized with empty skit list
  - Added empty vector check in `selectNextSkit()` method
  - Returns empty `ParsedSkit` instead of accessing invalid memory
  - Added graceful handling in `testSkitSelection()` for zero-skits scenario
- **Jaw Sync Test Override**: Removed temporary test code that was overriding initialization audio

### Changed
- **Initialization Audio**: Simplified to use `/audio/initialized.wav` directly
- **Error Handling**: Improved robustness when SD card has no skits available

### Technical Notes
- System successfully handles edge cases (empty skit lists, missing audio files)
- Bluetooth connection and audio streaming verified working
- SD card content loading operational
- All major subsystems initialized and functional

## [2025-01-25] - Matter Controller Alignment and Documentation Updates

### Added
- **Matter Controller Link**: Added link to Death Matter Controller repository in README.md
- **State Mapping Documentation**: Complete mapping of all 12 Matter controller command codes (0x01-0x0C) to Death state machine states
- **State Forcing Requirements**: Documented requirements for Matter controller to force state transitions via UART commands

### Changed
- **spec.md §6**: Removed UART implementation details, focused on requirements and state alignment with Matter controller
- **story-003**: Updated with Matter controller canonical command codes and existing code references
- **story-003a**: Added Matter Controller State Mapping section with complete command-to-state mapping and state forcing behavior
- **UART Command Names**: Updated from TRIGGER_FAR/TRIGGER_NEAR to FAR_MOTION_DETECTED/NEAR_MOTION_DETECTED
- **LED Pin Configuration**: Updated hardware documentation and code to reflect single eye LED (GPIO 32) and mouth LED (GPIO 33)

### Documentation
- **Requirements Focus**: Spec now focuses on what needs to be done, not how to implement it
- **Code References**: Stories now include specific file locations and line numbers for existing code that needs updating
- **Canonical Source**: Established Matter controller repository as canonical source for UART command codes

### Technical Details
- Matter controller can send 12 different command codes to control Death's state machine
- Commands 0x01-0x02 are trigger-based (FAR/NEAR motion detected)
- Commands 0x03-0x0C allow direct state forcing (can override busy state)
- All command codes must align with Matter controller's canonical definitions

## [2025-01-25] - LED System Update: Pink LEDs and Resistor Calculation

### Added
- **Resistor Calculation Section**: Added comprehensive resistor calculation for pink LEDs in hardware-led_cluster_wiring_diagram.md
- **BOJACK LED Specifications**: Documented pink LED specifications (3.0-3.2V forward voltage, 20mA current)

### Changed
- **LED Color Specification**: Updated all documentation to specify pink LEDs instead of red/green
  - Hardware documentation (hardware.md)
  - LED cluster wiring diagram (hardware-led_cluster_wiring_diagram.md)
  - System specification (spec.md)
- **Resistor Calculation**: Recalculated resistor value based on pink LED forward voltage (3.0-3.2V)
  - Theoretical value: 10Ω (vs 60Ω for red LEDs)
  - Confirmed 100Ω is still correct for safety margin and voltage tolerance

### Technical Details
- Pink LED forward voltage: 3.0-3.2V (from BOJACK specs)
- ESP32 GPIO output: 3.3V
- Resistor calculation: R = (3.3V - 3.1V) / 0.020A = 10Ω
- Recommended 100Ω provides safety margin for voltage spikes and ESP32 variations

## [2025-01-25] - Configuration Management System (Story 001a)

### Added
- **Complete Configuration Management**: Implemented all configuration keys from spec.md §8
- **Type-Safe Configuration Access**: Added getter methods for all configuration values with proper types
- **Configuration Validation**: Two-layer validation (load-time warnings + runtime enforcement with defaults)
- **Configuration Keys**: Implemented all required keys (15 total):
  - Basic settings: role, speaker_name, speaker_volume
  - WiFi settings: wifi_ssid, wifi_password, ota_hostname, ota_password
  - Servo: servo_us_min, servo_us_max
  - Cap sense: cap_threshold
  - Timing: finger_wait_ms, snap_delay_min_ms, snap_delay_max_ms, cooldown_ms
  - Printer: printer_baud, printer_logo, fortunes_json
- **SD Card Configuration**: Support for `/config.txt` on SD card with comment support

### Implemented
- **Getter Methods**: All configuration values accessible via type-safe methods
- **Validation Rules**: Comprehensive validation for ranges, relationships (min < max), and constraints
- **Default Values**: Sensible defaults for all configuration options
- **Error Handling**: Graceful fallback to defaults when configuration is invalid or missing
- **Load-Time Validation**: Warnings logged for invalid values during startup
- **Runtime Validation**: Getters enforce validation and return safe defaults

### Enhanced
- **ConfigManager Class**: Extended with 10 new getter methods for hardware/timing/printer settings
- **Documentation**: Updated SD_CARD_SETUP.md with all configuration keys and examples
- **Config Template**: Updated sd-card-files/config.txt with all configuration keys
- **Story Documentation**: Complete story tracking in docs/stories/story-001a-configuration-management.md

### Fixed
- **Remote Debug Configuration**: Removed unused remote_debug_enabled and remote_debug_port config keys (telnet starts automatically)
- **Config Path**: Corrected configuration file path from `/config/config.txt` to `/config.txt`

### Tested
- **Valid Configuration**: Tested with all 15 keys present - loads successfully
- **Invalid Configuration**: Tested with out-of-range values - validation warnings and safe defaults
- **Missing Configuration**: Tested with empty file - system operates with defaults
- **Hardware Validation**: All scenarios tested on actual ESP32 hardware

### Notes
- Configuration file changes must be done manually by removing SD card from ESP32
- System operates normally even with missing or invalid configuration
- Validation logs warnings but continues operation with safe defaults

## [2025-01-23] - Breathing cycle idle animation implementation

### Added
- **Breathing Cycle Animation**: Idle jaw movement that opens and closes every 7 seconds when no audio is playing
- **Story 001b Documentation**: Complete story documentation in `docs/stories/story-001b-breathing-cycle.md`
- **Issues Tracking**: New `docs/issues.md` file for tracking outstanding issues and known limitations
- **Servo Troubleshooting Log**: Detailed investigation log in `ai-work/issues/servo-issues.md` documenting servo smoothness issue

### Implemented
- **Breathing Function**: `breathingJawMovement()` function with configurable timing and angles
- **Loop Integration**: Automatic breathing cycle timing check in main loop with audio-aware activation
- **Servo Timer Width**: Increased PWM resolution to 20-bit (1,048,576 ticks) for finer servo control
- **Progress Clamping**: Added bounds checking to smoothMove() interpolation for consistent behavior

### Enhanced
- **Servo Controller**: Improved PWM resolution with `servo.setTimerWidth(20)` for smoother motion
- **Code Documentation**: Clear inline comments explaining breathing cycle timing and servo control
- **Story Status Tracking**: Updated story index to reflect completed breathing cycle implementation

### Known Limitations
- **Servo Motion Quality**: Motion exhibits visible stepping/jerky behavior compared to TwoSkulls reference implementation
- **Library Limitation**: ESP32Servo library differs from Arduino Servo.h in PWM generation, affecting motion smoothness
- **Investigation**: Multiple approaches tested (timer width, microsecond precision, update frequency) with no complete resolution
- Detailed investigation documented in `ai-work/issues/servo-issues.md`

## [2025-01-23] - Audio conversion system for ESP32 deployment

### Added
- **Audio Conversion Pipeline**: Complete system for converting M4A files to ESP32-compatible WAV format
- **Audio Specifications Documentation**: `audio-raw/audio-specs-for-ai.md` with detailed format requirements
- **Automated Conversion Script**: `audio-raw/convert_audio.sh` with intelligent silence trimming and normalization
- **Smart Silence Detection**: Advanced FFmpeg-based silence removal with natural audio buffer preservation
- **Peak Normalization**: Loudness normalization using EBU R128 standard for consistent audio levels

### Implemented
- **WAV Format Compliance**: 44.1kHz, 16-bit, stereo PCM format required by ESP32 audio player
- **Intelligent Silence Trimming**: Detects speech boundaries and preserves 0.5s of original audio context
- **Batch Processing**: Automated conversion of entire directories with proper naming conventions
- **Quality Optimization**: Tuned parameters for optimal balance between tight audio and natural spacing

### Enhanced
- **File Naming**: Automatic conversion from "Death - " prefix to lowercase underscore format
- **Error Handling**: Robust conversion pipeline with detailed success/failure reporting
- **Audio Quality**: Professional-grade normalization and silence removal for ESP32 deployment

## [2025-10-26] - Helper script regression verification

### Added
- Logged verification run for all helper scripts in `ai-work/issues/telnet-issues.md`, including results for discovery, telnet, OTA, and troubleshooting flows.

### Verified
- Reflashed firmware before each helper run to confirm cache-first discovery logic works end-to-end.
- Confirmed every helper script executes successfully post cache-first refactor, with sandbox timeouts noted where applicable.

## [2025-10-26] - Cache-first Telnet/OTA helpers

### Changed
- `scripts/telnet_command.py`, `telnet_stream.py`, and `ota_upload_auto.py` now try the cached or explicit host first, only invoking fast discovery on failure, with an optional `--full-discovery` fallback.
- `scripts/system_status.py` and `troubleshoot.py` reuse the shared cache/discovery helpers and call telnet commands with shorter timeouts so dashboards fail fast when the skull is offline.
- Updated `docs/ota.md` to document the cache-first behaviour and new `--full-discovery` option.

### Fixed
- Eliminated 45–70 s delays caused by automatic full subnet scans whenever telnet or OTA commands timed out.

### Testing
- `python -m compileall scripts`

## [2025-10-25] - Telnet stability and OTA workflow hardening

### Added
- `ai-work/issues/telnet-issues.md` log capturing systematic debugging steps and resolution
- Telnet `bluetooth on|off|status` and `reboot` commands for runtime control

### Changed
- Added telnet `bluetooth on|off` commands and wired OTA tasks to disable/re-enable audio streaming automatically
- Defaulted RemoteDebug auto-streaming to off and documented timeout guidance in `docs/ota.md`
- Hardened discovery/banner matching, cached the last known host for helper scripts, and exposed timeout knobs in `scripts/telnet_command.py`
- PlatformIO OTA/Telnet tasks rely on `scripts/ota_upload_auto.py` and cached host data instead of manual environment variables

### Fixed
- Telnet helper timeouts caused by startup-log flooding and weak Wi-Fi links
- OTA transfers stalling after telnet handshake failures by tuning streaming and helper behaviour

## [2025-01-23] - Story 002: SD Audio Playback & Jaw Synchronization Complete

### Added
- **SkitSelector Class**: Advanced weighted selection algorithm to prevent immediate skit repeats
- **Skit Selection Logic**: Time-based and play-count-based weighting for fair skit distribution
- **Repeat Prevention**: Excludes last played skit from immediate selection pool
- **Test Integration**: Built-in test function to validate skit selection functionality

### Implemented
- **SD Card Audio Playback**: Complete SD card mounting and A2DP streaming system
- **Jaw Servo Synchronization**: RMS amplitude analysis with exponential smoothing for natural speech motion
- **Audio Energy Analysis**: Real-time audio frame processing with sophisticated servo control
- **Playback Loop Validation**: End-to-end testing with multiple skit files

### Enhanced
- **Audio Player Integration**: Seamless integration with existing audio playback system
- **Servo Control**: Advanced smoothing algorithms for fluid jaw movement
- **Error Handling**: Robust error detection and recovery mechanisms
- **Thread Safety**: Proper FreeRTOS synchronization for multi-threaded operations

## [2025-10-24] - OTA System Improvements and Debugging Tools

### Added
- **Auto-Discovery OTA System**: Automatically discovers ESP32 devices and handles dynamic IP addresses
- **Enhanced Debugging Tools**: 
  - `scripts/discover_esp32.py` - Network scanner for ESP32 devices
  - `scripts/ota_upload_auto.py` - Auto-discovery OTA upload
  - `scripts/system_status.py` - System status dashboard
  - `scripts/troubleshoot.py` - Interactive troubleshooting guide
- **Telnet Authentication**: Added password support for secure telnet connections
- **PlatformIO Integration**: New custom targets for debugging tools in VS Code

### Fixed
- **OTA Partition Layout**: Optimized partition table by removing unnecessary SPIFFS partition
- **OTA Service Issues**: Resolved "No response from the ESP" errors through proper firmware initialization
- **Dynamic IP Handling**: Auto-discovery system handles ESP32 IP address changes

### Improved
- **OTA Documentation**: Enhanced `docs/ota.md` with comprehensive troubleshooting guide
- **Partition Sizes**: Increased OTA partition sizes from 1.69MB to 1.88MB each
- **Error Handling**: Better error messages and debugging information

### Documentation
- Added `OTA_TROUBLESHOOTING.md` - Comprehensive OTA troubleshooting guide
- Added `TEST_REPORT.md` - Test results for debugging tools
- Added `ai-work/ota-issues.md` - Detailed debugging log and solution documentation

## [2025-10-24] - Flash/Monitor helper workflow

### Added
- `scripts/flash_and_monitor.py` to combine firmware upload (USB or OTA) with a 30s log capture window
- PlatformIO custom targets (`Flash + Monitor (USB)` / `Flash + Monitor (OTA)`) and VS Code tasks for the new workflow

### Updated
- OTA documentation (`docs/ota.md`, README) and worklog summary to reference the helper script


## [2025-01-23] - Hardware Documentation and Proof-of-Concept Integration

### Added
- **Hardware Documentation**: Created comprehensive `docs/hardware.md` with complete BOM and pin assignments
- **Proof-of-Concept Modules**: Added three POC modules from `proof-of-concept-modules/`:
  - `esp32-esp32-matter-link/` - Matter communication between ESP32-WROVER and ESP32-C3 SuperMini
  - `finger-detector-test/` - Capacitive touch sensing for finger detection
  - `thermal-printer-test/` - 58mm thermal printer integration
- **Pin Conflict Analysis**: Identified 5 critical pin conflicts that must be resolved before hardware assembly
- **Complete Pinout References**: Full ESP32-WROVER and ESP32-C3 SuperMini pinout tables
- **Hardware Specifications**: Detailed technical specs for servo, touch sensor, thermal printer, and UART protocols

### Hardware Integration
- **Bill of Materials**: Organized by system (Audio, Visual, Motion, Sensor, Printing, Power)
- **Pin Assignments**: Documented all pins from TwoSkulls project and three POCs
- **Power Requirements**: 5V rail ≥3A for thermal printer + servo + LEDs
- **Wiring Layout**: Textual wiring connections with power distribution and signal routing

### Critical Issues Identified
- **GPIO 2**: Mouth LED vs Touch Sensor vs Built-in LED conflict
- **GPIO 18**: SD Card SCK vs UART RX vs Thermal Printer TX conflict  
- **GPIO 19**: SD Card MISO vs Thermal Printer RX conflict
- **GPIO 20/21**: Multiple UART functions conflict
- **Resolution Required**: Pin reassignment plan needed before hardware assembly

### Documentation Quality
- **Professional Structure**: Clean, well-organized sections with clear conflict indicators
- **Actionable Guidance**: Provides next steps for resolving pin conflicts
- **Comprehensive Coverage**: All hardware components and specifications documented
- **Troubleshooting Section**: Practical guidance for hardware integration issues

## [2025-01-23] - Added Cursor Slash Commands and Development Workflow

### Added
- Created `.cursor/commands/` directory with 5 custom slash commands
- `check-in-diff.md` - Git workflow with ESP32-specific considerations and CHANGELOG.md integration
- `advice-for-past-self.md` - Meta-reflection prompt for debugging and resetting approach
- `fix-difficult-issue.md` - Hardware/firmware troubleshooting guide for ESP32 projects
- `you-pick.md` - Task prioritization with Death Fortune Teller project context
- `validate.md` - Comprehensive work validation with A-F grading system
- New documentation files: `docs/arduino-vs-esp-idf.md`, `docs/stories.md`, `docs/stories/story-007-esp-idf-migration.md`

### Changed
- Updated `.gitignore` to allow `.cursor/` folder to be tracked in git
- Moved stories from `ai-work/stories.md` to `docs/stories.md` for better organization

### Development WorkflowOkay 
- Slash commands now provide structured workflow for ESP32 development
- Commands include ESP32-specific considerations (power management, hardware debugging, etc.)
- Validation system ensures work meets requirements with detailed grading

20251022: Reinstated jaw animation pipeline for verification; startup temporarily plays "Skit - imitations.wav" so the servo can be observed syncing with live audio frames

20251022: Optimized Bluetooth reconnect flow — pairing now consistently completes in ~3 seconds with audio playback kicking off immediately after connect.

20251022: Codex got the base code working in platformio! It was based off TwoSkulls, but that was build in Arduino IDE and I wasnt sure if that was REQUIRED to get the code working, especailly the esp32-a2dp library and audio streaming. But Codex got it working under platformio.

20251021: Project Created.
## [2025-10-25] - OTA/Bluetooth guard refactor

### Changed
- Pause the main loop during OTA and defer Bluetooth resume to avoid stack crashes
- Configure OTA uploads to read host IP from `DEATH_FORTUNE_HOST` env var
- Document discovery workflow and 8s Bluetooth restart window

### Added
- Config flag to enable/disable Bluetooth without rebuild
- OTA manager `isUpdating()` state for loop guards

### Fixed
- OTA crashes when Bluetooth is active during back-to-back updates
- Telnet capture newline bug in `flash_and_monitor.py`
