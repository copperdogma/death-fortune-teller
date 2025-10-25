# Changelog

## [2025-10-25] - Telnet stability and OTA workflow hardening

### Added
- `ai-work/issues/telnet-issues.md` log capturing systematic debugging steps and resolution

### Changed
- Disabled Bluetooth via build flags during OTA/test builds to free RF bandwidth and heap
- Defaulted RemoteDebug auto-streaming to off and documented short-timeout usage in `docs/ota.md`
- Hardened discovery/banner matching and exposed timeout knobs in `scripts/telnet_command.py`

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
