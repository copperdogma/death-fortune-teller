#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h" // This should include the Frame struct definition
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <functional>
#include <string>

// Enum to represent the current connection state of the Bluetooth controller
enum class ConnectionState
{
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    CONNECTED
};

// Main class for managing Bluetooth functionality
class bluetooth_controller
{
public:
    bluetooth_controller();

    // Initialize the Bluetooth controller
    // @param speaker_name: Name of the Bluetooth speaker to connect to
    // @param audioProviderCallback: Callback function to provide audio data
    // @param isPrimary: Whether this device is the primary or secondary skull
    void begin(const String &speaker_name, std::function<int32_t(Frame *, int32_t)> audioProviderCallback, bool isPrimary);

    // Separate A2DP and BLE initialization
    void initializeA2DP(const String &speaker_name, std::function<int32_t(Frame *, int32_t)> audioProviderCallback);
    void initializeBLE(bool isPrimary);

    // Check if A2DP is currently connected
    bool isA2dpConnected();

    // Set the volume for A2DP audio
    // @param volume: Volume level (0-255)
    void set_volume(uint8_t volume);

    // Get the name of the connected Bluetooth speaker
    const String &get_speaker_name() const;

    // Set the value of the BLE characteristic (for server mode)
    void setCharacteristicValue(const char *value);

    // Update the Bluetooth controller state (call this in the main loop)
    void update();

    // Singleton instance of the Bluetooth controller
    static bluetooth_controller *instance;

    // Set the value of the remote BLE characteristic (for client mode)
    bool setRemoteCharacteristicValue(const std::string &value);

    // Register for indications from the remote BLE characteristic
    bool registerForIndications();

    // Check if the BLE client is connected to a server
    bool clientIsConnectedToServer() const;

    // Check if the BLE server has a connected client
    bool serverHasClientConnected() const;

    // Check if a client-server connection exists
    bool isBleConnected() const;

    // Set the BLE client connection status
    void setBLEClientConnectionStatus(bool status);

    // Set the BLE server connection status
    void setBLEServerConnectionStatus(bool status);

    // Start scanning for BLE devices
    void startScan();

    // Connect to a BLE server
    bool connectToServer();

    // Get the current connection state
    ConnectionState getConnectionState() const { return m_connectionState; }

    // Set the current connection state
    void setConnectionState(ConnectionState newState)
    {
        if (m_connectionState != newState)
        {
            m_connectionState = newState;
            if (m_connectionStateChangeCallback)
            {
                m_connectionStateChangeCallback(m_connectionState);
            }
        }
    }

    // Set the discovered BLE device (internal use)
    void setMyDevice(BLEAdvertisedDevice *device) { myDevice = device; }

    // Set the connection start time (internal use)
    void setConnectionStartTime(unsigned long time) { connectionStartTime = time; }

    // Check if the server is still advertising
    bool isServerAdvertising();

    // Getter methods for UUIDs
    static constexpr const char *getServerServiceUUID() { return SERVER_SERVICE_UUID; }
    static constexpr const char *getCharacteristicUUID() { return CHARACTERISTIC_UUID; }

    // Add this new typedef for the connection state change callback
    typedef std::function<void(ConnectionState)> ConnectionStateChangeCallback;

    // Add this new method to set the callback
    void setConnectionStateChangeCallback(ConnectionStateChangeCallback callback)
    {
        m_connectionStateChangeCallback = callback;
    }

    // Add this new method to get the connection state string
    static std::string getConnectionStateString(ConnectionState state);

    // Add this new typedef for the characteristic change callback
    typedef std::function<void(const std::string &)> CharacteristicChangeCallback;

    // Add this new method to set the callback
    void setCharacteristicChangeCallback(CharacteristicChangeCallback callback)
    {
        m_characteristicChangeCallback = callback;
    }

    // Add this new public method
    void triggerCharacteristicChangeCallback(const std::string &value)
    {
        if (m_characteristicChangeCallback)
        {
            m_characteristicChangeCallback(value);
        }
    }

    // New method to check if both A2DP and BLE are initialized
    bool isFullyInitialized() const;

    void setCharacteristicChangeRequestCallback(std::function<bool(const std::string&)> callback);
    std::string getRemoteCharacteristicValue();

    std::function<bool(const std::string&)> m_characteristicChangeRequestCallback = nullptr;

private:
    BLEScan *pBLEScanner;
    BLEClient *pClient;
    BLECharacteristic *pCharacteristic;
    BLERemoteCharacteristic *pRemoteCharacteristic;

    void initializeBLEServer();
    void initializeBLEClient();
    static int audio_callback_trampoline(Frame *frame, int frame_count);
    static void connection_state_changed(esp_a2d_connection_state_t state, void *ptr);

    bool m_isPrimary;
    String m_speaker_name;
    std::function<int32_t(Frame *, int32_t)> audio_provider_callback;
    unsigned long last_reconnection_attempt;
    BluetoothA2DPSource a2dp_source;

    static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
    void handleIndication(const std::string &value);

    bool indicationReceived;
    bool m_clientIsConnectedToServer;
    bool m_serverHasClientConnected;

    ConnectionState m_connectionState;
    unsigned long m_lastReconnectAttempt;
    unsigned long connectionStartTime; // Add this line if it's not already present
    unsigned long scanStartTime;

    void disconnectFromServer();

    static BLEAdvertisedDevice *myDevice;

    ConnectionStateChangeCallback m_connectionStateChangeCallback = nullptr;
    CharacteristicChangeCallback m_characteristicChangeCallback = nullptr;

    // UUIDs for BLE services and characteristics
    static constexpr const char *SERVER_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    static constexpr const char *CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

    // Timing constants
    static const unsigned long SCAN_INTERVAL = 10000;      // 10 seconds between scan attempts
    static const unsigned long SCAN_DURATION = 10000;      // 10 seconds scan duration
    static const unsigned long CONNECTION_TIMEOUT = 30000; // 30 seconds connection timeout
    static const unsigned long SCAN_TIMEOUT = 30000;       // 30 seconds

    bool m_a2dpInitialized;
    bool m_bleInitialized;
};

#endif // BLUETOOTH_CONTROLLER_H