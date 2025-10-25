# OTA Troubleshooting Guide

## Quick Start

If OTA isn't working, run these commands in order:

```bash
# 1. Discover ESP32 devices
python scripts/discover_esp32.py

# 2. Check system status
python scripts/system_status.py

# 3. Test connection
python scripts/telnet_command.py status --host <ESP32_IP>

# 4. If still having issues
python scripts/troubleshoot.py
```

## Common Issues & Solutions

### Issue: "Host is down" or "Connection refused"

**Symptoms:**
- `python scripts/telnet_command.py status` fails
- Cannot connect to ESP32

**Solutions:**
1. **Check ESP32 is powered on**
   - Wall adapter connected
   - LEDs on ESP32 are lit
   - Skull moves when powered on

2. **Check SD card is inserted**
   - SD card properly inserted
   - Contains `/config/config.txt` with WiFi credentials
   - Config file has correct WiFi SSID and password

3. **Check WiFi connection**
   - ESP32 connected to your network
   - WiFi credentials are correct
   - Network is available

4. **Wait for boot**
   - ESP32 needs 30-60 seconds to boot and connect
   - Try again after waiting

### Issue: "No ESP32 found" or "Could not auto-discover"

**Symptoms:**
- Discovery script finds no active ESP32
- Multiple ESP32 devices but none are active

**Solutions:**
1. **Run discovery manually**
   ```bash
   python scripts/discover_esp32.py
   ```

2. **Check which devices have telnet**
   - Look for devices with "Telnet: ✅"
   - Try connecting to those IPs

3. **Power cycle ESP32**
   - Unplug and replug wall adapter
   - Wait 30 seconds for boot

### Issue: "Telnet connection timeout"

**Symptoms:**
- ESP32 is reachable (ping works)
- Telnet connection times out
- Connection closes immediately

**Solutions:**
1. **Check telnet server is running**
   - ESP32 must be connected to WiFi
   - Telnet server starts after WiFi connection

2. **Check firewall**
   - Port 23 might be blocked
   - Try from different network

3. **Check ESP32 status**
   - Look for error messages in logs
   - Check if ESP32 is in error state

## Step-by-Step Troubleshooting

### Step 1: Basic Checks
```bash
# Check if ESP32 is powered on
# - LEDs should be lit
# - Skull should move when powered on

# Check SD card
# - Properly inserted
# - Contains config.txt with WiFi credentials
```

### Step 2: Network Discovery
```bash
# Find all ESP32 devices
python scripts/discover_esp32.py

# Look for devices with "Telnet: ✅"
# Note the IP address of the active device
```

### Step 3: Test Connection
```bash
# Test connection to discovered IP
python scripts/telnet_command.py status --host <ESP32_IP>

# If successful, you should see system status
```

### Step 4: System Status
```bash
# Get complete system status
python scripts/system_status.py

# This shows WiFi, OTA, and telnet status
```

### Step 5: Interactive Troubleshooting
```bash
# Run interactive troubleshooting
python scripts/troubleshoot.py

# Follow the prompts to diagnose issues
```

## Configuration Requirements

### SD Card Structure
```
SD Card Root/
└── config/
    └── config.txt
```

### Required config.txt
```ini
# Basic settings
role=primary
speaker_name=JBL Flip 5
speaker_volume=100

# WiFi Settings (REQUIRED for OTA)
wifi_ssid=YourNetworkName
wifi_password=YourPassword
ota_hostname=death-fortune-teller
ota_password=YourSecurePassword
```

## Environment Variables

Set these to avoid typing IP addresses:

```bash
# Set default ESP32 IP
export DEATH_FORTUNE_HOST=192.168.86.46

# Now you can use:
python scripts/telnet_command.py status
python scripts/telnet_command.py log
```

## Advanced Troubleshooting

### Check ESP32 Logs
```bash
# Get recent logs
python scripts/telnet_command.py log --host <ESP32_IP>

# Get startup logs
python scripts/telnet_command.py startup --host <ESP32_IP>

# Get last 10 log entries
python scripts/telnet_command.py tail 10 --host <ESP32_IP>
```

### Manual Telnet Connection
```bash
# Connect directly to ESP32
telnet <ESP32_IP> 23

# Available commands:
# status - System status
# wifi - WiFi connection info
# ota - OTA update status
# log - Recent logs
# startup - Startup logs
# help - Show all commands
```

### Network Diagnostics
```bash
# Ping ESP32
ping <ESP32_IP>

# Check if port 23 is open
telnet <ESP32_IP> 23

# Check network connectivity
ping 8.8.8.8
```

## Still Having Issues?

1. **Check the logs** - Look for error messages
2. **Power cycle everything** - ESP32 and router
3. **Try different network** - Test on different WiFi
4. **Check hardware** - SD card, USB cable, power adapter
5. **Update firmware** - Flash latest version via USB

## Prevention

To avoid these issues in the future:

1. **Use static IPs** - Set up DHCP reservations
2. **Keep configs consistent** - Use same WiFi credentials
3. **Test regularly** - Run discovery script periodically
4. **Document IPs** - Keep track of ESP32 IP addresses
5. **Use environment variables** - Set DEATH_FORTUNE_HOST
