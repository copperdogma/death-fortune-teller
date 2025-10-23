# Changelog

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

### Development Workflow
- Slash commands now provide structured workflow for ESP32 development
- Commands include ESP32-specific considerations (power management, hardware debugging, etc.)
- Validation system ensures work meets requirements with detailed grading

20251021: Project Created.

20251022: Codex got the base code working in platformio! It was based off TwoSkulls, but that was build in Arduino IDE and I wasnt sure if that was REQUIRED to get the code working, especailly the esp32-a2dp library and audio streaming. But Codex got it working under platformio.

20251022: Optimized Bluetooth reconnect flow — pairing now consistently completes in ~3 seconds with audio playback kicking off immediately after connect.

20251022: Reinstated jaw animation pipeline for verification; startup temporarily plays "Skit - imitations.wav" so the servo can be observed syncing with live audio frames.
