#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include "esp_bt_defs.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"

class BluetoothController {
public:
    BluetoothController();
    ~BluetoothController();
    
    // Initialize the Bluetooth controller with A2DP
    typedef int32_t (*AudioProviderCallback)(void *context, Frame *, int32_t);
    void initializeA2DP(const String &speaker_name, AudioProviderCallback audioProviderCallback, void *context);
    
    // Check if A2DP is currently connected
    bool isA2dpConnected();
    
    // Set the volume for A2DP audio
    void set_volume(uint8_t volume);
    
    // Get the name of the connected Bluetooth speaker
    const String &get_speaker_name() const;
    
    // Update the Bluetooth controller state (call this in the main loop)
    void update();
    
    // Connection retry methods
    void startConnectionRetry();
    void stopConnectionRetry();
    bool isRetryingConnection() const;
    void pauseForOta();
    void resumeAfterOta();
    
    // Manual connection state checking
    void checkConnectionState();
    
    // Set callbacks
    void setConnectionStateChangeCallback(std::function<void(int)> callback);
    void setCharacteristicChangeCallback(std::function<void(const std::string &)> callback);
    void setCharacteristicChangeRequestCallback(std::function<bool(const std::string&)> callback);
    
    // BLE methods (simplified for compatibility)
    bool setRemoteCharacteristicValue(const std::string &value);
    std::string getRemoteCharacteristicValue();
    bool clientIsConnectedToServer() const;
    bool serverHasClientConnected() const;
    bool isBleConnected() const;
    
private:
    BluetoothA2DPSource *a2dp_source;
    String m_speaker_name;
    AudioProviderCallback m_audioProviderCallback;
    void *m_audioProviderContext;
    bool m_a2dpConnected;
    bool m_bleConnected;
    uint8_t m_volume;
    
    // Static instance for callbacks
    static BluetoothController *s_instance;
    
    // Connection retry state
    bool m_retryingConnection;
    unsigned long m_lastRetryTime;
    unsigned long m_retryInterval;
    
    // Connection state debouncing
    unsigned long m_lastConnectionStateChange;
    bool m_connectionStateStable;
    
    // Callbacks
    std::function<void(int)> m_connectionStateChangeCallback;
    std::function<void(const std::string &)> m_characteristicChangeCallback;
    std::function<bool(const std::string&)> m_characteristicChangeRequestCallback;
    
    // A2DP callback functions
    void onConnectionStateChanged(esp_a2d_connection_state_t state, void *remote_bda);
    void onAudioStateChanged(esp_a2d_audio_state_t state, void *remote_bda);
    void onVolumeChanged(uint8_t volume);
    
    // Static callback functions for ESP32-A2DP
    static void staticConnectionStateChanged(esp_a2d_connection_state_t state, void *remote_bda);
    static int32_t staticDataCallback(Frame *data, int32_t len);
    static void staticAudioStateChanged(esp_a2d_audio_state_t state, void *remote_bda);
    static bool ssidMatchCallback(const char *ssid, esp_bd_addr_t address, int rssi);
    void logBondedDevices();
    void requestMediaStart(uint32_t delayMs = 100);
    void processMediaStart();
    void performDeferredResume();
    void finalizePause();

    bool m_mediaStartPending;
    unsigned long m_mediaStartDeadlineMs;
    void startA2dp();

    enum class PauseState {
        Idle,
        WaitingForDisconnect,
        Paused
    };

    PauseState m_pauseState;
    bool m_controllerWasEnabledBeforeOta;
    bool m_bluedroidWasEnabledBeforeOta;
    bool m_resumeDeferred;
    unsigned long m_resumeAfterMillis;
    unsigned long m_disconnectDeadlineMs;

    static constexpr unsigned long kResumeDelayMs = 8000;
};

#endif // BLUETOOTH_CONTROLLER_H
