# Migration from Arduino IDE to PlatformIO/Cursor

This document outlines the challenges encountered during an attempt to migrate the "TwoSkulls" project from the Arduino IDE to a PlatformIO-based environment like Cursor. The goal of the initial migration was to create a minimal, compilable build by disabling Bluetooth functionality. This effort was unsuccessful, and the changes were reverted.

## Core Challenge: Build System Abstraction

The fundamental difficulty lies in the difference between how the Arduino IDE and PlatformIO handle the underlying ESP-IDF (Espressif IoT Development Framework) configuration.

-   **Arduino IDE:** Abstracts away most of the ESP-IDF's complexity. Critical configuration files like `sdkconfig.h` are automatically generated and managed behind the scenes based on the board and options selected in the IDE menus. This makes it easy to get started but hides the underlying build process.

-   **PlatformIO:** Exposes the full ESP-IDF build system. This provides much more power and flexibility but requires the developer to explicitly manage the configuration. Migrating a project that relies on deep hardware features (like Bluetooth) means this hidden configuration must be recreated manually.

## Specific Issues Encountered

The migration process was an iterative cycle of fixing one build error only to uncover another.

### 1. The `sdkconfig.h` Rabbit Hole

*   **Initial Error:** The first compilation failed because multiple core ESP32 headers and libraries could not find `sdkconfig.h`.
*   **Problem:** This file is essential for configuring low-level hardware features of the ESP32. In PlatformIO, it is not automatically created or included.
*   **Attempted Fix:** A blank `sdkconfig.h` was created in an `include/` directory, and the `platformio.ini` was updated with `build_flags = -I include` to add it to the compiler's search path.
*   **Result:** This led to a cascade of new errors, as the compiler now found the file but expected dozens of macros to be defined within it.

### 2. Cascading Configuration Errors

With the `sdkconfig.h` file now in the build path, the compiler began requesting specific macros that were previously handled by the Arduino IDE's build system. This became a frustrating game of "whack-a-mole," adding defines one by one:

-   `CONFIG_SPIRAM_SIZE`
-   `CONFIG_FREERTOS_MAX_TASK_NAME_LEN`
-   `CONFIG_WL_SECTOR_SIZE` (for the SD card wear-leveling library)
-   Numerous low-level Bluetooth controller settings (`CONFIG_BT_ENABLED`, `CONFIG_BTDM_CTRL_...`, etc.), even though the stated goal was to *disable* Bluetooth.

### 3. Code & Library Incompatibilities

PlatformIO's stricter compiler and different build environment revealed several code-level issues:

*   **`Serial` Not Declared:** Many `.cpp` files used the `Serial` object without including `<Arduino.h>` or providing a forward declaration (`extern HardwareSerial Serial;`). The Arduino IDE is more lenient with this.
*   **Conditional Compilation Scoping:** To disable the servo, its usage was wrapped in `#ifdef DISABLE_SERVO` blocks. However, the member variable declarations in `servo_controller.h` were also moved inside these blocks, causing compile errors when the code was disabled, as other parts of the class still expected those members to exist.
*   **Deep Bluetooth Dependencies:** The attempt to create a minimal build by stubbing out the project's `BluetoothController` was insufficient. The `ESP32-A2DP` library has deep dependencies on the ESP-IDF's Bluetooth stack. The compiler continued to fail because it could not find core types like `BluetoothA2DPSource` and `BLERemoteCharacteristic`, which are defined in headers that are themselves dependent on a correctly configured `sdkconfig.h`.

## Recommendations for Future Attempts

1.  **Do Not Attempt a Minimal Build:** Trying to strip out features like Bluetooth is likely more complex than configuring them correctly in the new environment. The dependencies are too intertwined.
2.  **Generate `sdkconfig.h`:** The most promising path is to generate a valid `sdkconfig.h` from a working Arduino IDE build. This file can then be copied into the PlatformIO project. It is often located in a temporary build folder.
3.  **Start Fresh & Add Incrementally:** An alternative is to create a new, blank PlatformIO project for the ESP32-WROVER.
    -   First, configure `platformio.ini` with all the required libraries from the `README.md`.
    -   Then, copy the source (`.cpp`, `.h`, `.ino`) files into the `src/` directory.
    -   Compile and fix the errors one by one. This is a more systematic approach than trying to patch a broken build.
4.  **Rename `.ino` to `.cpp`:** The main `TwoSkulls.ino` file should be renamed to `main.cpp`. The `.ino` file type has special preprocessing behavior in the Arduino IDE that can cause issues in PlatformIO. After renaming, you will need to manually add `#include <Arduino.h>` at the top and add function prototypes for `setup()` and `loop()` if they are defined after being called.

**Conclusion:** Migrating this project is feasible, but requires a significant, one-time effort to recreate the ESP-IDF configuration. The path of least resistance for continued development is to remain with the Arduino IDE. If the benefits of PlatformIO (better editor, dependency management, debugging) are desired, a systematic approach as outlined above is recommended. 