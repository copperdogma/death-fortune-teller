<!-- 58791e9e-2bfc-4f1f-8b0a-95ad08241c40 dd69010f-7acb-4497-90cb-03a991f3068d -->
# ESP32 Wireless Features Implementation

## Overview

This plan implements two wireless features that share common WiFi infrastructure:

1. **Phase 1:** Over-The-Air (OTA) firmware updates
2. **Phase 2:** Wireless serial monitoring via telnet

Both phases build on shared WiFi connectivity setup.

---

## Shared Prerequisites

- WiFi network credentials (SSID and password)
- ESP32 and computer must be on the same network
- Initial firmware upload must be done via USB

---

# PHASE 1: Wireless OTA Upload

Enable wireless firmware uploads to the ESP32 via WiFi using ArduinoOTA library and PlatformIO's OTA support.

## Phase 1 Implementation Steps

### 1.1 Add WiFi Configuration

Add WiFi credentials to the config file at `/config/config.txt`:

```
wifi_ssid=YourNetworkName
wifi_password=YourPassword
ota_hostname=death-fortune-teller
ota_password=optional_ota_password
```

### 1.2 Update ConfigManager

Modify `src/config_manager.h` and `src/config_manager.cpp`:

- Add methods to retrieve WiFi SSID, password, OTA hostname, and OTA password
- Parse new config keys from config.txt

### 1.3 Create WiFi Manager Module

Create new files `src/wifi_manager.h` and `src/wifi_manager.cpp`:

- Handle WiFi connection with retry logic
- Non-blocking WiFi connection to avoid blocking main loop
- Provide connection status checks
- Handle reconnection if WiFi drops
- This will be shared between Phase 1 and Phase 2

### 1.4 Create OTA Manager Module

Create new files `src/ota_manager.h` and `src/ota_manager.cpp`:

- Include ArduinoOTA library
- Initialize OTA with hostname and password
- Provide status callbacks for OTA events (start, progress, end, error)
- Handle OTA updates in loop

### 1.5 Integrate Phase 1 into main.cpp

Modify `src/main.cpp`:

- Include WiFi manager and OTA manager headers
- Initialize WiFi manager in `setup()` after config loads
- Initialize OTA manager in `setup()` after WiFi connects
- Call WiFi and OTA update handlers in `loop()`
- Add status LED feedback during OTA updates (optional)

### 1.6 Update PlatformIO Configuration for OTA

Modify `platformio.ini`:

- Add `upload_protocol = espota` for OTA environment
- Create a second environment `[env:esp32dev_ota]` that inherits from `[env:esp32dev]`
- Add `upload_port` with ESP32's IP address or mDNS hostname
- Add `upload_flags` for OTA password if configured

Example:

```ini
[env:esp32dev_ota]
platform = espressif32
board = esp32dev
framework = arduino
upload_protocol = espota
upload_port = death-fortune-teller.local
upload_flags = 
    --auth=optional_ota_password
```

## Phase 1 Usage

### Initial Setup

1. Upload firmware via USB: `pio run --target upload`
2. ESP32 will connect to WiFi and advertise OTA service
3. Check serial monitor for IP address

### Subsequent Uploads

Use the OTA environment:

```bash
pio run --environment esp32dev_ota --target upload
```

Or discover devices on network:

```bash
pio device list --mdns
```

## Phase 1 Testing

- Verify WiFi connection on boot
- Confirm mDNS hostname resolves
- Test OTA upload successfully
- Verify fallback to USB still works

---

# PHASE 2: Wireless Serial Monitor

Enable wireless serial output monitoring over WiFi using a telnet server, allowing remote debug output viewing without USB connection.

## Phase 2 Implementation Steps

### 2.1 Add Remote Debug Configuration

Add to config file at `/config/config.txt`:

```
remote_debug_enabled=true
remote_debug_port=23
```

### 2.2 Update ConfigManager for Phase 2

Modify `src/config_manager.h` and `src/config_manager.cpp`:

- Add methods to retrieve remote debug settings
- Parse new config keys

### 2.3 Add RemoteDebug Library

Add to `platformio.ini` lib_deps:

```ini
lib_deps = 
    arduinoFFT@^1.5.7
    SD@^2.0.0
    ArduinoJson@^6.21.0
    ESP32Servo@^3.0.0
    https://github.com/pschatzmann/ESP32-A2DP
    RemoteDebug@^3.0.5
```

### 2.4 Create Remote Debug Manager Module

Create new files `src/remote_debug_manager.h` and `src/remote_debug_manager.cpp`:

**Features:**

- Initialize RemoteDebug library with hostname
- Set up log levels (Verbose, Debug, Info, Warning, Error)
- Create custom commands for debugging (e.g., trigger test sequences)
- Handle client connections/disconnections
- Provide macros to simplify usage throughout codebase
- Gracefully handle when WiFi not connected

**Helper macros in header:**

```cpp
#define DEBUG_V(fmt, ...) if(RemoteDebug.isActive(RemoteDebug.VERBOSE)) RemoteDebug.printf(fmt, ##__VA_ARGS__)
#define DEBUG_D(fmt, ...) if(RemoteDebug.isActive(RemoteDebug.DEBUG)) RemoteDebug.printf(fmt, ##__VA_ARGS__)
#define DEBUG_I(fmt, ...) if(RemoteDebug.isActive(RemoteDebug.INFO)) RemoteDebug.printf(fmt, ##__VA_ARGS__)
#define DEBUG_W(fmt, ...) if(RemoteDebug.isActive(RemoteDebug.WARNING)) RemoteDebug.printf(fmt, ##__VA_ARGS__)
#define DEBUG_E(fmt, ...) if(RemoteDebug.isActive(RemoteDebug.ERROR)) RemoteDebug.printf(fmt, ##__VA_ARGS__)
```

### 2.5 Add Custom Debug Commands (Optional)

Implement custom telnet commands in RemoteDebugManager:

- `status` - Show system status
- `test_welcome` - Trigger welcome sequence
- `test_fortune` - Trigger fortune sequence
- `show_config` - Display current configuration
- `heap` - Show free heap memory
- `wifi` - Show WiFi status and IP

### 2.6 Integrate Phase 2 into main.cpp

Modify `src/main.cpp`:

- Include RemoteDebugManager header
- Initialize RemoteDebugManager in `setup()` after WiFi connects
- Call RemoteDebug handler in `loop()`: `RemoteDebug.handle()`
- Keep existing Serial.print() calls (dual output)
- Optionally add DEBUG_X() macros for WiFi-only verbose messages

## Phase 2 Usage

### Connecting via Telnet

**macOS/Linux:**

```bash
telnet death-fortune-teller.local 23
# or use IP address
telnet 192.168.1.xxx 23
```

**Windows:**

```powershell
# Enable telnet client first (one-time):
# Control Panel > Programs > Turn Windows features on/off > Telnet Client

telnet death-fortune-teller.local 23
```

### Using RemoteDebug Commands

Once connected:

- Type `?` or `help` to see available commands
- Type `m` to change debug level (verbose/debug/info/warning/error)
- Type `r` to reset board
- Custom commands as implemented

### Disconnecting

- Press `Ctrl+]` then type `quit` (macOS/Linux)
- Press `Ctrl+C` (most systems)

## Phase 2 Testing

- Connect via telnet and verify output
- Test custom commands work
- Verify dual output (USB and WiFi simultaneously)
- Test reconnection after disconnect

---

## Safety Considerations (Both Phases)

- OTA updates will be ignored during active fortune-telling sequences (busy state)
- WiFi connection won't interfere with Bluetooth A2DP (different radio)
- Failed OTA updates automatically rollback to previous firmware
- Serial monitoring via USB still works normally when connected
- If WiFi fails to connect, system continues normal operation
- No sensitive information should be logged (WiFi passwords, etc.)
- Connection limit: 1 telnet client at a time to conserve memory

## Performance Considerations

- WiFi/OTA adds ~80KB to firmware size
- RemoteDebug adds ~40KB to firmware size
- Minimal CPU overhead when features not actively in use
- Non-blocking implementations won't affect timing-sensitive operations

## Troubleshooting

**WiFi won't connect:**

- Verify SSID and password in config.txt
- Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check serial output for error messages

**OTA won't work:**

- Verify ESP32 and computer on same network
- Check hostname resolves: `ping death-fortune-teller.local`
- Try IP address instead of hostname
- Ensure firewall allows mDNS and espota traffic

**Telnet won't connect:**

- Complete Phase 1 first (WiFi must be working)
- Check port 23 not blocked by firewall
- Try IP address instead of hostname
- Verify RemoteDebug initialized in serial output

---

## Implementation Order

1. **Phase 1 Steps 1.1-1.6:** WiFi and OTA functionality
2. **Test Phase 1 thoroughly**
3. **Phase 2 Steps 2.1-2.6:** Remote debug functionality  
4. **Test Phase 2 thoroughly**
5. **Integration test both features together**

### To-dos

- [ ] Add WiFi configuration keys to config.txt and update ConfigManager
- [ ] Create WiFi manager module with connection handling
- [ ] Create OTA manager module with ArduinoOTA initialization
- [ ] Integrate WiFi and OTA managers into main.cpp
- [ ] Add OTA upload environment to platformio.ini
- [ ] Test Phase 1: WiFi connection and OTA upload
- [ ] Add remote debug configuration to config.txt and ConfigManager
- [ ] Add RemoteDebug library to platformio.ini lib_deps
- [ ] Create RemoteDebugManager module with helper macros
- [ ] Add custom debug commands to RemoteDebugManager
- [ ] Integrate RemoteDebugManager into main.cpp
- [ ] Test Phase 2: telnet connection and remote debug output
- [ ] Test both features working simultaneously
- [ ] Update documentation with WiFi setup instructions

### To-dos

- [ ] Add WiFi configuration keys to config.txt and update ConfigManager
- [ ] Create WiFi manager module with connection handling
- [ ] Create OTA manager module with ArduinoOTA initialization
- [ ] Integrate WiFi and OTA managers into main.cpp
- [ ] Add OTA upload environment to platformio.ini
- [ ] Test Phase 1: WiFi connection and OTA upload
- [ ] Add remote debug configuration to config.txt and ConfigManager
- [ ] Add RemoteDebug library to platformio.ini lib_deps
- [ ] Create RemoteDebugManager module with helper macros
- [ ] Add custom debug commands to RemoteDebugManager
- [ ] Integrate RemoteDebugManager into main.cpp
- [ ] Test Phase 2: telnet connection and remote debug output
- [ ] Test both features working simultaneously
- [ ] Update documentation with WiFi setup instructions