/*
    Bluetooth Controller Class

    This class manages both A2DP audio streaming and BLE skull-to-skull communication for an ESP32-WROVER platform.
    It handles the following main functionalities:
    1. A2DP audio streaming to a Bluetooth speaker
    2. BLE communication between two skull devices (primary and secondary)

    The class can operate in two modes:
    - Primary mode: Acts as a BLE client, connecting to a secondary skull
    - Secondary mode: Acts as a BLE server, waiting for a primary skull to connect

    Note: Ideally, this should be split into two separate classes for better separation of concerns.

    Dependencies:
    - ESP32 BLE and A2DP libraries
    - SoundData.h from the ESP32-A2DP library (for the Frame struct used in A2DP streaming)

    Note: Frame (for A2DP audio streaming) is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

    struct __attribute__((packed)) Frame {
        int16_t channel1;
        int16_t channel2;

        Frame(int v=0){
            channel1 = channel2 = v;
        }

        Frame(int ch1, int ch2){
            channel1 = ch1;
            channel2 = ch2;
        }
    };
*/

#include "bluetooth_controller.h"
#include <cstring>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "nvs_flash.h"

// BLE-related includes and definitions
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Global variables for BLE communication
BLECharacteristic *pCharacteristic;

// Connection status flags
static bool serverHasClientConnected = false;                  // Indicates if the BLE server (secondary skull) has a connected client
static boolean clientIsConnectedToServer = false;              // Indicates if the BLE client (primary skull) is connected to a server
BLEAdvertisedDevice *bluetooth_controller::myDevice = nullptr; // Stores the discovered BLE server device

// BLE scanning-related variables
static BLEScan *pBLEScan = nullptr; // BLE scanner object
static bool isScanning = false;     // Flag to track if BLE scanning is in progress

// Callback class for handling BLE characteristic writes
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            Serial.println("*********");
            Serial.print("New value: ");
            Serial.println(value.c_str());
            Serial.println("*********");

            // Call the callback to check if the change is acceptable
            bool canAcceptChange = true;
            if (bluetooth_controller::instance && bluetooth_controller::instance->m_characteristicChangeRequestCallback)
            {
                canAcceptChange = bluetooth_controller::instance->m_characteristicChangeRequestCallback(value);
            }

            if (canAcceptChange)
            {
                pCharacteristic->notify(); // Notify the client
                bluetooth_controller::instance->triggerCharacteristicChangeCallback(value);
            }
            else
            {
                // If change is not acceptable, set an error message
                std::string errorMsg = "Error: Cannot play " + value;
                pCharacteristic->setValue(errorMsg);
                pCharacteristic->notify();
            }
        }
    }
};

// Callback class for handling BLE server events (connect/disconnect)
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
    {
        bluetooth_controller::instance->setBLEServerConnectionStatus(true);
        bluetooth_controller::instance->setConnectionState(ConnectionState::CONNECTED);

        // Log client address and connection details
        char remoteAddress[18];
        sprintf(remoteAddress, "%02x:%02x:%02x:%02x:%02x:%02x",
                param->connect.remote_bda[0], param->connect.remote_bda[1],
                param->connect.remote_bda[2], param->connect.remote_bda[3],
                param->connect.remote_bda[4], param->connect.remote_bda[5]);

        Serial.println("BT-BLE: Client connected!");
        Serial.printf("BT-BLE: Client Address: %s\n", remoteAddress);
        Serial.printf("BT-BLE: Connection ID: %d\n", param->connect.conn_id);
        Serial.printf("BT-BLE: Connection Handle: %d\n", param->connect.conn_handle);
    }

    void onDisconnect(BLEServer *pServer)
    {
        bluetooth_controller::instance->setBLEServerConnectionStatus(false);
        bluetooth_controller::instance->setConnectionState(ConnectionState::DISCONNECTED);
        Serial.println("BT-BLE: Client disconnected");

        // Restart advertising to allow new connections
        BLEDevice::startAdvertising();
        Serial.println("BT-BLE: Restarted advertising after disconnection");
    }
};

// Static instance pointer initialization
bluetooth_controller *bluetooth_controller::instance = nullptr;

// Constructor
bluetooth_controller::bluetooth_controller()
    : m_speaker_name(""),
      m_clientIsConnectedToServer(false),
      m_serverHasClientConnected(false),
      m_connectionState(ConnectionState::DISCONNECTED),
      m_lastReconnectAttempt(0),
      scanStartTime(0),
      connectionStartTime(0),
      m_a2dpInitialized(false),
      m_bleInitialized(false)
{
    instance = this; // Ensure proper initialization of the static instance
}

void bluetooth_controller::initializeA2DP(const String &speaker_name, std::function<int32_t(Frame *, int32_t)> audioProviderCallback)
{
    Serial.println("BT: Initializing Bluetooth A2DP...");

    m_speaker_name = speaker_name;
    audio_provider_callback = audioProviderCallback;

    // Start A2DP (audio streaming)
    Serial.printf("BT-A2DP: Starting as A2DP source, connecting to speaker name: %s\n", m_speaker_name.c_str());

    a2dp_source.set_default_bt_mode(ESP_BT_MODE_BTDM); // Essential to use A2DP and BLE at the same time
    a2dp_source.set_auto_reconnect(true);
    a2dp_source.set_on_connection_state_changed(connection_state_changed, this);
    a2dp_source.start(m_speaker_name.c_str(), audio_callback_trampoline);

    m_a2dpInitialized = true;
    Serial.println("BT: Bluetooth A2DP initialization complete.");
}

void bluetooth_controller::initializeBLE(bool isPrimary)
{
    Serial.println("BT: Initializing Bluetooth BLE...");

    m_isPrimary = isPrimary;

    // Initialize BLE based on whether this is the primary or secondary skull
    if (m_isPrimary)
    {
        initializeBLEClient();
    }
    else
    {
        initializeBLEServer();
    }

    m_bleInitialized = true;
    Serial.println("BT: Bluetooth BLE initialization complete.");
}

// Initialize the BLE client (for primary skull)
void bluetooth_controller::initializeBLEClient()
{
    Serial.println("BT-BLE: Starting as BLE PRIMARY (client)");
    if (!BLEDevice::getInitialized())
    {
        BLEDevice::init("SkullPrimary-Client");
        if (!BLEDevice::getInitialized())
        {
            Serial.println("BT-BLE: Failed to initialize BLEDevice!");
            return;
        }
    }
    startScan();
}

/*
    Initialize the BLE server (for secondary skull).

    Note: This service/characteristic comes up (correctly) in a BLE scanner as:

        **Advertised Services**
        Unknown Service    -- the service itself
        UUID: 4FAFC201-1FB5-459E-8FCC-C5C9C331914B

        **Attribute Table**
        Unknown Service  -- generic access service (standard Bluetooth service)
        UUID: 1800
        PRIMARY SERVICE

        Unknown Service  -- generic attribute service (standard Bluetooth service)
        UUID: 1801
        PRIMARY SERVICE

        Unknown Service  -- the service itself
        UUID: 4FAFC201-1FB5-459E-8FCC-C5C9C331914B
        PRIMARY SERVICE

        Unknown Characteristic  -- the characteristic itself
        UUID: BEB5483E-36E1-4688-B7F5-EA07361B26A8
        Properties: Read, Write, and Indicate
        Value: Hello from SkullSecondary
        Value Sent: Testing can

        Unknown Descriptor  -- the Client Characteristic Configuration Descriptor (CCCD) we added for indications
        UUID: 2902
        Value: Disabled
        Value Sent: N/A

        ---

        Device Name: **SkullSecondary-Server**

*/
void bluetooth_controller::initializeBLEServer()
{
    Serial.println("BT-BLE: Starting as BLE SECONDARY (server)");

    // Initialize BLE device, create server and service
    BLEDevice::init("SkullSecondary-Server");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVER_SERVICE_UUID);

    // Create characteristic with read, write, and indicate properties
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_INDICATE);

    pCharacteristic->setValue("Hello from SkullSecondary");
    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    // Add descriptor for indications
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    // Set up advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BT-BLE: Audio Playback characteristic defined. Ready for Primary skull (client) to command an audio file be played.");
}

// Callback class for BLE client events
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        Serial.println("BT-BLE: Client connected callback triggered");
        if (bluetooth_controller::instance)
        {
            bluetooth_controller::instance->setBLEClientConnectionStatus(true);
        }
    }

    void onDisconnect(BLEClient *pclient)
    {
        Serial.println("BT-BLE: Client disconnected callback triggered");
        if (bluetooth_controller::instance)
        {
            bluetooth_controller::instance->setBLEClientConnectionStatus(false);
        }
    }
};

// Main update function for the Bluetooth controller
void bluetooth_controller::update()
{
    if (m_isPrimary)
    {
        static unsigned long lastStatusUpdate = 0;
        unsigned long currentTime = millis();

        switch (m_connectionState)
        {
        case ConnectionState::DISCONNECTED:
            if (currentTime - m_lastReconnectAttempt > SCAN_INTERVAL)
            {
                m_lastReconnectAttempt = currentTime;
                startScan();
            }
            break;

        case ConnectionState::SCANNING:
            if (currentTime - scanStartTime > SCAN_TIMEOUT)
            {
                Serial.println("BT-BLE: Scan timed out. Restarting scan.");
                pBLEScan->stop();
                delay(100); // Give some time for the scan to stop
                startScan();
            }
            // Scanning is handled in the BLEAdvertisedDeviceCallbacks
            if (!isScanning && myDevice != nullptr)
            {
                m_connectionState = ConnectionState::CONNECTING;
                connectionStartTime = currentTime;
            }
            break;

        case ConnectionState::CONNECTING:
            if (currentTime - connectionStartTime > CONNECTION_TIMEOUT)
            {
                Serial.println("BT-BLE: Connection attempt timed out. Restarting scan immediately.");
                disconnectFromServer();
                m_connectionState = ConnectionState::DISCONNECTED;
                m_lastReconnectAttempt = currentTime; // Reset the last reconnect attempt time
                startScan();                          // Start a new scan immediately
            }
            else if (connectToServer())
            {
                m_connectionState = ConnectionState::CONNECTED;
                Serial.println("BT-BLE: Successfully connected to server");
            }
            else
            {
                Serial.println("BT-BLE: Connection attempt failed, but not timed out yet. Retrying...");
                delay(1000); // Wait a bit before retrying
            }
            break;

        case ConnectionState::CONNECTED:
            if (!m_clientIsConnectedToServer || (pClient && !pClient->isConnected()))
            {
                Serial.println("BT-BLE: Connection lost. Moving to DISCONNECTED state.");
                disconnectFromServer();
                m_connectionState = ConnectionState::DISCONNECTED;
            }
            break;
        }

        // Periodic status update
        if (currentTime - lastStatusUpdate > 30000)
        {
            Serial.printf("BT-BLE: Current connection state: %s\n",
                          getConnectionStateString(m_connectionState).c_str());
            Serial.printf("BT-BLE: Client connected: %s, Server has client: %s\n",
                          m_clientIsConnectedToServer ? "true" : "false",
                          m_serverHasClientConnected ? "true" : "false");
            lastStatusUpdate = currentTime;
        }
    }
}

// Check if the server is still advertising
bool bluetooth_controller::isServerAdvertising()
{
    if (myDevice == nullptr)
    {
        return false;
    }

    BLEScanResults scanResults = BLEDevice::getScan()->start(1);
    for (int i = 0; i < scanResults.getCount(); i++)
    {
        BLEAdvertisedDevice device = scanResults.getDevice(i);
        if (device.getAddress().equals(myDevice->getAddress()))
        {
            return true;
        }
    }
    return false;
}

// Callback class for handling advertised devices during scanning
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
public:
    MyAdvertisedDeviceCallbacks(bluetooth_controller *controller) : m_controller(controller) {}

    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(bluetooth_controller::getServerServiceUUID())))
        {
            Serial.print("BT-BLE: Found our server: ");
            Serial.println(advertisedDevice.toString().c_str());
            m_controller->setMyDevice(new BLEAdvertisedDevice(advertisedDevice));
            m_controller->setConnectionState(ConnectionState::CONNECTING);
            m_controller->setConnectionStartTime(millis()); // Reset the connection start time
            BLEDevice::getScan()->stop();
        }
    }

private:
    bluetooth_controller *m_controller;
};

// Start BLE scanning
void bluetooth_controller::startScan()
{
    if (isScanning)
    {
        Serial.println("BT-BLE: Already scanning, stopping current scan");
        pBLEScan->stop();
        delay(100); // Give some time for the scan to stop
    }

    Serial.println("BT-BLE: Starting scan...");
    m_connectionState = ConnectionState::SCANNING;
    scanStartTime = millis(); // Initialize scanStartTime here

    if (pBLEScan == nullptr)
    {
        pBLEScan = BLEDevice::getScan();
    }
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(this));
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);

    isScanning = true;
    bool scanStarted = pBLEScan->start(SCAN_DURATION, [](BLEScanResults results)
                                       {
        Serial.println("BT-BLE: Scan completed");
        isScanning = false; }, false);

    if (scanStarted)
    {
        Serial.println("BT-BLE: Scan started successfully");
    }
    else
    {
        Serial.println("BT-BLE: Failed to start scan");
        isScanning = false;
    }
}

// Connect to the BLE server
bool bluetooth_controller::connectToServer()
{
    if (myDevice == nullptr)
    {
        Serial.println("BT-BLE: No device to connect to.");
        return false;
    }

    Serial.print("BT-BLE: Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    Serial.println("BT-BLE: Created client");

    pClient->setClientCallbacks(new MyClientCallback());
    Serial.println("BT-BLE: Set client callbacks");

    // Connect to the remote BLE Server
    BLEAddress address = myDevice->getAddress();
    esp_ble_addr_type_t type = myDevice->getAddressType();
    Serial.println("BT-BLE: Attempting to connect...");
    if (pClient->connect(address, type))
    {
        Serial.println("BT-BLE: Connected to the server");
        pClient->setMTU(517); // Set MTU after connection

        // Discover service
        BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVER_SERVICE_UUID));
        if (pRemoteService == nullptr)
        {
            Serial.println("BT-BLE: Failed to find our service UUID");
            pClient->disconnect();
            return false;
        }

        // Discover characteristic
        pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
        if (pRemoteCharacteristic == nullptr)
        {
            Serial.println("BT-BLE: Failed to find our characteristic UUID");
            pClient->disconnect();
            return false;
        }

        if (pRemoteCharacteristic->canIndicate())
        {
            pRemoteCharacteristic->registerForNotify(notifyCallback);
            Serial.println("BT-BLE: Registered for notifications/indications");
        }

        m_connectionState = ConnectionState::CONNECTED;
        m_clientIsConnectedToServer = true;
        return true;
    }

    Serial.println("BT-BLE: Failed to connect to the server");
    return false;
}

// Disconnect from the BLE server
void bluetooth_controller::disconnectFromServer()
{
    if (pClient != nullptr)
    {
        if (pClient->isConnected())
        {
            pClient->disconnect();
        }
        delete pClient;
        pClient = nullptr;
    }
    m_clientIsConnectedToServer = false;
    m_connectionState = ConnectionState::DISCONNECTED;
    Serial.println("BT-BLE: Disconnected from server");
}

// Register for indications from the remote characteristic
bool bluetooth_controller::registerForIndications()
{
    if (pRemoteCharacteristic->canIndicate())
    {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
        Serial.println("BT-BLE: Registered for indications");
        return true;
    }
    else
    {
        Serial.println("BT-BLE: Characteristic does not support indications");
        return false;
    }
}

// Static callback function for handling notifications/indications
void bluetooth_controller::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    if (instance)
    {
        std::string value((char *)pData, length);
        instance->handleIndication(value);
    }
}

// Handle received indications
void bluetooth_controller::handleIndication(const std::string &value)
{
    Serial.print("BT-BLE: Received indication: ");
    Serial.println(value.c_str());
    indicationReceived = true;
}

// Set the value of the BLE characteristic
void bluetooth_controller::setCharacteristicValue(const char *value)
{
    pCharacteristic->setValue(value);
}

// Primary (client) only: Set the value of the remote characteristic
bool bluetooth_controller::setRemoteCharacteristicValue(const std::string &value)
{
    if (m_clientIsConnectedToServer && pRemoteCharacteristic)
    {
        indicationReceived = false;
        pRemoteCharacteristic->writeValue(value);
        delay(100); // Wait for the server to process the write

        // Wait for indication (with timeout)
        unsigned long startTime = millis();
        while (!indicationReceived && (millis() - startTime < 5000))
        {
            delay(10);
        }

        if (indicationReceived)
        {
            // Check if the final value matches what we set
            std::string finalValue = getRemoteCharacteristicValue();
            if (finalValue == value)
            {
                Serial.println("BT-BLE: Successfully set characteristic value and received indication");
                return true;
            }
            else
            {
                Serial.print("BT-BLE: Characteristic value mismatch. Expected: ");
                Serial.print(value.c_str());
                Serial.print(", Actual: ");
                Serial.println(finalValue.c_str());
                return false;
            }
        }
        else
        {
            Serial.println("BT-BLE: Failed to receive indication after setting characteristic value");
            return false;
        }
    }
    else
    {
        Serial.println("BT-BLE: Not connected or characteristic not available");
        return false;
    }
}

// Set the BLE client connection status
void bluetooth_controller::setBLEClientConnectionStatus(bool status)
{
    m_clientIsConnectedToServer = status;
    setConnectionState(status ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED);
    Serial.printf("BT-BLE: Client connection status changed to %s\n", status ? "connected" : "disconnected");
}

// Set the BLE server connection status
void bluetooth_controller::setBLEServerConnectionStatus(bool status)
{
    m_serverHasClientConnected = status;
    setConnectionState(status ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED);
    Serial.printf("BT-BLE: Server connection status changed to %s\n", status ? "connected" : "disconnected");
}

// Get the speaker name
const String &bluetooth_controller::get_speaker_name() const
{
    return m_speaker_name;
}

// Handle A2DP connection state changes
void bluetooth_controller::connection_state_changed(esp_a2d_connection_state_t state, void *ptr)
{
    bluetooth_controller *self = static_cast<bluetooth_controller *>(ptr);

    switch (state)
    {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
        Serial.printf("BT-A2DP: Not connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
        Serial.printf("BT-A2DP: Attempting to connect to Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
        Serial.printf("BT-A2DP: Successfully connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
        Serial.printf("BT-A2DP: Disconnecting from Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    default:
        Serial.printf("BT-A2DP: Unknown connection state for Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
    }
}

// Set the volume for the A2DP connection
void bluetooth_controller::set_volume(uint8_t volume)
{
    Serial.printf("BT-A2DP: Setting bluetooth speaker volume to %d\n", volume);
    a2dp_source.set_volume(volume);
}

// Check if A2DP is connected
bool bluetooth_controller::isA2dpConnected()
{
    return a2dp_source.is_connected();
}

// Check if the BLE client is connected to the server
bool bluetooth_controller::clientIsConnectedToServer() const
{
    return m_clientIsConnectedToServer;
}

// Check if the BLE server has a client connected
bool bluetooth_controller::serverHasClientConnected() const
{
    return m_serverHasClientConnected;
}

// Check if a client-server connection exists
bool bluetooth_controller::isBleConnected() const
{
    return m_serverHasClientConnected || m_serverHasClientConnected;
}

// Static trampoline function for audio callback
int bluetooth_controller::audio_callback_trampoline(Frame *frame, int frame_count)
{
    if (instance && instance->audio_provider_callback)
    {
        return instance->audio_provider_callback(frame, frame_count);
    }
    return 0;
}

// Helper method to get a string representation of the connection state
std::string bluetooth_controller::getConnectionStateString(ConnectionState state)
{
    switch (state)
    {
    case ConnectionState::DISCONNECTED:
        return "DISCONNECTED";
    case ConnectionState::SCANNING:
        return "SCANNING";
    case ConnectionState::CONNECTING:
        return "CONNECTING";
    case ConnectionState::CONNECTED:
        return "CONNECTED";
    default:
        return "UNKNOWN";
    }
}

bool bluetooth_controller::isFullyInitialized() const
{
    return m_a2dpInitialized && m_bleInitialized;
}

void bluetooth_controller::setCharacteristicChangeRequestCallback(std::function<bool(const std::string &)> callback)
{
    m_characteristicChangeRequestCallback = callback;
}

std::string bluetooth_controller::getRemoteCharacteristicValue()
{
    if (pRemoteCharacteristic)
    {
        return pRemoteCharacteristic->readValue();
    }
    return "";
}