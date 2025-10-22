#include "bluetooth_controller.h"
#include "esp_bt.h"
#include "esp_a2dp_api.h"
#include "esp_bt_device.h"
#include "esp_err.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

namespace {
constexpr const char *kLogTag = "BluetoothController";

String formatAddress(const uint8_t *addr) {
    if (!addr) {
        return String("??:??:??:??:??:??");
    }
    char buffer[18];
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return String(buffer);
}
} // namespace

// Static instance pointer
BluetoothController *BluetoothController::s_instance = nullptr;

BluetoothController::BluetoothController() {
    a2dp_source = nullptr;
    m_speaker_name = "";
    m_a2dpConnected = false;
    m_bleConnected = false;
    m_volume = 50;
    m_retryingConnection = false;
    m_lastRetryTime = 0;
    m_retryInterval = 5000; // align with library heartbeat
    m_lastConnectionStateChange = 0;
    m_connectionStateStable = false;
    m_audioProviderCallback = nullptr;
    m_audioProviderContext = nullptr;
    s_instance = this; // Set static instance
}

BluetoothController::~BluetoothController() {
    if (a2dp_source) {
        delete a2dp_source;
        a2dp_source = nullptr;
    }
}

void BluetoothController::initializeA2DP(const String &speaker_name, AudioProviderCallback audioProviderCallback, void *context) {
    m_speaker_name = speaker_name;
    m_audioProviderCallback = audioProviderCallback;
    m_audioProviderContext = context;
    
    // Create A2DP source
    a2dp_source = new BluetoothA2DPSource();
    
    // Set up callbacks using static function pointers
    a2dp_source->set_on_connection_state_changed(staticConnectionStateChanged, this);
    a2dp_source->set_on_audio_state_changed(staticAudioStateChanged, this);
    a2dp_source->set_default_bt_mode(ESP_BT_MODE_BTDM);
    a2dp_source->set_auto_reconnect(true);
    a2dp_source->set_ssid_callback(ssidMatchCallback);

    esp_log_level_set("BT_APP", ESP_LOG_INFO);
    esp_log_level_set("BT_AV", ESP_LOG_INFO);

    logBondedDevices();
    
    Serial.println("🔍 Checking initial A2DP connection state...");
    
    // Start A2DP source
    a2dp_source->start(speaker_name.c_str(), staticDataCallback);
    a2dp_source->set_volume(m_volume);
    
    Serial.println("✅ A2DP Bluetooth initialized: " + speaker_name);
}

bool BluetoothController::isA2dpConnected() {
    return m_a2dpConnected;
}

void BluetoothController::set_volume(uint8_t volume) {
    m_volume = volume;
    if (a2dp_source) {
        a2dp_source->set_volume(volume);
    }
}

const String &BluetoothController::get_speaker_name() const {
    return m_speaker_name;
}

void BluetoothController::update() {
    // Check connection state periodically
    static unsigned long lastConnectionCheck = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastConnectionCheck >= 1000) { // Check every second
        checkConnectionState();
        lastConnectionCheck = currentTime;
    }
    
    // Allow the library to restart discovery if needed
    if (m_retryingConnection && !m_a2dpConnected && !m_connectionStateStable) {
        if (currentTime - m_lastRetryTime >= m_retryInterval) {
            Serial.printf("🔄 Retrying A2DP connection to: %s\n", m_speaker_name.c_str());
            if (a2dp_source) {
                esp_a2d_connection_state_t state = a2dp_source->get_connection_state();
                if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                    a2dp_source->start(m_speaker_name.c_str());
                }
            }
            m_lastRetryTime = currentTime;
        }
    }
}

void BluetoothController::setConnectionStateChangeCallback(std::function<void(int)> callback) {
    m_connectionStateChangeCallback = callback;
}

void BluetoothController::setCharacteristicChangeCallback(std::function<void(const std::string &)> callback) {
    m_characteristicChangeCallback = callback;
}

void BluetoothController::setCharacteristicChangeRequestCallback(std::function<bool(const std::string&)> callback) {
    m_characteristicChangeRequestCallback = callback;
}

bool BluetoothController::setRemoteCharacteristicValue(const std::string &value) {
    // BLE functionality not implemented in this version
    return false;
}

std::string BluetoothController::getRemoteCharacteristicValue() {
    // BLE functionality not implemented in this version
    return "";
}

bool BluetoothController::clientIsConnectedToServer() const {
    return m_bleConnected;
}

bool BluetoothController::serverHasClientConnected() const {
    return m_bleConnected;
}

bool BluetoothController::isBleConnected() const {
    return m_bleConnected;
}

void BluetoothController::onConnectionStateChanged(esp_a2d_connection_state_t state, void *remote_bda) {
    unsigned long currentTime = millis();
    Serial.printf("🔔 Connection state callback triggered: state=%d\n", (int)state);
    
    bool wasConnected = m_a2dpConnected;
    bool isConnectedState = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);

    if (isConnectedState) {
        m_lastConnectionStateChange = currentTime;
        m_a2dpConnected = true;

        if (remote_bda) {
            Serial.printf("🤝 Connected to %s\n", formatAddress(reinterpret_cast<uint8_t *>(remote_bda)).c_str());
        }

        if (!wasConnected) {
            Serial.println("🔗 A2DP Connected!");
            stopConnectionRetry(); // Stop retrying when connected
            m_connectionStateStable = true;
        } else {
            Serial.println("🔄 Connection state unchanged: Connected");
        }

        if (m_connectionStateChangeCallback) {
            m_connectionStateChangeCallback((int)state);
        }
        return;
    }

    // Debounce rapid disconnect notifications only when we're already disconnected
    if (m_lastConnectionStateChange != 0 && (currentTime - m_lastConnectionStateChange) < 2000 && !wasConnected) {
        Serial.println("⏳ Ignoring rapid state change (debouncing)");
        return;
    }

    m_lastConnectionStateChange = currentTime;
    m_a2dpConnected = false;

    if (wasConnected) {
        Serial.println("🔌 A2DP Disconnected!");
        startConnectionRetry(); // Start retrying when disconnected
        m_connectionStateStable = false;
    } else {
        Serial.println("🔄 Connection state unchanged: Disconnected");
    }

    if (m_connectionStateChangeCallback) {
        m_connectionStateChangeCallback((int)state);
    }
}

void BluetoothController::onAudioStateChanged(esp_a2d_audio_state_t state, void *remote_bda) {
    const char *stateStr = "UNKNOWN";
    switch (state) {
        case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
            stateStr = "SUSPENDED";
            break;
        case ESP_A2D_AUDIO_STATE_STOPPED:
            stateStr = "STOPPED";
            break;
        case ESP_A2D_AUDIO_STATE_STARTED:
            stateStr = "STARTED";
            break;
    }
    Serial.printf("🎧 A2DP audio state changed: %s\n", stateStr);
}

void BluetoothController::onVolumeChanged(uint8_t volume) {
    m_volume = volume;
    Serial.printf("🔊 Volume changed to: %d\n", volume);
}

// Static callback implementations
void BluetoothController::staticConnectionStateChanged(esp_a2d_connection_state_t state, void *remote_bda) {
    if (s_instance) {
        s_instance->onConnectionStateChanged(state, remote_bda);
    }
}

int32_t BluetoothController::staticDataCallback(Frame *data, int32_t len) {
    if (s_instance && s_instance->m_audioProviderCallback) {
        return s_instance->m_audioProviderCallback(s_instance->m_audioProviderContext, data, len);
    }
    return 0;
}

void BluetoothController::staticAudioStateChanged(esp_a2d_audio_state_t state, void *remote_bda) {
    if (s_instance) {
        s_instance->onAudioStateChanged(state, remote_bda);
    }
}

bool BluetoothController::ssidMatchCallback(const char *ssid, esp_bd_addr_t address, int rssi) {
    if (!s_instance) {
        return false;
    }
    if (!ssid) {
        return false;
    }
    String reported(ssid);
    reported.trim();

    String target = s_instance->m_speaker_name;
    target.trim();

    bool match = reported.equalsIgnoreCase(target);
    if (match) {
        Serial.printf("🔎 Found target speaker %s (RSSI %d)\n", ssid, rssi);
        Serial.printf("🔗 Cached address %s\n", formatAddress(address).c_str());
    }
    return match;
}

void BluetoothController::startConnectionRetry() {
    m_retryingConnection = true;
    m_lastRetryTime = millis();
    Serial.println("🔄 Starting A2DP connection retry...");
}

void BluetoothController::stopConnectionRetry() {
    m_retryingConnection = false;
    Serial.println("✅ Stopping A2DP connection retry.");
}

bool BluetoothController::isRetryingConnection() const {
    return m_retryingConnection;
}

void BluetoothController::checkConnectionState() {
    if (!a2dp_source) return;
    
    if (!m_retryingConnection && !m_a2dpConnected) {
        Serial.println("🔍 Checking if A2DP is actually connected...");
    }
}

void BluetoothController::logBondedDevices() {
    int count = esp_bt_gap_get_bond_device_num();
    if (count <= 0) {
        Serial.println("ℹ️ No bonded Bluetooth devices recorded.");
        return;
    }

    esp_bd_addr_t *devices = new esp_bd_addr_t[count];
    if (esp_bt_gap_get_bond_device_list(&count, devices) == ESP_OK) {
        Serial.printf("ℹ️ Bonded devices (%d):\n", count);
        for (int i = 0; i < count; ++i) {
            Serial.printf("   • %s\n", formatAddress(devices[i]).c_str());
        }
    } else {
        Serial.println("⚠️ Failed to read bonded device list.");
    }
    delete[] devices;
}
