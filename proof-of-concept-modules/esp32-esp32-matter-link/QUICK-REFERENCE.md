# Quick Reference - Matter Master/Slave Patterns

## 🚀 Copy-Paste Patterns for Future Projects

### 1. UART Frame Protocol

```cpp
// Frame format: 0xA5 LEN CMD PAYLOAD... CRC8
#define FRAME_START 0xA5
#define CRC_POLY 0x31  // Dallas/Maxim

uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ CRC_POLY : (crc << 1);
        }
    }
    return crc;
}

void sendFrame(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen) {
    uint8_t frame[64];
    uint8_t len = 3 + payloadLen;  // START + LEN + CMD + PAYLOAD + CRC
    frame[0] = FRAME_START;
    frame[1] = len;
    frame[2] = cmd;
    if (payloadLen > 0) memcpy(&frame[3], payload, payloadLen);
    frame[len - 1] = crc8(&frame[1], len - 2);
    Serial1.write(frame, len);
}
```

### 2. Matter Mutual Exclusivity Pattern

```cpp
// Globals
static volatile int g_target_mode = -1;
static volatile int64_t g_last_tap_time = 0;
static volatile bool g_syncing_modes = false;

// In callback - just record the tap
if (val->val.b == true && !g_syncing_modes) {
    g_target_mode = mode;
    g_last_tap_time = esp_timer_get_time();
}

// In sync task - execute after 200ms
int64_t time_since_tap_ms = (esp_timer_get_time() - g_last_tap_time) / 1000;
if (g_target_mode >= 0 && time_since_tap_ms >= 200) {
    g_syncing_modes = true;
    
    // Turn OFF all except target
    for (int i = 0; i < NUM_MODES; i++) {
        if (i != g_target_mode) {
            attribute::report(mode_ids[i], OnOff::Id, OnOff::Attributes::OnOff::Id, &off_val);
        }
    }
    
    // Turn ON target
    attribute::report(mode_ids[g_target_mode], OnOff::Id, OnOff::Attributes::OnOff::Id, &on_val);
    
    g_syncing_modes = false;
    g_target_mode = -1;
}
```

### 3. UART Response Timing (Critical!)

```cpp
// ❌ WRONG - ACK arrives after slow operation
void handle_command() {
    led_blink(10, 50);  // 500ms delay
    uart_send_response(RSP_ACK);  // Too late!
}

// ✅ CORRECT - ACK first, then slow operations
void handle_command() {
    uart_send_response(RSP_ACK);  // Immediate!
    led_blink(10, 50);  // Now we can take our time
}
```

### 4. Creating Matter Endpoints

```cpp
// Create node with custom name
node::config_t node_config{};
strncpy(node_config.root_node.basic_information.node_label, "H-Death", 
        sizeof(node_config.root_node.basic_information.node_label) - 1);
node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

// Create plugin unit endpoint
endpoint::on_off_plugin_unit::config_t cfg;
endpoint_t *ep = endpoint::on_off_plugin_unit::create(node, &cfg, ENDPOINT_FLAG_NONE, NULL);
uint16_t endpoint_id = endpoint::get_id(ep);
```

### 5. Commissioning Status Notifications

```cpp
static esp_err_t app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            // Device paired with HomeKit
            uart_send_frame(CMD_STATUS_PAIRED, nullptr, 0);
            break;
        case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
            // Device unpaired
            uart_send_frame(CMD_STATUS_UNPAIRED, nullptr, 0);
            break;
    }
    return ESP_OK;
}
```

### 6. Factory Reset Implementation

```cpp
// Console command
static int factory_reset_cmd(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "confirm") == 0) {
        xTaskCreate([](void*){ 
            vTaskDelay(pdMS_TO_TICKS(10000));  // 10s countdown
            esp_matter::factory_reset(); 
            vTaskDelete(NULL); 
        }, "factory_reset", 4096, NULL, 5, NULL);
        return 0;
    }
    printf("Usage: factory_reset confirm\n");
    return 1;
}

// Button (GPIO 9, hold 5+ seconds)
button_config_t button_config = {
    .long_press_time = 5000,
};
button_gpio_config_t gpio_config = {
    .gpio_num = 9,
    .active_level = 0,
};
```

---

## 🎯 Critical Gotchas

### 1. attribute::update() vs attribute::report()
- **`update()`**: Skips if value unchanged, triggers callbacks
- **`report()`**: Forces update, notifies HomeKit, NO callbacks
- **Use `report()` when**: Enforcing mutual exclusivity, fixing stuck states

### 2. Callback Recursion Prevention
Always use a flag when updating attributes from a callback:
```cpp
static volatile bool g_syncing = false;

// In callback
if (g_syncing) return;  // Ignore our own updates

// When updating
g_syncing = true;
attribute::report(...);
g_syncing = false;
```

### 3. Don't Turn OFF the Target Mode
```cpp
// ❌ WRONG - creates flicker
for (int i = 0; i < 4; i++) {
    attribute::report(mode[i], ...OFF...);  // Includes target!
}
attribute::report(target, ...ON...);

// ✅ CORRECT - target never flickers
for (int i = 0; i < 4; i++) {
    if (i != target) {  // Skip target!
        attribute::report(mode[i], ...OFF...);
    }
}
attribute::report(target, ...ON...);
```

### 4. Back-Feeding Prevention
**Never power only one board** when they're connected via GPIO. Always power both independently via USB.

### 5. HomeKit UI Caching
**Accept it** - rapid changes may require force-closing Home app. This affects ALL Matter devices.

---

## 📋 Command Reference

### S3 CLI Commands
```
hello              Test UART handshake
ping               Test connectivity
trigger            Send trigger to C3
mode <0-3>         Set mode (0=Little Kid, 1=Big Kid, 2=Take One, 3=Closed)
status             Show system status
help               Show commands
```

### C3 Console Commands
```
factory_reset confirm    Erase all pairing data (10s countdown)
```

---

## 🔧 Build Commands

### ESP32-S3 (Arduino)
```
Open Arduino IDE → Upload
Serial Monitor: 115200 baud
```

### ESP32-C3 (ESP-IDF)
```bash
cd esp32-matter-node/firmware
source ~/esp/esp-idf/export.sh
export ESP_MATTER_PATH=~/esp/esp-matter
source ~/esp/esp-matter/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash
```

---

## 🎨 HomeKit Setup

1. Scan QR: `MT:Y.K90GSY00KA0648G00`
2. Device appears as "Power Strip"
3. Rename device to "H-Death" (or your choice)
4. Rename 5 outlets:
   - Outlet → Trigger
   - Outlet 2 → Little Kid
   - Outlet 3 → Big Kid
   - Outlet 4 → Take One
   - Outlet 5 → Closed

---

## 🐛 Troubleshooting

**Port busy when flashing:**
```bash
# Close Arduino IDE Serial Monitor
# Or kill processes:
lsof | grep usbmodem
```

**HomeKit shows multiple modes ON:**
```
1. Wait 5 seconds for safety cleanup
2. Force-close Home app
3. Reopen Home app
```

**Factory reset not working:**
```
Hold BOOT button (GPIO 9) for full 5+ seconds
Or use console: factory_reset confirm
```

**UART timeout on "hello" command:**
```
Check that C3 sends ACK before LED blinks
Verify both boards are powered
```

---

## 📊 Performance

- **UART latency**: 10-50ms
- **HomeKit → UART**: 50-100ms  
- **Mode change**: 200ms (debounce) + 150ms (execution)
- **Safety cleanup**: 5s after last change
- **HomeKit UI update**: 1-10s (uncontrollable)

---

## 🎓 For Future AI Agents

When building the production project:

1. **Read POC-SUMMARY.md first** - Contains all patterns and gotchas
2. **Copy UART protocol exactly** - It's battle-tested
3. **Use the mutual exclusivity pattern** - Don't reinvent it
4. **Document HomeKit limitations** - Users need to know
5. **Test with rapid input** - HomeKit caching is real
6. **Use `report()` for forced updates** - Critical for mutual exclusivity
7. **Never turn OFF the target mode** - Prevents flicker
8. **Send ACK before slow operations** - Prevents timeouts

**This POC is your blueprint.** The patterns work. The code is solid. HomeKit is just HomeKit. 🎃

---

*POC Completed: October 13, 2025*

