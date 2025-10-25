#include "wifi_manager.h"
#include "logging_manager.h"

static constexpr const char* TAG = "WiFiManager";

WiFiManager::WiFiManager() 
    : m_connected(false)
    , m_lastConnectionAttempt(0)
    , m_connectionRetryInterval(10000) // 10 seconds
    , m_connectionAttempts(0)
    , m_maxConnectionAttempts(10)
{
}

bool WiFiManager::begin(const String& ssid, const String& password) {
    if (ssid.length() == 0) {
        LOG_WARN(TAG, "No SSID provided, Wi-Fi disabled");
        return false;
    }
    
    m_ssid = ssid;
    m_password = password;
    m_hostname = "death-fortune-teller";
    
    LOG_INFO(TAG, "Starting connection to '%s'", ssid.c_str());
    
    // Set hostname before connecting
    WiFi.setHostname(m_hostname.c_str());
    
    // Start connection attempt
    attemptConnection();
    
    return true;
}

void WiFiManager::update() {
    if (m_ssid.length() == 0) {
        return; // WiFi disabled
    }
    
    unsigned long currentTime = millis();
    
    if (!m_connected) {
        // Check if we should retry connection
        if (currentTime - m_lastConnectionAttempt >= m_connectionRetryInterval) {
            if (m_connectionAttempts < m_maxConnectionAttempts) {
                attemptConnection();
            } else {
                LOG_WARN(TAG, "Max connection attempts reached, giving up");
                m_connectionAttempts = 0; // Reset for next begin() call
            }
        }
        
        // Check if connection was successful
        if (WiFi.status() == WL_CONNECTED && !m_connected) {
            handleConnection();
        }
    } else {
        // Check if we lost connection
        if (WiFi.status() != WL_CONNECTED) {
            handleDisconnection();
        }
    }
}

bool WiFiManager::isConnected() const {
    return m_connected;
}

String WiFiManager::getIPAddress() const {
    if (m_connected) {
        return WiFi.localIP().toString();
    }
    return "";
}

String WiFiManager::getHostname() const {
    return m_hostname;
}

void WiFiManager::setHostname(const String& hostname) {
    m_hostname = hostname;
    if (m_connected) {
        WiFi.setHostname(hostname.c_str());
    }
}

void WiFiManager::setConnectionCallback(std::function<void(bool)> callback) {
    m_connectionCallback = callback;
}

void WiFiManager::setDisconnectionCallback(std::function<void()> callback) {
    m_disconnectionCallback = callback;
}

void WiFiManager::attemptConnection() {
    m_lastConnectionAttempt = millis();
    m_connectionAttempts++;
    
    LOG_DEBUG(TAG, "Connection attempt %d/%d", m_connectionAttempts, m_maxConnectionAttempts);
    
    WiFi.begin(m_ssid.c_str(), m_password.c_str());
}

void WiFiManager::handleConnection() {
    m_connected = true;
    m_connectionAttempts = 0;

    // Ensure optimal Wi-Fi performance once the driver is fully active
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    
    LOG_INFO(TAG, "Connected successfully");
    LOG_INFO(TAG, "IP address: %s", WiFi.localIP().toString().c_str());
    LOG_INFO(TAG, "Hostname: %s", WiFi.getHostname());
    LOG_INFO(TAG, "RSSI: %ddBm", WiFi.RSSI());
    
    if (m_connectionCallback) {
        m_connectionCallback(true);
    }
}

void WiFiManager::handleDisconnection() {
    m_connected = false;
    
    LOG_WARN(TAG, "Connection lost");
    
    if (m_disconnectionCallback) {
        m_disconnectionCallback();
    }
}
