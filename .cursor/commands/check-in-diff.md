# Check In Changes

NOTE: If you need to review changes, use `git diff` or `git log --oneline` to see what was modified.

## ESP32 Project Check-in Process

1. **Review Changes**
   - Check what was added/modified: `git diff --name-only`
   - Verify all changes are appropriate (no temp files, debug logs, or secrets)
   - Ensure no sensitive data like WiFi passwords or API keys are committed

2. **Update CHANGELOG.md**
   - Add entry with current date and description of changes
   - Use format: `## [YYYY-MM-DD] - Brief description`
   - Include specific features, fixes, or improvements made

3. **Commit Changes**
   ```bash
   git add .
   git commit -m "Brief description of changes"
   ```

4. **Push to Repository**
   ```bash
   git push origin main
   ```

## ESP32-Specific Considerations

- **Firmware Changes**: Note any changes to `src/` files, `platformio.ini`, or partition tables
- **Hardware Changes**: Document any wiring, component, or configuration changes
- **SD Card Files**: Ensure any new audio files or configs are properly documented
- **OTA Updates**: If this affects OTA functionality, note it in the changelog

## Example CHANGELOG Entry

```markdown
## [2025-01-23] - Improved servo control and audio sync

### Added
- Enhanced servo animation timing for jaw movement
- New audio file validation in SD card manager

### Fixed
- Servo jitter during audio playback
- Memory leak in audio player buffer management

### Changed
- Updated servo control algorithm for smoother movement
- Improved error handling in thermal printer module
```

## Pre-commit Checklist

- [ ] All temporary debug code removed
- [ ] No hardcoded credentials or sensitive data
- [ ] CHANGELOG.md updated with meaningful description
- [ ] Code compiles without errors (`pio run`)
- [ ] No serial monitor output or debug prints left in production code
