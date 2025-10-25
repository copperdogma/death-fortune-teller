# Story: Configuration Management System

**Status**: To Do

---

## Related Requirement
[docs/spec.md §8 Config.txt Keys](../spec.md#8-configtxt-keys) - Configuration file format and keys

## Alignment with Design
[docs/spec.md §4 Assets & File Layout](../spec.md#4-assets--file-layout) - SD card structure with `/config/config.txt`

## Acceptance Criteria
- System loads and parses `/config/config.txt` on startup with all required configuration keys
- Configuration validation ensures all required keys are present and valid
- Runtime configuration access provides type-safe access to all settings
- Default values are used when configuration is missing or invalid
- [ ] User must sign off on functionality before story can be marked complete

## Tasks
- [ ] **Implement ConfigManager Class**: Create configuration management system with file parsing
- [ ] **Add Configuration Keys**: Implement all keys from spec.md §8 (speaker, servo, LED, timing, printer)
- [ ] **Add Validation**: Validate configuration values and provide meaningful error messages
- [ ] **Add Default Values**: Provide sensible defaults for all configuration options
- [ ] **Add Runtime Access**: Provide type-safe getter methods for all configuration values
- [ ] **Test Configuration**: Test with valid, invalid, and missing configuration files

## Technical Implementation Details

### Configuration File Format
The system must parse `/config/config.txt` with the following key categories:

**Basic Settings:**
- `role`: System role identifier (default: "primary")
- `speaker_name`: Bluetooth speaker name (default: "JBL Flip 5")
- `speaker_volume`: Audio volume level (default: 100)

**WiFi Settings:**
- `wifi_ssid`: Network name for OTA updates
- `wifi_password`: Network password
- `ota_hostname`: OTA update hostname (default: "death-fortune-teller")
- `ota_password`: OTA update password

**Remote Debug Settings:**
- `remote_debug_enabled`: Enable wireless serial monitor (default: false)
- `remote_debug_port`: Telnet port for remote debug (default: 23)

**Hardware Settings:**
- `servo_us_min`: Servo minimum microseconds (default: 500)
- `servo_us_max`: Servo maximum microseconds (default: 2500)
- `cap_threshold`: Capacitive sensor threshold (default: 0.002)

**Timing Settings:**
- `finger_wait_ms`: Finger detection timeout (default: 6000)
- `snap_delay_min_ms`: Minimum snap delay (default: 1000)
- `snap_delay_max_ms`: Maximum snap delay (default: 3000)
- `cooldown_ms`: Post-fortune cooldown (default: 12000)

**Printer Settings:**
- `printer_baud`: Thermal printer baud rate (default: 9600)
- `printer_logo`: Logo file path (default: "/printer/logo_384w.bmp")
- `fortunes_json`: Fortune file path (default: "/printer/fortunes_littlekid.json")

### Configuration Management Requirements

**File Parsing:**
- Parse key=value pairs from config file
- Support comments (lines starting with #)
- Handle missing or malformed entries gracefully
- Provide meaningful error messages for invalid configurations

**Validation Rules:**
- Validate required keys are present
- Check numeric ranges (servo min < max, cap threshold 0.001-0.1)
- Validate timing values (snap delay min < max)
- Ensure file paths are valid

**Default Values:**
- Provide sensible defaults for all configuration options
- Use defaults when configuration file is missing
- Use defaults when validation fails
- Log when defaults are used

**Runtime Access:**
- Provide type-safe access to all configuration values
- Cache configuration in memory for performance
- Support configuration reloading if needed
- Handle configuration changes gracefully

### Integration Requirements

**System Integration:**
- Load configuration during system startup
- Make configuration available to all components
- Handle configuration errors without system failure
- Log configuration loading and validation results

**Component Integration:**
- Servo controller uses servo timing values
- Audio system uses speaker name and volume
- Finger sensor uses capacitive threshold
- Thermal printer uses baud rate and file paths
- State machine uses timing values
- WiFi system uses network credentials

**Error Handling:**
- Graceful fallback to defaults on configuration errors
- Clear error messages for debugging
- Continue system operation with defaults when possible
- Log all configuration issues for troubleshooting

## Notes
- **Configuration File**: Located at `/config/config.txt` on SD card
- **Validation**: All required keys must be present and valid
- **Defaults**: Sensible defaults provided for all configuration options
- **Type Safety**: Type-safe getter methods for all configuration values
- **Error Handling**: Graceful fallback to defaults when configuration is invalid
- **Performance**: Configuration loaded once at startup, cached in memory
- **Pin Assignments**: All pin assignments are hardcoded in firmware and not configurable
- **Config File Updates**: The `/config/config.txt` file must be updated with final values and copied to the skull's SD card

## Dependencies
- SD card mounted and accessible
- Configuration file present on SD card
- All other system components (for integration testing)

## Testing Strategy
1. **Valid Configuration**: Test with complete, valid configuration file
2. **Missing Keys**: Test with configuration missing required keys
3. **Invalid Values**: Test with invalid numeric ranges and values
4. **Default Fallback**: Test behavior when configuration file is missing
5. **Integration Testing**: Verify configuration values are used by other components
