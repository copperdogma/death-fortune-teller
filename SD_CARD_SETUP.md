# SD Card Setup Guide

This guide explains how to set up the SD card for the Death Fortune Teller ESP32 project.

## SD Card Structure

Your SD card should have the following structure:

```
SD Card Root/
├── config/
│   └── config.txt          # Main configuration file
├── audio/
│   ├── Initialized - Primary.wav
│   ├── Initialized - Secondary.wav
│   ├── welcome/
│   │   ├── welcome_01.wav
│   │   └── welcome_02.wav
│   └── fortune/
│       ├── fortune_01.wav
│       └── fortune_02.wav
└── printer/
    ├── logo_384w.bmp
    └── fortunes_littlekid.json
```

## Configuration File Setup

### 1. Create the config file

Create a file at `/config/config.txt` on your SD card with the following content:

```ini
# Death Fortune Teller Configuration
# Copy this file to your SD card as /config/config.txt

# Basic settings
role=primary
speaker_name=JBL Flip 5
speaker_volume=100

# 🛜 WiFi settings (Phase 1: OTA Upload)
# Uncomment and fill in your WiFi credentials to enable wireless features
wifi_ssid=YourNetworkName
wifi_password=YourPassword
ota_hostname=death-fortune-teller
ota_password=your_secure_ota_password

# Remote debug settings (Phase 2: Wireless Serial Monitor)
#remote_debug_enabled=true
#remote_debug_port=23
```

### 2. Configure WiFi Settings

**Required for 🛜 wireless features:**
- `wifi_ssid`: Your WiFi network name (SSID)
- `wifi_password`: Your WiFi password

**Required OTA settings:**
- `ota_hostname`: Hostname for the ESP32 (default: "death-fortune-teller")
- `ota_password`: Password for OTA updates (must be non-empty to enable OTA)

### 3. Example Configurations

#### Minimal Configuration (No WiFi)
```ini
role=primary
speaker_name=JBL Flip 5
speaker_volume=100
```

#### With WiFi and OTA Password Protection
```ini
role=primary
speaker_name=JBL Flip 5
speaker_volume=100
wifi_ssid=MyHomeNetwork
wifi_password=MyPassword123
ota_hostname=death-fortune-teller
ota_password=MyOTAPassword456
```

## Testing WiFi Connection

After uploading firmware with WiFi credentials:

1. **Check Serial Monitor** for WiFi status messages:
   ```
   🛜 Checking WiFi configuration...
      SSID: MyHomeNetwork
      Password: [SET]
      OTA Hostname: death-fortune-teller
      OTA Password: [SET]
   🛜 WiFi credentials found, initializing WiFi...
   🛜 WiFi manager started, attempting connection...
   🛜 WiFi connected! IP: 192.168.1.100
   🛜 Hostname: death-fortune-teller
   🔄 OTA manager started
   🔄 OTA password protection enabled

If `ota_password` is omitted, the firmware logs `OTA password missing; OTA disabled` and the ESP32 will reject OTA uploads until a password is supplied on the SD card.
   ```

2. **Test OTA Upload** (after WiFi connects):
   ```bash
   pio run --environment esp32dev_ota --target upload
   ```

3. **Check Status Logs** (every 5 seconds):
   ```
   Status: Free mem: 62124 bytes, A2DP connected: true, Retrying: false, Audio playing: false, Init queued: false, Init played: true, 🛜 WiFi: connected (192.168.1.100)
   ```

## Troubleshooting

### WiFi Won't Connect
- Verify SSID and password are correct
- Ensure you're using 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check serial output for specific error messages
- Try moving closer to your router

### OTA Upload Fails
- Verify ESP32 and computer are on the same network
- Check hostname resolves: `ping death-fortune-teller.local`
- Try using IP address instead of hostname
- Ensure firewall allows mDNS and espota traffic

### No WiFi Credentials Found
If you see:
```
🛜 WiFi credentials incomplete or missing:
   - wifi_ssid not set in config.txt
   - wifi_password not set in config.txt
🛜 WiFi and OTA disabled - system will work normally without wireless features
```

This means the system will work normally without wireless features. To enable WiFi:
1. Add `wifi_ssid=YourNetworkName` to config.txt
2. Add `wifi_password=YourPassword` to config.txt
3. Reboot the ESP32

## Notes

- **🛜 Bluetooth Priority**: Bluetooth A2DP and WiFi can run simultaneously without conflict
- **Security**: OTA password is optional but recommended for production
- **Fallback**: System works normally without WiFi - all core features remain functional
- **Performance**: WiFi adds minimal overhead when not actively uploading
