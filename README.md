# Death, the Fortune Telling Skeleton

An animatronic skeleton that tells fortunes, built with ESP32 and PlatformIO.

## 2026 single-WROVER rebuild

The new active implementation is [native ESP-IDF firmware](firmware/README.md),
based on the newer TwoSkulls single-WROVER code. The WROVER handles Matter and
A2DP itself; the C3 Super Mini is retired from this runtime. Death’s fortune flow,
SD content, calibrated jaw control and a rewritten paced text printer are
integrated. See [Story015](docs/stories/story-015-single-wrover-2026-rebuild.md)
for implementation/verification status and the remaining physical bench checks.
The demo controls are **Start**, **Waiting** and **Dead Stop**; see
[Story016](docs/stories/story-016-demo-matter-start-waiting-stop.md). Start runs
the whole fortune sequence and cooldown automatically; Dead Stop cancels back
to Waiting. The separate Box of Death can turn Start On through a Home automation.

Build the replacement with `idf.py -C firmware build` after activating ESP-IDF5.5.4.
The Arduino/PlatformIO instructions below describe the retained historical
firmware. They do not build the replacement. The native partition layout requires
a deliberate backed-up first migration; see the native README before flashing.

## Debugging the native firmware

The active 2026 native firmware automatically retains selected errors to the SD
card, with a compact internal-flash fallback if SD export fails.
Start here when audio stops, a show aborts, or a skull behaves unexpectedly.
The Arduino instructions below describe the older runtime.

### Where the error logs are

- **On the board:** `/sdcard/diagnostics/fault-0.log` through `fault-7.log`.
- **On a computer:** open the card’s **`diagnostics` folder at its root**. For
  example, a card mounted as `/Volumes/SKULL` has
  `/Volumes/SKULL/diagnostics/fault-0.log`; `sdcard` is the firmware mount point,
  not another folder to create on the card.
- Reports rotate through eight slots. Copy **all eight available files** to a
  private incident folder before more tests overwrite older reports. Slot numbers
  wrap; use report IDs, uptime and firmware hashes inside the files for context.
  `pending.tmp` is staging evidence and may be incomplete; preserve it if present.
- This is selected recent history around the first error, not continuous full
  serial output. More errors coalesce while a report is pending. No report may
  mean no selected fault, old firmware, unavailable logger memory, or failed export.

### Capture evidence before changing anything

1. Record which board/role, firmware build, trigger, audio file/show phase and
   elapsed time failed, plus what was actually heard or seen. Record whether
   the speaker remained connected and what the printer/jaw actually did.
2. Attach 115200-baud serial **without resetting** and send `status`, then
   `fault-log`. Keep the entire capture. Avoid rebooting, removing the card,
   re-pairing, reflashing or running `fault-test` until the real fault is saved.
   A full pending RAM report is more detailed than the roughly 1 KiB flash fallback.
3. Read `pending`, `SD_attempts`, `SD_errno`, `NVS`, and `next_SD_ms` in status.
   Writes wait for idle show/audio/startup/tone/printer and audio-file cleanup;
   busy or cancellation holds can defer export. Failed exports retry at most once
   per 60 seconds while idle. `sd=1` alone does not prove writes work. After a
   successful export/acknowledgement, `pending=0` and `fault-log` may say there is
   no pending report: retrieve the saved SD files instead.
4. Once work has stopped and evidence is captured, power the board down before
   removing its card. Copy the `diagnostics` folder using a computer reader; also
   preserve the exact implicated WAV/script/config privately. Do not format or
   replace files as the first debugging step. Leave the original evidence intact.

From the repository root, activate the ESP-IDF environment (with `pyserial`),
choose the actual board’s serial port and a **new** capture filename, then:

```sh
python native_scripts/serial_probe.py --port /dev/cu.usbserial-REPLACE \
  --output build/validation/incident-001.log --seconds 30 \
  --send-after 1 --send status --send fault-log
```

The helper leaves reset disabled unless `--reset` is supplied. Close other
monitors first so only one process owns the port. Raw serial captures can contain
provisioning data; keep them private under ignored `build/validation/`, and redact excerpts before
checking them in. The filtered SD reports do not replace the full serial capture.

### Isolate the cause and verify one change

Use the error context to separate SD reads, speaker readiness, playback
scheduling and physical outputs. Reproduce one bounded case and change one
variable at a time. Zero underruns alone does not prove clean audible playback;
software completion does not prove jaw motion or paper output. Death’s idle
`sd-check` compares repeat reads without the speaker; use
`asset /audio/<file>.wav` to inspect an asset. Printer transport completion needs
a separate paper check.

After saving the real incident, idle-only `fault-test` can check the logging path
with a labelled synthetic error. Follow with `status` and inspect the new report;
it does not reproduce the underlying failure. Pending compact reports can survive
reboot, but a crash or power loss before persistence can lose RAM evidence. This
logger is not a panic/core-dump facility. Keep Matter fabrics, speaker settings
and original card assets intact during ordinary debugging.

Record the observation, hypothesis, single change, test result and next step in
the relevant story work log. For a nontrivial obstacle, make a bounded research
pass on the general problem before adding retries or special cases. Record useful
sources and distinguish host tests, device telemetry and human-observed behavior.
See the [native firmware reference](firmware/README.md) for command details,
build/upload procedures and logging implementation limits.

---

## Historical Arduino implementation

> **🔗 Related Project**: [Death Matter Controller](https://github.com/copperdogma/death-matter-controller) - ESP32-C3 Matter controller for Apple Home integration

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
const int EYE_LED_PIN = 32;    // Eye LED pin
const int MOUTH_LED_PIN = 33;  // Mouth LED pin
const int SERVO_PIN = 23;      // Servo control pin
const int CAP_SENSE_PIN = 4;   // Capacitive finger sensor pin
const int PRINTER_TX_PIN = 18; // Thermal printer TX pin (ESP32 -> printer RXD)
const int PRINTER_RX_PIN = 19; // Thermal printer RX pin (printer TXD -> ESP32)
const int MATTER_TX_PIN = 21;  // Matter UART TX pin (ESP32 -> ESP32-C3 RX)
const int MATTER_RX_PIN = 22;  // Matter UART RX pin (ESP32 <- ESP32-C3 TX)
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
- Helper scripts: `python scripts/flash_and_monitor.py --mode usb --seconds 30` (USB flash + 30s serial capture) or `--mode ota --capture telnet` for OTA. See [`docs/ota.md`](docs/ota.md) for details.
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

## Modular Runtime Options

`AppController` wires the core runtime modules (CLI, content selection, printer, connectivity) using lightweight seams. You can enable or disable modules per prop by defining the following build flags (e.g. in `platformio.local.ini` or via `build_flags`):

- `APP_ENABLE_CLI` (default `1`) – disable to drop the serial CLI.
- `APP_ENABLE_CONTENT_SELECTION` (default `1`) – disable audio directory + skit helpers.
- `APP_ENABLE_PRINTER` (default `1`) – disable thermal printer support.
- `APP_ENABLE_CONNECTIVITY` (default `1`) – master switch for Wi-Fi/OTA/RemoteDebug.
- `APP_ENABLE_WIFI`, `APP_ENABLE_OTA`, `APP_ENABLE_REMOTE_DEBUG` – override individual connectivity modules (default follow `APP_ENABLE_CONNECTIVITY`).
- `APP_ENABLE_BLUETOOTH` (default `1`) – disable Bluetooth A2DP wiring.

Each module can also be replaced at runtime by supplying custom instances through `AppController::ModuleProviders` before calling `setup()`. This lets you plug in prop-specific implementations without touching the default Arduino-friendly wiring.

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
