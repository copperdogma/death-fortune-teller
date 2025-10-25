#include "bluetooth_controller.h"
#include "logging_manager.h"

static constexpr const char* TAG = "Bluetooth";
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_a2dp_api.h"
#include "esp_bt_device.h"
#include "esp_err.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

#include "ota_manager.h"
// ESP32-A2DP keeps global pointers to the active controller instance; we reset them around OTA teardown.
extern BluetoothA2DPCommon *actual_bluetooth_a2dp_common;
extern BluetoothA2DPSource *actual_bluetooth_a2dp_source;

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
    m_mediaStartPending = false;
    m_mediaStartDeadlineMs = 0;
    m_pauseState = PauseState::Idle;
    m_controllerWasEnabledBeforeOta = false;
    m_bluedroidWasEnabledBeforeOta = false;
    m_resumeDeferred = false;
    m_resumeAfterMillis = 0;
    m_disconnectDeadlineMs = 0;
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
    startA2dp();
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
            LOG_INFO(TAG, "🔄 Retrying A2DP connection to: %s", m_speaker_name.c_str());
            if (a2dp_source) {
                esp_a2d_connection_state_t state = a2dp_source->get_connection_state();
                if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                    a2dp_source->start(m_speaker_name.c_str());
                }
            }
            m_lastRetryTime = currentTime;
        }
    }

    processMediaStart();

    if (m_resumeDeferred) {
        OTAManager *ota = OTAManager::instance();
        if (ota && ota->isUpdating()) {
            m_resumeAfterMillis = currentTime + kResumeDelayMs;
        } else if (currentTime >= m_resumeAfterMillis) {
            performDeferredResume();
        }
    }

    if (m_pauseState == PauseState::WaitingForDisconnect) {
        bool disconnected = true;
        if (a2dp_source) {
            esp_a2d_connection_state_t state = a2dp_source->get_connection_state();
            disconnected = (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED);
        }
        if (disconnected || currentTime >= m_disconnectDeadlineMs) {
            finalizePause();
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
    LOG_DEBUG(TAG, "🔔 Connection state callback triggered: state=%d", static_cast<int>(state));
    
    bool wasConnected = m_a2dpConnected;
    bool isConnectedState = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);

    if (isConnectedState) {
        m_lastConnectionStateChange = currentTime;
        m_a2dpConnected = true;

        if (remote_bda) {
            LOG_INFO(TAG, "🤝 Connected to %s", formatAddress(reinterpret_cast<uint8_t *>(remote_bda)).c_str());
        }

        if (!wasConnected) {
            LOG_INFO(TAG, "🔗 A2DP Connected!");
            stopConnectionRetry(); // Stop retrying when connected
            m_connectionStateStable = true;
        } else {
            LOG_DEBUG(TAG, "🔄 Connection state unchanged: Connected");
        }

        requestMediaStart(200);

        if (m_connectionStateChangeCallback) {
            m_connectionStateChangeCallback((int)state);
        }
        return;
    }

    // Debounce rapid disconnect notifications only when we're already disconnected
    if (m_lastConnectionStateChange != 0 && (currentTime - m_lastConnectionStateChange) < 2000 && !wasConnected) {
        LOG_DEBUG(TAG, "⏳ Ignoring rapid state change (debouncing)");
        return;
    }

    m_lastConnectionStateChange = currentTime;
    m_a2dpConnected = false;

    if (wasConnected) {
        LOG_WARN(TAG, "🔌 A2DP Disconnected!");
        startConnectionRetry(); // Start retrying when disconnected
        m_connectionStateStable = false;
    } else {
        LOG_DEBUG(TAG, "🔄 Connection state unchanged: Disconnected");
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
    LOG_INFO(TAG, "🎧 A2DP audio state changed: %s", stateStr);

    if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        m_mediaStartPending = false;
    } else if (state == ESP_A2D_AUDIO_STATE_STOPPED && m_a2dpConnected) {
        requestMediaStart(250);
    }
}

void BluetoothController::onVolumeChanged(uint8_t volume) {
    m_volume = volume;
    LOG_INFO(TAG, "🔊 Volume changed to: %d", volume);
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
        LOG_INFO(TAG, "🔎 Found target speaker %s (RSSI %d)", ssid, rssi);
        LOG_INFO(TAG, "🔗 Cached address %s", formatAddress(address).c_str());
    }
    return match;
}

void BluetoothController::startConnectionRetry() {
    m_retryingConnection = true;
    m_lastRetryTime = millis();
    LOG_INFO(TAG, "🔄 Starting A2DP connection retry...");
}

void BluetoothController::stopConnectionRetry() {
    m_retryingConnection = false;
    LOG_INFO(TAG, "✅ Stopping A2DP connection retry.");
}

bool BluetoothController::isRetryingConnection() const {
    return m_retryingConnection;
}

void BluetoothController::pauseForOta() {
    if (m_pauseState != PauseState::Idle) {
        LOG_DEBUG(TAG, "Bluetooth already handling OTA pause (state=%d)", static_cast<int>(m_pauseState));
        return;
    }

    LOG_INFO(TAG, "⏸️ Pausing Bluetooth for OTA");
    m_resumeDeferred = false;
    stopConnectionRetry();
    m_connectionStateStable = false;
    m_mediaStartPending = false;
    m_controllerWasEnabledBeforeOta = false;
    m_bluedroidWasEnabledBeforeOta = false;

    if (a2dp_source) {
        esp_a2d_connection_state_t currentState = a2dp_source->get_connection_state();
        if (currentState == ESP_A2D_CONNECTION_STATE_CONNECTED || currentState == ESP_A2D_CONNECTION_STATE_CONNECTING) {
            LOG_INFO(TAG, "🔻 Disconnecting A2DP link before OTA (state=%d)", static_cast<int>(currentState));
            a2dp_source->disconnect();
            m_pauseState = PauseState::WaitingForDisconnect;
            m_disconnectDeadlineMs = millis() + 1500;
        } else {
            finalizePause();
        }
    } else {
        finalizePause();
    }
    m_a2dpConnected = false;
}

void BluetoothController::resumeAfterOta() {
    if (!m_audioProviderCallback) {
        LOG_WARN(TAG, "Cannot resume Bluetooth after OTA – audio provider not set");
        return;
    }

    if (m_pauseState == PauseState::WaitingForDisconnect) {
        finalizePause();
    }

    LOG_INFO(TAG, "⏱️ Scheduling Bluetooth resume %.1fs after OTA", kResumeDelayMs / 1000.0f);
    m_resumeDeferred = true;
    m_resumeAfterMillis = millis() + kResumeDelayMs;
}

void BluetoothController::finalizePause() {
    if (m_pauseState == PauseState::Paused) {
        return;
    }

    if (a2dp_source) {
        a2dp_source->end(false);
        delete a2dp_source;
        a2dp_source = nullptr;
        actual_bluetooth_a2dp_common = nullptr;
        actual_bluetooth_a2dp_source = nullptr;
    }

    m_disconnectDeadlineMs = 0;

    esp_bluedroid_status_t bluedroidStatus = esp_bluedroid_get_status();
    if (bluedroidStatus == ESP_BLUEDROID_STATUS_ENABLED) {
        esp_err_t disableResult = esp_bluedroid_disable();
        if (disableResult != ESP_OK && disableResult != ESP_ERR_INVALID_STATE) {
            LOG_ERROR(TAG, "Failed to disable Bluedroid for OTA: %s", esp_err_to_name(disableResult));
        } else {
            unsigned long deadline = millis() + 500;
            while (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED && millis() < deadline) {
                delay(10);
            }
            if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
                m_bluedroidWasEnabledBeforeOta = true;
            } else {
                LOG_WARN(TAG, "Timed out waiting for Bluedroid to disable before OTA");
            }
        }
    }

    esp_bt_controller_status_t controllerStatus = esp_bt_controller_get_status();
    if (controllerStatus == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        esp_err_t disableController = esp_bt_controller_disable();
        if (disableController != ESP_OK && disableController != ESP_ERR_INVALID_STATE) {
            LOG_ERROR(TAG, "Failed to disable Bluetooth controller for OTA: %s", esp_err_to_name(disableController));
        } else {
            unsigned long deadline = millis() + 500;
            while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED && millis() < deadline) {
                delay(10);
            }
            if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
                m_controllerWasEnabledBeforeOta = true;
            } else {
                LOG_WARN(TAG, "Timed out waiting for Bluetooth controller to disable before OTA");
            }
        }
    } else if (controllerStatus == ESP_BT_CONTROLLER_STATUS_INITED) {
        m_controllerWasEnabledBeforeOta = true;
    }

    m_pauseState = PauseState::Paused;
}

void BluetoothController::performDeferredResume() {
    if (!m_audioProviderCallback) {
        LOG_WARN(TAG, "Cannot resume Bluetooth after OTA – audio provider not set");
        return;
    }

    m_resumeDeferred = false;

    if (m_controllerWasEnabledBeforeOta) {
        esp_bt_controller_status_t controllerStatus = esp_bt_controller_get_status();
        if (controllerStatus == ESP_BT_CONTROLLER_STATUS_INITED) {
            esp_err_t enableResult = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
            if (enableResult != ESP_OK && enableResult != ESP_ERR_INVALID_STATE) {
                LOG_ERROR(TAG, "Failed to enable Bluetooth controller after OTA: %s", esp_err_to_name(enableResult));
            } else {
                unsigned long deadline = millis() + 500;
                while (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED && millis() < deadline) {
                    delay(10);
                }
            }
        }
    }

    if (m_bluedroidWasEnabledBeforeOta) {
        esp_bluedroid_status_t bluedroidStatus = esp_bluedroid_get_status();
        if (bluedroidStatus == ESP_BLUEDROID_STATUS_INITIALIZED) {
            esp_err_t enableResult = esp_bluedroid_enable();
            if (enableResult != ESP_OK && enableResult != ESP_ERR_INVALID_STATE) {
                LOG_ERROR(TAG, "Failed to enable Bluedroid after OTA: %s", esp_err_to_name(enableResult));
            }
        } else if (bluedroidStatus == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
            esp_err_t initResult = esp_bluedroid_init();
            if (initResult == ESP_OK) {
                esp_err_t enableResult = esp_bluedroid_enable();
                if (enableResult != ESP_OK && enableResult != ESP_ERR_INVALID_STATE) {
                    LOG_ERROR(TAG, "Failed to enable Bluedroid after OTA: %s", esp_err_to_name(enableResult));
                }
            } else if (initResult != ESP_ERR_INVALID_STATE) {
                LOG_ERROR(TAG, "Failed to init Bluedroid after OTA: %s", esp_err_to_name(initResult));
            }
        }
    }

    if (!a2dp_source) {
        startA2dp();
    }
    LOG_INFO(TAG, "▶️ Resuming Bluetooth after OTA");
    m_a2dpConnected = false;
    m_connectionStateStable = false;
    m_mediaStartPending = false;
    startConnectionRetry();
    m_controllerWasEnabledBeforeOta = false;
    m_bluedroidWasEnabledBeforeOta = false;
    m_pauseState = PauseState::Idle;
}

void BluetoothController::startA2dp() {
    if (!m_audioProviderCallback) {
        LOG_WARN(TAG, "Cannot start A2DP – audio provider not initialized");
        return;
    }
    if (a2dp_source) {
        delete a2dp_source;
        a2dp_source = nullptr;
    }

    a2dp_source = new BluetoothA2DPSource();
    a2dp_source->set_on_connection_state_changed(staticConnectionStateChanged, this);
    a2dp_source->set_on_audio_state_changed(staticAudioStateChanged, this);
    a2dp_source->set_default_bt_mode(ESP_BT_MODE_BTDM);
    a2dp_source->set_auto_reconnect(true);
    a2dp_source->set_ssid_callback(ssidMatchCallback);

    esp_log_level_set("BT_APP", ESP_LOG_INFO);
    esp_log_level_set("BT_AV", ESP_LOG_INFO);

    logBondedDevices();

    LOG_INFO(TAG, "🔍 Starting A2DP source for speaker %s", m_speaker_name.c_str());
    a2dp_source->start(m_speaker_name.c_str(), staticDataCallback);
    a2dp_source->set_volume(m_volume);
    LOG_INFO(TAG, "✅ A2DP Bluetooth initialized: %s", m_speaker_name.c_str());
    m_mediaStartPending = false;
    m_connectionStateStable = false;
}

void BluetoothController::checkConnectionState() {
    if (!a2dp_source) return;
    
    if (!m_retryingConnection && !m_a2dpConnected) {
        LOG_DEBUG(TAG, "🔍 Checking if A2DP is actually connected...");
    }
}

void BluetoothController::logBondedDevices() {
    int count = esp_bt_gap_get_bond_device_num();
    if (count <= 0) {
        LOG_INFO(TAG, "ℹ️ No bonded Bluetooth devices recorded.");
        return;
    }

    esp_bd_addr_t *devices = new esp_bd_addr_t[count];
    if (esp_bt_gap_get_bond_device_list(&count, devices) == ESP_OK) {
        LOG_INFO(TAG, "ℹ️ Bonded devices (%d):", count);
        for (int i = 0; i < count; ++i) {
            LOG_INFO(TAG, "   • %s", formatAddress(devices[i]).c_str());
        }
    } else {
        LOG_WARN(TAG, "⚠️ Failed to read bonded device list.");
    }
    delete[] devices;
}

void BluetoothController::requestMediaStart(uint32_t delayMs) {
    unsigned long scheduled = millis() + delayMs;
    if (m_mediaStartPending) {
        if (scheduled < m_mediaStartDeadlineMs) {
            m_mediaStartDeadlineMs = scheduled;
        }
        return;
    }
    m_mediaStartPending = true;
    m_mediaStartDeadlineMs = scheduled;
}

void BluetoothController::processMediaStart() {
    if (!m_mediaStartPending || !a2dp_source) {
        return;
    }
    if (!m_a2dpConnected) {
        m_mediaStartPending = false;
        return;
    }
    unsigned long now = millis();
    if ((long)(now - m_mediaStartDeadlineMs) < 0) {
        return;
    }

    esp_err_t checkResult = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
    if (checkResult != ESP_OK && checkResult != ESP_ERR_INVALID_STATE) {
        LOG_WARN(TAG, "⚠️ MEDIA_CTRL_CHECK_SRC_RDY failed: %s", esp_err_to_name(checkResult));
        m_mediaStartDeadlineMs = now + 200;
        return;
    }

    esp_err_t startResult = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
    if (startResult == ESP_OK) {
        LOG_INFO(TAG, "▶️ Requested A2DP media start");
        m_mediaStartPending = false;
    } else {
        LOG_WARN(TAG, "⚠️ MEDIA_CTRL_START failed: %s", esp_err_to_name(startResult));
        m_mediaStartDeadlineMs = now + 200;
    }
}
