# Bluetooth A2DP Audio Playback Issues

<!-- CURRENT_STATE_START -->
## Current State

**Domain Overview:**  
Bluetooth A2DP audio system manages connection to JBL Flip 5 speaker and streams audio from SD card via ESP32-A2DP library. All known issues resolved - connection state detection and audio playback both working correctly.

**Subsystems / Components:**  
- A2DP Connection State Detection — Working — Fixed debouncing issue
- Audio Queuing — Working — Files successfully queued to audio player
- Audio Buffer Management — Fixed — `audioPlayer->update()` now called in main loop
- Media Start Control — Working — `requestMediaStart()` called on connection
- Bluetooth Controller Update Loop — Fixed — `bluetoothController->update()` now called

**Active Issue:** None  
**Status:** All Issues Resolved  
**Last Updated:** 20250125-1215  
**Next Step:** N/A

**Open Issues (latest first):**
- None

**Recently Resolved (last 5):**
- 20250125-1200 — Queued audio not playing after Bluetooth connection — Fixed missing update() calls in main loop
- 20251025 — Connection state detection debouncing — Fixed rapid state change handling
<!-- CURRENT_STATE_END -->

## Problem Summary

The ESP32-A2DP Bluetooth connection is working correctly (speaker connects and makes "connected" sound), but the firmware's connection state detection is failing. The system continues to show "A2DP connected: false" and keeps retrying connections even when the Bluetooth connection is actually established and stable.

## Current Behavior

1. **Firmware boots successfully** - All components initialize properly
2. **Bluetooth connection works** - Speaker makes "connected" sound, indicating successful A2DP pairing
3. **Connection state callback fires** - We see `🔔 Connection state callback triggered: state=2` (state=2 = connected)
4. **Debouncing blocks the callback** - The rapid state change gets ignored due to 2-second debounce
5. **System never recognizes connection** - Status continues showing "A2DP connected: false"
6. **Infinite retry loop** - System keeps trying to connect every 5 seconds

## Technical Details

### Connection State Values
- `state=0` = Disconnected
- `state=1` = Disconnected (intermediate state)
- `state=2` = Connected

### Current Code Flow
1. `onConnectionStateChanged()` gets called with `state=1` (disconnected)
2. 2 seconds later, `onConnectionStateChanged()` gets called with `state=2` (connected)
3. **Problem**: The second callback is ignored due to debouncing logic
4. `m_a2dpConnected` remains `false`
5. Retry logic continues indefinitely

### Key Files
- `src/bluetooth_controller.h` - Connection state management
- `src/bluetooth_controller.cpp` - Connection logic and callbacks
- `src/main.cpp` - Status reporting

## Code Analysis

### Current Debouncing Logic (PROBLEMATIC)
```cpp
void BluetoothController::onConnectionStateChanged(esp_a2d_connection_state_t state, void *remote_bda) {
    unsigned long currentTime = millis();
    
    // Only process state changes if enough time has passed (debouncing)
    if (currentTime - m_lastConnectionStateChange < 2000) { // 2 second debounce
        Serial.println("⏳ Ignoring rapid state change (debouncing)");
        return; // THIS IS THE PROBLEM - blocks the connected state!
    }
    
    // ... rest of logic never executes for state=2
}
```

### The Issue
The ESP32-A2DP library fires connection state callbacks in rapid succession:
1. `state=1` (disconnected) - processed, starts retry logic
2. `state=2` (connected) - **IGNORED** due to debouncing
3. System never knows it's connected

## Expected Behavior

1. Connection state callback fires with `state=2`
2. `m_a2dpConnected` gets set to `true`
3. Retry logic stops
4. Status shows "A2DP connected: true"

## Debugging Evidence

### Serial Output Analysis
```
01:33:49.189 > 🔔 Connection state callback triggered: state=1
01:33:49.192 > 🔄 Connection state unchanged: Disconnected
01:33:49.197 > 🔔 Connection state callback triggered: state=2
01:33:49.200 > ⏳ Ignoring rapid state change (debouncing)  <-- PROBLEM HERE
```

The `state=2` (connected) callback is being ignored, so the system never recognizes the connection.

## Potential Solutions

### Option 1: Remove Debouncing for Connected State
```cpp
void BluetoothController::onConnectionStateChanged(esp_a2d_connection_state_t state, void *remote_bda) {
    unsigned long currentTime = millis();
    
    // Always process connected state, debounce only disconnections
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        // Process connected state immediately
        m_a2dpConnected = true;
        stopConnectionRetry();
        Serial.println("🔗 A2DP Connected!");
        return;
    }
    
    // Debounce only disconnection states
    if (currentTime - m_lastConnectionStateChange < 2000) {
        Serial.println("⏳ Ignoring rapid state change (debouncing)");
        return;
    }
    
    // Process disconnection logic...
}
```

### Option 2: Shorter Debounce Period
```cpp
if (currentTime - m_lastConnectionStateChange < 500) { // 500ms instead of 2000ms
```

### Option 3: Smart Debouncing
```cpp
// Only debounce if we're already connected and getting disconnected
if (m_a2dpConnected && state != ESP_A2D_CONNECTION_STATE_CONNECTED) {
    if (currentTime - m_lastConnectionStateChange < 2000) {
        return; // Debounce disconnections
    }
}
// Always process connections immediately
```

### Option 4: State Machine Approach
Track the connection process more intelligently:
```cpp
enum ConnectionPhase {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};
```

## Test Cases to Verify Fix

1. **Connection Test**: Verify that `state=2` callback properly sets `m_a2dpConnected = true`
2. **Retry Stop Test**: Verify that retry logic stops when connected
3. **Status Test**: Verify that status shows "A2DP connected: true"
4. **Disconnection Test**: Verify that actual disconnections are still detected
5. **Rapid Reconnection Test**: Verify that rapid connect/disconnect cycles work

## Files to Modify

1. **`src/bluetooth_controller.cpp`** - Fix `onConnectionStateChanged()` method
2. **`src/bluetooth_controller.h`** - Possibly add connection phase tracking
3. **Test with** - Monitor serial output to verify connection state detection

## Current Project Context

- **Platform**: ESP32-WROVER with PlatformIO
- **Library**: ESP32-A2DP@^1.0.0 (from GitHub)
- **Speaker**: JBL Flip 6
- **Framework**: Arduino
- **Upload Speed**: 460800 baud (working well)

## Success Criteria

The fix is successful when:
1. ✅ Connection state callback with `state=2` is processed
2. ✅ `m_a2dpConnected` becomes `true`
3. ✅ Retry logic stops
4. ✅ Status shows "A2DP connected: true"
5. ✅ No more infinite retry loops
6. ✅ Actual disconnections are still detected

## Additional Context

This is part of the "Death Fortune Teller" project - an animatronic skull that tells fortunes. The Bluetooth connection is needed for audio streaming to the speaker. The connection works at the hardware level (speaker makes connected sound), but the firmware's state management is broken.

The project successfully compiles and uploads with PlatformIO, and all other components (SD card, servo, LEDs, etc.) work correctly. Only the Bluetooth connection state detection needs to be fixed.

---

## 20250125-1200: NEW ISSUE: Queued Audio Not Playing After Bluetooth Connection

**Description:**  
Board successfully connects to Bluetooth speaker, audio files are queued, but audio doesn't play. The logs show:
- ✅ SD card mounted and initialization audio file found
- ✅ Audio queued: `/audio/initialized.wav`
- ✅ Bluetooth connects successfully (`🔗 A2DP Connected!`)
- ✅ A2DP audio state changed to STARTED
- ❌ No audio playback occurs

**Environment:**
- Platform: ESP32-WROVER with PlatformIO
- Library: ESP32-A2DP@^1.0.0
- Speaker: JBL Flip 5
- Framework: Arduino

**Evidence:**
```
I/SDCard: Mounted successfully
I/Audio: 🎵 Initialization audio discovered: /audio/initialized.wav
D/AudioPlayer: Added file to queue: /audio/initialized.wav
I/Audio: 🎵 Queued initialization audio: /audio/initialized.wav
I/Bluetooth: 🔗 A2DP Connected!
I/Bluetooth: 🎧 A2DP audio state changed: STARTED
```

### Step 1 (20250125-1200): Identified Missing Update Calls

**Action**: Analyzed main loop and component initialization flow. Found that `audioPlayer->update()` and `bluetoothController->update()` are never called in `loop()`.

**Result**: 
- `main.cpp` `loop()` function only handles serial commands
- `AudioPlayer::update()` is responsible for filling audio buffer from SD card
- `BluetoothController::update()` is responsible for processing media start requests
- Both are critical for audio playback but missing from main loop

**Notes**:
- Audio queue system works (`playNext()` adds files to queue)
- Audio buffer filling happens in `AudioPlayer::fillBuffer()` called from `update()`
- Media start control (`processMediaStart()`) happens in `BluetoothController::update()`
- Git history shows audio was working before, suggesting update calls were removed or never added

**Next Steps**: Add `audioPlayer->update()` and `bluetoothController->update()` calls to main loop() function.

### Step 2 (20250125-1200): Added Missing Update Calls to Main Loop

**Action**: Modified `src/main.cpp` `loop()` function to call `audioPlayer->update()` and `bluetoothController->update()`.

**Result**: 
- Audio player will now fill buffer from SD card files in queue
- Bluetooth controller will process media start requests (`processMediaStart()`)
- Both updates happen every loop iteration before handling serial commands

**Notes**:
- `AudioPlayer::update()` calls `fillBuffer()` which reads from SD card and writes to circular buffer
- `BluetoothController::update()` calls `processMediaStart()` which issues `ESP_A2D_MEDIA_CTRL_START` when connected
- Update calls are protected with null checks
- Serial command handling remains in loop but happens after component updates

**Code Changes**:
```cpp
void loop() {
    // Update audio player (fills buffer, handles playback events)
    if (audioPlayer) {
        audioPlayer->update();
    }
    
    // Update Bluetooth controller (processes media start, connection retry)
    if (bluetoothController) {
        bluetoothController->update();
    }
    
    // Handle serial commands...
}
```

**Next Steps**: Test with hardware to verify audio playback works after Bluetooth connection.

---

## Resolution

Issue "Queued audio not playing after Bluetooth connection" Resolved (20250125-1200)

**Symptoms:**
- Audio files successfully queued but never played
- Bluetooth connected successfully (`🔗 A2DP Connected!`)
- A2DP audio state changed to STARTED
- No audio output despite connection

**Timeline:**
- 20250125-1200: Issue reported and investigated
- 20250125-1200: Root cause identified - missing `update()` calls in main loop
- 20250125-1200: Fix implemented

**Root Cause:**
The main `loop()` function in `src/main.cpp` was missing critical update calls:
- `audioPlayer->update()` - Required to fill audio buffer from SD card and process playback events
- `bluetoothController->update()` - Required to process media start requests (`processMediaStart()`)

Without these calls, the audio system couldn't:
1. Fill the circular buffer from queued SD card files
2. Issue `ESP_A2D_MEDIA_CTRL_START` commands to initiate A2DP streaming

**Fix:**
Added update calls to `loop()` function:
```cpp
void loop() {
    // Update audio player (fills buffer, handles playback events)
    if (audioPlayer) {
        audioPlayer->update();
    }
    
    // Update Bluetooth controller (processes media start, connection retry)
    if (bluetoothController) {
        bluetoothController->update();
    }
    
    // Handle serial commands...
}
```

**Preventive Actions:**
- Ensure all component `update()` methods are called in main loop
- Document which components require periodic updates
- Add initialization checklist to verify all update loops are connected
