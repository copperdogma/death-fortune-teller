# Death, the Fortune Telling Skeleton

An animatronic skeleton that tells fortunes, built with ESP32 and PlatformIO.

## Project Overview

This project is based on the TwoSkulls codebase but adapted for a single skull with Matter control integration. The skull features:

- **ESP32-WROVER/WROOM** microcontroller
- **A2DP Bluetooth** audio streaming
- **SD Card** audio storage
- **Servo-controlled jaw** animation
- **Capacitive finger sensor** for fortune reading
- **Thermal printer** for fortune output
- **LED eye control** for visual feedback
- **UART communication** for Matter node control

## Hardware Setup

### ESP32 Board Configuration
- **Board**: ESP32-WROVER or ESP32-WROOM
- **Framework**: Arduino
- **Partition Scheme**: `partitions/fortune_ota.csv` (dual 1.7 MB OTA slots + 512 KB SPIFFS log storage)

### Pin Assignments
```cpp
const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED
const int SERVO_PIN = 15;       // Servo control pin
const int MOUTH_LED_PIN = 2;    // Mouth LED pin
const int CAP_SENSE_PIN = 4;    // Capacitive finger sensor pin
const int PRINTER_TX_PIN = 21;  // Thermal printer TX pin
const int PRINTER_RX_PIN = 20;  // Thermal printer RX pin
```

## PlatformIO Setup

### Prerequisites
1. **PlatformIO IDE** installed (VS Code extension recommended)
2. **ESP32 board** connected via USB
3. **SD card** with audio files and configuration

### Installation Steps

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd death-fortune-teller
   ```

2. **Open in PlatformIO:**
   - Open VS Code
   - Open the project folder
   - PlatformIO should automatically detect the project

3. **Install dependencies:**
   PlatformIO will automatically install the required libraries:
   - `arduinoFFT@^1.5.7`
   - `SD@^2.0.0`
   - `ArduinoJson@^6.21.0`
   - `ESP32Servo@^3.0.0`
   - `ESP32-A2DP@^1.0.0` (from GitHub)

### Upload Configuration

#### Maximum Supported Baud Rates
- **Recommended**: `460800` baud (tested and stable)
- **Maximum tested**: `921600` baud (may fail on some hardware)
- **Default**: `115200` baud (slow but reliable)

#### Upload Process
0. **Hardware sanity check:** Confirm the ESP32 is powered and connected over USB (or OTA-ready) before proceeding. If it is unplugged, plug it in now.
1. **Always stop monitoring first:**
   ```bash
   lsof /dev/cu.usbserial-*
   pkill -f "cat /dev/cu.usbserial-10"
   ```

2. **Upload firmware:**
   ```bash
   pio run --target upload
   ```

3. **Monitor output:** (serial runs at 115200 baud)
   ```bash
   pio device monitor -b 115200
   ```

#### OTA (Over-the-Air) Quick Start
- Set a strong `ota_password` value in `config.txt` on the SD card. OTA remains disabled until this value is present.
- Create `platformio.local.ini` in the project root with your OTA password (`upload_flags = --auth=${sysenv.ESP32_OTA_PASSWORD}`) and export the env var before running `pio run -e esp32dev_ota -t upload`.
- The firmware mutes audio and pauses Bluetooth retries during OTA so updates are stable.
- Full instructions (custom tasks, telnet helpers, troubleshooting) live in [`docs/ota.md`](docs/ota.md).

### PlatformIO Configuration (`platformio.ini`)

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 460800

; Partition scheme with dual 1.7 MB OTA slots + 512 KB SPIFFS log storage
board_build.partitions = partitions/fortune_ota.csv

; Libraries
lib_deps = 
    arduinoFFT@^1.5.7
    SD@^2.0.0
    ArduinoJson@^6.21.0
    ESP32Servo@^3.0.0
    https://github.com/pschatzmann/ESP32-A2DP

; Build flags
build_flags = 
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=0

; Monitor configuration
monitor_filters = 
    time
    default
```

## Software Architecture

### Core Components

1. **AudioPlayer** - Manages audio playback from SD card
2. **BluetoothController** - Handles A2DP Bluetooth connections with retry logic
3. **ServoController** - Controls jaw animation
4. **LightController** - Manages LED eye effects
5. **UARTController** - Handles Matter node communication
6. **ThermalPrinter** - Prints fortunes
7. **FingerSensor** - Detects finger placement
8. **FortuneGenerator** - Generates fortunes from JSON templates

### State Machine

```cpp
enum class DeathState {
    IDLE,
    PLAY_WELCOME,
    FORTUNE_FLOW,
    PLAY_PROMPT,
    MOUTH_OPEN,
    WAIT_FINGER,
    SNAP,
    PRINT_FORTUNE,
    COOLDOWN
};
```

### Bluetooth A2DP Integration

The project successfully integrates ESP32-A2DP library with PlatformIO:

- **Connection retry logic** - Automatically retries Bluetooth connection every 5 seconds
- **Status monitoring** - Periodic logging of connection status
- **Audio streaming** - Real-time audio data provision to A2DP source

## Development Notes

### Key Achievements
- ✅ **PlatformIO compilation** - Successfully compiled with all dependencies
- ✅ **ESP32-A2DP integration** - Working A2DP Bluetooth implementation
- ✅ **Faster upload speeds** - 460800 baud rate (4x faster than default)
- ✅ **Connection retry logic** - Automatic Bluetooth reconnection
- ✅ **Memory optimization** - 36.4% flash usage with full A2DP library

### Troubleshooting

#### Upload Issues
- **Port contention**: Always kill monitoring processes before uploading
- **Bootloader mode**: Manually put ESP32 in bootloader mode if needed
- **Baud rate**: Use 460800 for reliable uploads

#### Bluetooth Issues
- **Connection retry**: System automatically retries every 5 seconds
- **Status monitoring**: Check serial output for connection status
- **Speaker pairing**: Ensure Bluetooth speaker is in pairing mode

#### Compilation Issues
- **Library conflicts**: ESP32-A2DP library properly integrated
- **Frame definitions**: Using ESP32-A2DP Frame struct
- **Callback signatures**: Static callbacks for ESP32-A2DP compatibility

## File Structure

```
src/
├── main.cpp                    # Main application logic
├── audio_player.h/.cpp         # Audio playback management
├── bluetooth_controller.h/.cpp # A2DP Bluetooth control
├── servo_controller.h/.cpp     # Jaw servo control
├── light_controller.h/.cpp     # LED eye control
├── uart_controller.h/.cpp      # UART communication
├── thermal_printer.h/.cpp      # Thermal printer control
├── finger_sensor.h/.cpp        # Capacitive finger detection
├── fortune_generator.h/.cpp    # Fortune generation
├── sd_card_manager.h/.cpp      # SD card management
├── config_manager.h/.cpp       # Configuration management
└── skull_audio_animator.h/.cpp # Audio-synced jaw animation
```

## Configuration Files

### SD Card Structure
```
/audio/
├── Initialized.wav
├── welcome/
│   └── welcome_01.wav
├── fortune/
│   └── fortune_01.wav
└── /printer/
    └── fortunes_littlekid.json
```

### Settings File
The system loads configuration from SD card with settings for:
- Bluetooth speaker name
- Servo min/max degrees
- Speaker volume
- Fortune templates
- WiFi credentials
- OTA password (required for OTA updates)

## Future Development

- [ ] **Matter integration** - Full Matter node implementation
- [ ] **Audio file management** - Dynamic audio loading
- [ ] **Fortune templates** - Expandable fortune generation
- [ ] **Power management** - Battery operation support
- [ ] **Web interface** - Configuration via web interface

## Credits

Based on the TwoSkulls project, adapted for single skull operation with enhanced Matter control integration.
