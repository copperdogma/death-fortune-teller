/*
 * ESP32-C3 Matter Node - Master/Slave POC
 * 
 * This is a WORKING PROOF OF CONCEPT for Matter-enabled master/slave architecture.
 * 
 * Key Patterns Implemented:
 * 1. Bidirectional UART with CRC8 for inter-board communication
 * 2. Multiple Matter endpoints (5 on/off plugin units)
 * 3. Debounced mutual exclusivity for mode selection (200ms + 5s cleanup)
 * 4. HomeKit workarounds for UI caching and state synchronization
 * 
 * Critical Learnings:
 * - Use attribute::report() not update() for forced updates
 * - Never turn OFF the target mode (prevents flicker)
 * - Send UART ACK before slow operations (prevents timeouts)
 * - Debounce rapid input (HomeKit sends commands very fast)
 * - Accept HomeKit UI caching (force-close/reopen fixes it)
 * 
 * See POC-SUMMARY.md for complete documentation.
 * 
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 */

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_ota.h>
#include <nvs_flash.h>

// Include the sdkconfig.h file to access Kconfig values
#include "sdkconfig.h"

#include <app_openthread_config.h>
#include "app_reset.h"
#include "utils/common_macros.h"

// Button component direct include (for factory reset only)
#include "iot_button.h"
#include "button_gpio.h"

// For VID/PID and Onboarding Codes (official example method)
#include <app/server/OnboardingCodesUtil.h>

// Attempting to include ESP32 specific config to resolve GetVendorId issue
#include <platform/ConfigurationManager.h> // Base interface
#include <platform/ESP32/ESP32Config.h>    // ESP32 specific implementations

// drivers implemented by this example


#include <esp_matter_event.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "app_main";

// Global variables
static uint16_t g_switch_endpoint_id = 0;
static uint16_t g_mode_plugin_ids[4] = {0}; // 4 plugin units for mode selection
static bool g_pulse_active = false;    // Track if pulse is currently active
static volatile int g_target_mode = -1;         // User's desired mode (-1 = none pending)
static volatile int64_t g_last_tap_time = 0;    // Time of last user tap (microseconds)
static volatile int64_t g_last_execution_time = 0; // Time of last mode execution (microseconds)
static volatile bool g_syncing_modes = false;   // Flag to prevent callback recursion during sync

// Use the Kconfig value directly

using namespace esp_matter;
using namespace esp_matter::attribute;

// Helper function to set custom endpoint name (simplified for now)
static void set_endpoint_name(endpoint_t *endpoint, const char *name) {
    // TODO: Implement custom naming when ESP-Matter API is better understood
    ESP_LOGI(TAG, "Would set endpoint name to: %s", name);
}
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

// Define GPIO pins
#define BUTTON_GPIO CONFIG_BSP_BUTTON_GPIO  // GPIO 9 on ESP32-C3 SuperMini
#define BSP_BUTTON_NUM 0
#define SIGNAL_GPIO (gpio_num_t)4            // GPIO 4 for signal output
#define PULSE_DURATION_MS 500               // 500ms pulse duration

// UART configuration for S3 communication
#define UART_NUM UART_NUM_1
#define UART_TX_PIN 21
#define UART_RX_PIN 20
#define UART_BAUD 115200
#define UART_BUF_SIZE 1024

// LED for visual feedback
#define LED_GPIO (gpio_num_t)8               // Built-in LED (inverted: LOW=ON)

// UART Protocol
#define FRAME_START 0xA5
#define CRC_POLY 0x31

// Commands from S3
#define CMD_HELLO    0x01
#define CMD_SET_MODE 0x02
#define CMD_TRIGGER  0x03
#define CMD_PING     0x04

// Commands from C3 (status notifications)
#define CMD_STATUS_PAIRED    0x10
#define CMD_STATUS_UNPAIRED  0x11

// Responses to S3
#define RSP_ACK      0x80
#define RSP_ERR      0x81
#define RSP_BUSY     0x82
#define RSP_DONE     0x83

// Global UART state
static uint8_t g_current_mode = 0;  // 0=Little Kid, 1=Big Kid, 2=Take One, 3=Closed

// ===== LED Control Functions =====
static void led_on() {
    gpio_set_level(LED_GPIO, 0);  // Inverted: LOW = ON
}

static void led_off() {
    gpio_set_level(LED_GPIO, 1);  // Inverted: HIGH = OFF
}

static void led_blink(int count, int on_ms, int off_ms) {
    for (int i = 0; i < count; i++) {
        led_on();
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        led_off();
        if (i < count - 1) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }
}

// LED patterns for different events
static void led_ack() {
    led_blink(2, 100, 100);  // 2 quick blinks
}

static void led_command_sent() {
    led_blink(1, 500, 0);  // 1 long blink
}

static void led_error() {
    led_blink(5, 50, 50);  // 5 rapid blinks
}

static void led_hello() {
    led_blink(3, 300, 300);  // 3 slow blinks
}

// ===== CRC8 Calculation =====
static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC_POLY;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ===== UART Helper Functions =====
static bool uart_send_frame(uint8_t cmd, const uint8_t *payload = nullptr, uint8_t payload_len = 0) {
    uint8_t frame[64];
    uint8_t idx = 0;
    
    // Header
    frame[idx++] = FRAME_START;
    
    // Length = CMD (1 byte) + PAYLOAD
    uint8_t len = 1 + payload_len;
    frame[idx++] = len;
    
    // Command/Response
    frame[idx++] = cmd;
    
    // Payload
    if (payload && payload_len > 0) {
        memcpy(&frame[idx], payload, payload_len);
        idx += payload_len;
    }
    
    // CRC (over LEN + CMD + PAYLOAD)
    uint8_t crc = crc8(&frame[1], idx - 1);
    frame[idx++] = crc;
    
    // Send
    int written = uart_write_bytes(UART_NUM, frame, idx);
    
    ESP_LOGI(TAG, "UART TX: %d bytes, CMD=0x%02X", idx, cmd);
    
    return (written == idx);
}

// Wrapper for responses
static bool uart_send_response(uint8_t response_cmd, const uint8_t *payload = nullptr, uint8_t payload_len = 0) {
    return uart_send_frame(response_cmd, payload, payload_len);
}

// ===== Command Handlers =====
static void handle_cmd_hello(const uint8_t *payload, uint8_t len) {
    ESP_LOGI(TAG, "CMD: HELLO");
    uart_send_response(RSP_ACK);  // Send ACK FIRST
    led_hello();  // Then do LED pattern
}

static void handle_cmd_ping(const uint8_t *payload, uint8_t len) {
    ESP_LOGI(TAG, "CMD: PING");
    uart_send_response(RSP_ACK);  // Send ACK FIRST
    led_ack();  // Then do LED pattern
}

static void handle_cmd_trigger(const uint8_t *payload, uint8_t len) {
    ESP_LOGI(TAG, "CMD: TRIGGER");
    
    if (g_pulse_active) {
        // Already running
        ESP_LOGW(TAG, "Skit already active - sending BUSY");
        uart_send_response(RSP_BUSY);  // Send response FIRST
    } else {
        uart_send_response(RSP_ACK);  // Send response FIRST
        led_command_sent();  // Then LED
        // TODO: In future, actually trigger skit here
        ESP_LOGI(TAG, "Trigger acknowledged (placeholder)");
    }
}

static void handle_cmd_set_mode(const uint8_t *payload, uint8_t len) {
    if (len < 1) {
        ESP_LOGE(TAG, "SET_MODE: missing payload");
        uart_send_response(RSP_ERR);  // Send response FIRST
        led_error();  // Then LED
        return;
    }
    
    uint8_t mode = payload[0];
    if (mode > 3) {
        ESP_LOGE(TAG, "SET_MODE: invalid mode %d", mode);
        uart_send_response(RSP_ERR);  // Send response FIRST
        led_error();  // Then LED
        return;
    }
    
    g_current_mode = mode;
    ESP_LOGI(TAG, "CMD: SET_MODE -> %d", mode);
    
    uart_send_response(RSP_ACK);  // Send response FIRST
    
    // Blink LED to show mode (1-4 blinks for modes 0-3)
    led_blink(mode + 1, 200, 200);  // LED after response
}

// ===== Mode Synchronization Task =====
// Debounced mode switching: waits 200ms after last tap, then executes once
// Plus a 5s safety cleanup to ensure HomeKit converges to correct state
static void mode_sync_task(void *arg)
{
    const char* mode_names[] = {"Little Kid", "Big Kid", "Take One", "Closed"};
    
    while (1) {
        int64_t now = esp_timer_get_time();
        int64_t time_since_tap_ms = (now - g_last_tap_time) / 1000;
        int64_t time_since_exec_ms = (now - g_last_execution_time) / 1000;
        
        // PRIMARY EXECUTION: 200ms after last tap
        if (g_target_mode >= 0 && g_target_mode != g_current_mode && time_since_tap_ms >= 200) {
            ESP_LOGI(TAG, "🎯 Debounce complete! Executing mode change to %d (%s)", 
                     g_target_mode, mode_names[g_target_mode]);
            
            // Update current mode
            g_current_mode = g_target_mode;
            g_target_mode = -1; // Clear pending
            
            // Send UART command to S3
            uint8_t payload[1] = { (uint8_t)g_current_mode };
            uart_send_frame(CMD_SET_MODE, payload, 1);
            
            // Update HomeKit state - use report() to FORCE updates even if values match
            g_syncing_modes = true;
            
            ESP_LOGI(TAG, "📤 Setting mode %d ON, all others OFF...", g_current_mode);
            
            // Turn OFF all modes EXCEPT the target mode
            esp_matter_attr_val_t off_val = esp_matter_bool(false);
            for (int i = 0; i < 4; i++) {
                if (i != g_current_mode) {  // Skip the target mode!
                    esp_err_t err = attribute::report(g_mode_plugin_ids[i], chip::app::Clusters::OnOff::Id,
                                                       chip::app::Clusters::OnOff::Attributes::OnOff::Id, &off_val);
                    ESP_LOGI(TAG, "  Mode %d → OFF (result: %s)", i, esp_err_to_name(err));
                    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between each
                }
            }
            
            // Small delay before turning ON the target
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Turn ON the target mode
            esp_matter_attr_val_t on_val = esp_matter_bool(true);
            esp_err_t err = attribute::report(g_mode_plugin_ids[g_current_mode], chip::app::Clusters::OnOff::Id,
                                               chip::app::Clusters::OnOff::Attributes::OnOff::Id, &on_val);
            ESP_LOGI(TAG, "  Mode %d → ON (result: %s)", g_current_mode, esp_err_to_name(err));
            
            g_syncing_modes = false;
            g_last_execution_time = now;
            
            ESP_LOGI(TAG, "✅ Mode change complete: %s is now active", mode_names[g_current_mode]);
        }
        
        // SAFETY CLEANUP: ONCE at 5s after last execution, re-assert current mode
        // This ensures HomeKit converges to correct state even if it got confused
        static bool cleanup_done = false;
        if (time_since_exec_ms >= 5000 && time_since_tap_ms >= 5000 && !cleanup_done) {
            ESP_LOGI(TAG, "🔧 Safety cleanup: Re-asserting mode %d (%s)", 
                     g_current_mode, mode_names[g_current_mode]);
            
            g_syncing_modes = true;
            
            ESP_LOGI(TAG, "🧹 Cleanup: Setting mode %d ON, all others OFF...", g_current_mode);
            
            // Turn OFF all modes EXCEPT the target mode
            esp_matter_attr_val_t off_val = esp_matter_bool(false);
            for (int i = 0; i < 4; i++) {
                if (i != g_current_mode) {  // Skip the target mode!
                    esp_err_t err = attribute::report(g_mode_plugin_ids[i], chip::app::Clusters::OnOff::Id,
                                                       chip::app::Clusters::OnOff::Attributes::OnOff::Id, &off_val);
                    ESP_LOGI(TAG, "  Cleanup mode %d → OFF (result: %s)", i, esp_err_to_name(err));
                    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between each
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Turn ON the target mode
            esp_matter_attr_val_t on_val = esp_matter_bool(true);
            esp_err_t err = attribute::report(g_mode_plugin_ids[g_current_mode], chip::app::Clusters::OnOff::Id,
                                               chip::app::Clusters::OnOff::Attributes::OnOff::Id, &on_val);
            ESP_LOGI(TAG, "  Cleanup mode %d → ON (result: %s)", g_current_mode, esp_err_to_name(err));
            
            g_syncing_modes = false;
            cleanup_done = true; // Mark as done so we only do this ONCE per mode change
            
            ESP_LOGI(TAG, "✅ Safety cleanup complete (will not run again until next mode change)");
        }
        
        // Reset cleanup_done flag when user taps a new mode
        if (g_target_mode >= 0 && cleanup_done) {
            cleanup_done = false;
            ESP_LOGD(TAG, "New mode tap detected - cleanup flag reset");
        }
        
        // Run this task every 10ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ===== UART RX Task =====
static void uart_rx_task(void *arg) {
    uint8_t *data = (uint8_t *)malloc(UART_BUF_SIZE);
    uint8_t frame_buf[64];
    uint8_t buf_idx = 0;
    uint8_t state = 0;  // 0=wait start, 1=len, 2=cmd+payload, 3=crc
    uint8_t frame_len = 0;
    uint8_t bytes_to_read = 0;
    uint8_t cmd = 0;
    
    ESP_LOGI(TAG, "UART RX task started");
    
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(100));
        
        for (int i = 0; i < len; i++) {
            uint8_t b = data[i];
            
            switch (state) {
                case 0:  // Wait for start byte
                    if (b == FRAME_START) {
                        frame_buf[buf_idx++] = b;
                        state = 1;
                    }
                    break;
                    
                case 1:  // Length byte
                    frame_buf[buf_idx++] = b;
                    frame_len = b;
                    if (frame_len == 0 || frame_len > 60) {
                        ESP_LOGW(TAG, "Invalid frame length: %d", frame_len);
                        state = 0;
                        buf_idx = 0;
                    } else {
                        bytes_to_read = frame_len;  // CMD + payload
                        state = 2;
                    }
                    break;
                    
                case 2:  // CMD + Payload
                    frame_buf[buf_idx++] = b;
                    if (buf_idx == 3) {  // Just read CMD byte
                        cmd = b;
                    }
                    bytes_to_read--;
                    if (bytes_to_read == 0) {
                        state = 3;  // Next is CRC
                    }
                    break;
                    
                case 3:  // CRC
                    frame_buf[buf_idx++] = b;
                    uint8_t received_crc = b;
                    uint8_t calc_crc = crc8(&frame_buf[1], buf_idx - 2);
                    
                    if (calc_crc == received_crc) {
                        // Valid frame - extract payload
                        uint8_t payload_len = frame_len - 1;  // Exclude CMD
                        uint8_t *payload = (payload_len > 0) ? &frame_buf[3] : nullptr;
                        
                        // Check if this is a response (0x80+) or command (0x01-0x7F)
                        if (cmd >= 0x80) {
                            // This is a response from S3 - just log it (don't dispatch)
                            ESP_LOGI(TAG, "Received response from S3: 0x%02X", cmd);
                        } else {
                            // This is a command - dispatch it
                            switch (cmd) {
                                case CMD_HELLO:
                                    handle_cmd_hello(payload, payload_len);
                                    break;
                                case CMD_PING:
                                    handle_cmd_ping(payload, payload_len);
                                    break;
                                case CMD_TRIGGER:
                                    handle_cmd_trigger(payload, payload_len);
                                    break;
                                case CMD_SET_MODE:
                                    handle_cmd_set_mode(payload, payload_len);
                                    break;
                                default:
                                    ESP_LOGW(TAG, "Unknown command: 0x%02X", cmd);
                                    uart_send_response(RSP_ERR);
                                    led_error();
                                    break;
                            }
                        }
                    } else {
                        ESP_LOGE(TAG, "CRC error: expected 0x%02X, got 0x%02X", calc_crc, received_crc);
                        led_error();
                    }
                    
                    // Reset for next frame
                    state = 0;
                    buf_idx = 0;
                    break;
            }
        }
    }
    
    free(data);
}

static void open_commissioning_window_if_necessary()
{
    VerifyOrReturn(chip::Server::GetInstance().GetFabricTable().FabricCount() == 0);

    chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    VerifyOrReturn(commissionMgr.IsCommissioningWindowOpen() == false);

    // After removing last fabric, this example does not remove the Wi-Fi credentials
    // and still has IP connectivity so, only advertising on DNS-SD.
    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(300),
                                    chip::CommissioningWindowAdvertisement::kDnssdOnly);
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
    }
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete - notifying S3");
        // Notify S3 that we're now paired with HomeKit
        uart_send_frame(CMD_STATUS_PAIRED, nullptr, 0);
        led_blink(5, 100, 100);  // Celebration blinks!
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed successfully - notifying S3");
        // Notify S3 that we're unpaired
        uart_send_frame(CMD_STATUS_UNPAIRED, nullptr, 0);
        open_commissioning_window_if_necessary();
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;

    default:
        break;
    }
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing a light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// GPIO control functions (defined before they're used)
static esp_err_t init_signal_gpio()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SIGNAL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", SIGNAL_GPIO, esp_err_to_name(err));
        return err;
    }
    
    // Initialize to LOW
    gpio_set_level(SIGNAL_GPIO, 0);
    ESP_LOGI(TAG, "Signal GPIO %d initialized", SIGNAL_GPIO);
    return ESP_OK;
}

static void start_pulse()
{
    if (g_pulse_active) {
        ESP_LOGW(TAG, "Pulse already active, ignoring");
        return;
    }
    
    g_pulse_active = true;
    gpio_set_level(SIGNAL_GPIO, 1);
    ESP_LOGI(TAG, "Pulse started - GPIO %d HIGH", SIGNAL_GPIO);
    
    // Schedule pulse end
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            gpio_set_level(SIGNAL_GPIO, 0);
            g_pulse_active = false;
            ESP_LOGI(TAG, "Pulse ended - GPIO %d LOW", SIGNAL_GPIO);
            
            // Small delay to ensure GPIO state is stable
            vTaskDelay(pdMS_TO_TICKS(10));
            
            // Update Matter attribute back to OFF
            esp_matter_attr_val_t val = esp_matter_bool(false);
            esp_err_t err = attribute::update(g_switch_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Matter attribute updated to OFF successfully");
            } else {
                ESP_LOGE(TAG, "Failed to update Matter attribute to OFF: %s", esp_err_to_name(err));
            }
        },
        .arg = nullptr,
        .name = "pulse_timer"
    };
    
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_once(timer, PULSE_DURATION_MS * 1000); // Convert to microseconds
}

static void stop_pulse()
{
    gpio_set_level(SIGNAL_GPIO, 0);
    g_pulse_active = false;
    ESP_LOGI(TAG, "Pulse stopped - GPIO %d LOW", SIGNAL_GPIO);
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    if (type == PRE_UPDATE) {
        // Handle On/Off cluster commands
        if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
            bool new_state = val->val.b;
            ESP_LOGI(TAG, "On/Off command received on endpoint %d: %s", endpoint_id, new_state ? "ON" : "OFF");
            
            // Check if this is the trigger switch (endpoint 1)
            if (endpoint_id == g_switch_endpoint_id && new_state) {
                ESP_LOGI(TAG, "HomeKit TRIGGER detected - sending UART command to S3");
                uart_send_frame(CMD_TRIGGER, nullptr, 0);
            }
            
            if (new_state) {
                // Matter "ON" command - start pulse
                start_pulse();
            } else {
                // Matter "OFF" command - stop pulse immediately
                stop_pulse();
            }
        }
        
        // Handle 4 Plugin Units for mode selection (4 discrete outlets with robust mutual exclusivity)
        for (int mode = 0; mode < 4; mode++) {
            if (endpoint_id == g_mode_plugin_ids[mode] && 
                cluster_id == OnOff::Id && 
                attribute_id == OnOff::Attributes::OnOff::Id) {
                
                if (val->val.b == true) {  // Plugin turned ON
                    // Ignore callbacks triggered by our own sync task
                    if (g_syncing_modes) {
                        ESP_LOGD(TAG, "Ignoring sync callback for mode %d", mode);
                        break;
                    }
                    
                    // Record the tap - debounce timer will handle it
                    g_target_mode = mode;
                    g_last_tap_time = esp_timer_get_time();
                    ESP_LOGI(TAG, "👆 User tapped mode %d - debouncing (200ms)...", mode);
                } else {
                    // Plugin turned OFF - ignore, sync task enforces mutual exclusivity
                    if (!g_syncing_modes) {
                        ESP_LOGD(TAG, "Mode %d turned OFF by HomeKit (ignoring)", mode);
                    }
                }
                break; // Found the matching endpoint, no need to continue loop
            }
        }
    } else if (type == POST_UPDATE) {
        // Handle post-update to ensure HomeKit gets the final state
        if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
            ESP_LOGI(TAG, "Post-update: On/Off attribute updated to %s", val->val.b ? "ON" : "OFF");
        }
    }
    return ESP_OK;
}

static esp_err_t factory_reset_button_register()
{
    // Create button configurations
    button_config_t button_config = {
        .long_press_time = 5000,     // 5 seconds for long press
        .short_press_time = 50,      // 50ms for short press
    };
    
    button_gpio_config_t gpio_config = {
        .gpio_num = 9,               // GPIO 9 (BOOT button on ESP32-C3 SuperMini)
        .active_level = 0,           // Active low
        .enable_power_save = false,  // No power save
        .disable_pull = false,       // Use internal pull-up
    };
    
    button_handle_t push_button = NULL;
    
    // Create the GPIO button device
    esp_err_t err = iot_button_new_gpio_device(&button_config, &gpio_config, &push_button);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device: %s", esp_err_to_name(err));
        return err;
    }
    
    return app_reset_button_register(push_button);
}

// AFTER factory_reset_button_register(), add switch button callback registration

/* -------------------------------------------------------------------------- */
/* Generic Switch button callback                                             */
/* -------------------------------------------------------------------------- */





// Physical button handling removed - GPIO control is now via Matter commands only

// Button registration removed - GPIO control is now via Matter commands only

// Simple factory reset trigger - will reset after 10 seconds
static void trigger_factory_reset_timer(void)
{
    ESP_LOGW(TAG, "=== FACTORY RESET TRIGGERED ===");
    ESP_LOGW(TAG, "Device will reset in 10 seconds...");
    ESP_LOGW(TAG, "Unplug power now if you want to cancel!");
    
    vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds
    
    ESP_LOGI(TAG, "Starting factory reset NOW");
    esp_matter::factory_reset();
}

// Console command for factory reset
static int factory_reset_cmd(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "confirm") == 0) {
        // Start the reset in a new task
        xTaskCreate([](void*){ trigger_factory_reset_timer(); vTaskDelete(NULL); }, 
                   "factory_reset", 4096, NULL, 5, NULL);
        return 0;
    } else {
        printf("Usage: factory_reset confirm\n");
        printf("WARNING: This will erase all pairing data!\n");
        return 1;
    }
}

static void register_factory_reset_console_cmd()
{
    esp_console_cmd_t cmd = {
        .command = "factory_reset",
        .help = "Perform factory reset (use 'factory_reset confirm')",
        .hint = NULL,
        .func = &factory_reset_cmd,
    };
    esp_console_cmd_register(&cmd);
}

extern "C" void app_main()
{
    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Initialize console for factory reset command */
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    register_factory_reset_console_cmd();
    esp_console_start_repl(repl);

    /* Initialize push button on the dev-kit to reset the device */
    esp_err_t err = factory_reset_button_register();
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to initialize reset button, err:%d", err));

    /* Initialize signal GPIO */
    err = init_signal_gpio();
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to initialize signal GPIO, err:%d", err));

    /* Initialize LED for visual feedback */
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&led_conf);
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to configure LED GPIO, err:%d", err));
    led_off();  // Start with LED off
    ESP_LOGI(TAG, "LED GPIO %d initialized", LED_GPIO);

    /* Initialize UART for S3 communication */
    uart_config_t uart_conf = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    err = uart_param_config(UART_NUM, &uart_conf);
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to configure UART params, err:%d", err));
    
    err = uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to set UART pins, err:%d", err));
    
    err = uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to install UART driver, err:%d", err));
    
    ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d, Baud=%d", UART_TX_PIN, UART_RX_PIN, UART_BAUD);
    
    /* Start UART RX task */
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "UART RX task created");
    
    /* Start Mode Sync task */
    xTaskCreate(mode_sync_task, "mode_sync", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "Mode sync task created");

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config{}; // Explicitly zero-initialize

    // --- BEGIN CUSTOM DEVICE INFO CONFIGURATION ---
    // When 'Device Info Provider' is 'Custom' (via menuconfig),
    // core device identity (VID, PID, names, versions) is primarily expected
    // from the factory NVS partition.
    // Kconfig settings under "Device Basic Information" provide other fields.
    // We will only set the node_label here if we want to override the Kconfig value at runtime.

    // Basic Information Cluster Configuration for Root Node
    // Set a node_label (user-visible device name for this node).
    // This overrides any node_label set via Kconfig.
    strncpy(node_config.root_node.basic_information.node_label, "H-Death", 
            sizeof(node_config.root_node.basic_information.node_label) - 1);
    node_config.root_node.basic_information.node_label[sizeof(node_config.root_node.basic_information.node_label) - 1] = '\0';
    ESP_LOGI(TAG, "Device name set to: H-Death");

    // Identify Cluster on Root Node is mandatory and typically initialized by default by the SDK.

    // --- END CUSTOM DEVICE INFO CONFIGURATION ---

    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    // ------------------------------------------------------------------
    // Create On/Off Plugin Unit endpoint (trigger) - same type as mode buttons for compact UI
    // ------------------------------------------------------------------

    endpoint::on_off_plugin_unit::config_t trigger_cfg; // default config
    endpoint_t *trigger_ep = endpoint::on_off_plugin_unit::create(node, &trigger_cfg, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(trigger_ep != nullptr, ESP_LOGE(TAG, "Failed to create trigger plugin unit endpoint"));

    g_switch_endpoint_id = endpoint::get_id(trigger_ep);
    
    // Set custom name for trigger
    set_endpoint_name(trigger_ep, "🎃 Trigger Skit");

    // ------------------------------------------------------------------
    // Create 4 On/Off Plugin Unit endpoints for mode selection (4 discrete outlets)
    // ------------------------------------------------------------------

    const char* mode_names[] = {"Little Kid", "Big Kid", "Take One", "Closed"};
    const char* mode_emoji_names[] = {"👶 Little Kid", "👦 Big Kid", "🍭 Take One", "🚪 Closed"};
    endpoint::on_off_plugin_unit::config_t mode_plug_cfg;
    
    for (int i = 0; i < 4; i++) {
        endpoint_t *mode_plug_ep = endpoint::on_off_plugin_unit::create(node, &mode_plug_cfg, ENDPOINT_FLAG_NONE, NULL);
        ABORT_APP_ON_FAILURE(mode_plug_ep != nullptr, ESP_LOGE(TAG, "Failed to create %s plugin unit endpoint", mode_names[i]));
        g_mode_plugin_ids[i] = endpoint::get_id(mode_plug_ep);
        
        // Set custom name with emoji
        set_endpoint_name(mode_plug_ep, mode_emoji_names[i]);
        
        ESP_LOGI(TAG, "Created %s plugin unit endpoint (ID: %d)", mode_names[i], g_mode_plugin_ids[i]);
    }

    // Initialize to mode 0 (Little Kid): FORCE sync to clear any stale HomeKit state
    {
        ESP_LOGI(TAG, "=== FORCING MODE 0 (LITTLE KID) ON STARTUP ===");
        
        // Set flag to prevent callback recursion during boot initialization
        g_syncing_modes = true;
        
        esp_matter_attr_val_t off_val = esp_matter_bool(false);
        esp_matter_attr_val_t on_val = esp_matter_bool(true);
        
        // AGGRESSIVELY turn OFF all mode plugins using REPORT to notify HomeKit
        for (int i = 0; i < 4; i++) {
            attribute::report(g_mode_plugin_ids[i], chip::app::Clusters::OnOff::Id,
                              chip::app::Clusters::OnOff::Attributes::OnOff::Id, &off_val);
        }
        
        // Small delay to ensure OFF commands propagate to HomeKit
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Turn ON only the first plugin (Little Kid mode) using REPORT to notify HomeKit
        attribute::report(g_mode_plugin_ids[0], chip::app::Clusters::OnOff::Id,
                          chip::app::Clusters::OnOff::Attributes::OnOff::Id, &on_val);
        
        // Clear flag
        g_syncing_modes = false;
        
        // Set current mode
        g_current_mode = 0;
        
        ESP_LOGI(TAG, "=== MODE INITIALIZATION COMPLETE: Little Kid=ON, all others=OFF ===");
    }

    // GPIO control is now handled via Matter commands only

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    // PrintOnboardingCodes will log the necessary VID/PID and commissioning info
    chip::DeviceLayer::StackLock lock; // RAII lock for Matter stack
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE).Set(chip::RendezvousInformationFlag::kOnNetwork));
}
