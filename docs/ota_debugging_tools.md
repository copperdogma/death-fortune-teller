# New OTA Debugging Tools

## Quick Start for OTA Issues
If OTA isn't working, run these commands in order:

```bash
# 1. Discover ESP32 devices on network
python scripts/discover_esp32.py

# 2. Check system status
python scripts/system_status.py

# 3. Test connection with auto-discovery
python scripts/telnet_command.py status --auto-discover

# 4. If still having issues, run interactive troubleshooting
python scripts/troubleshoot.py
```

## Available Debugging Tools

### 1. **ESP32 Discovery** (`python scripts/discover_esp32.py`)
- Scans network for all ESP32 devices
- Shows which devices have telnet servers running
- Identifies active vs stale devices
- Provides clear status for each device

### 2. **System Status Dashboard** (`python scripts/system_status.py`)
- Auto-discovers ESP32 and shows complete status
- Displays WiFi, OTA, telnet, SD card, and audio status
- Provides next steps and commands
- Good for quick health check

### 3. **Interactive Troubleshooting** (`python scripts/troubleshoot.py`)
- Step-by-step troubleshooting guide
- Checks power, SD card, WiFi, and telnet
- Provides specific solutions for common issues
- Interactive prompts to diagnose problems

### 4. **Enhanced Telnet Commands** (`python scripts/telnet_command.py`)
- Auto-discovery feature (`--auto-discover`)
- Better error messages and help system
- Environment variable support
- All existing telnet commands work as before

## PlatformIO Integration

The new debugging tools are available as PlatformIO Project Tasks:

**VS Code**: PlatformIO panel → Project Tasks → esp32dev_ota → New debugging tasks
- `Discover ESP32` - Scan network for ESP32 devices
- `System Status` - Show system status dashboard  
- `Troubleshoot` - Interactive troubleshooting guide

**CLI**: 
```bash
pio run -e esp32dev_ota -t discover_esp32
pio run -e esp32dev_ota -t system_status
pio run -e esp32dev_ota -t troubleshoot
```

## Common Issues & Solutions

### Issue: "No ESP32 found" or "Could not auto-discover"
**Symptoms**: Discovery script finds no active ESP32 devices
**Solutions**:
1. Check ESP32 is powered on and connected to WiFi
2. Verify SD card is inserted with proper config
3. Wait 30-60 seconds for ESP32 to fully boot
4. Power cycle the ESP32 if needed

### Issue: "Telnet connection timeout" or "Connection closed"
**Symptoms**: ESP32 is reachable but telnet connections fail
**Solutions**:
1. ESP32 may be in error state - power cycle required
2. Check if telnet server is running on ESP32
3. Verify WiFi connection is stable
4. Check for error messages in ESP32 logs

### Issue: "OTA upload fails" or "No response from ESP"
**Symptoms**: OTA upload attempts fail with timeout
**Solutions**:
1. Ensure ESP32 is connected to WiFi
2. Check OTA server is running (telnet should work)
3. Verify OTA password is correct
4. Power cycle ESP32 if in error state

## Environment Variables

Set these to avoid typing IP addresses:

```bash
# Set default ESP32 IP
export DEATH_FORTUNE_HOST=192.168.86.46

# Now you can use:
python scripts/telnet_command.py status
python scripts/telnet_command.py log
```

## Prevention

To avoid OTA issues in the future:

1. **Use static IPs** - Set up DHCP reservations for consistent addressing
2. **Keep configs consistent** - Use same WiFi credentials across devices
3. **Test regularly** - Run discovery script periodically to check status
4. **Document IPs** - Keep track of ESP32 IP addresses
5. **Use environment variables** - Set DEATH_FORTUNE_HOST for convenience
