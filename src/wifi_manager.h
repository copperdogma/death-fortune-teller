#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
    WiFiManager();
    
    bool begin(const String& ssid, const String& password);
    void update();
    bool isConnected() const;
    String getIPAddress() const;
    String getHostname() const;
    void setHostname(const String& hostname);
    
    // Connection status callbacks
    void setConnectionCallback(std::function<void(bool)> callback);
    void setDisconnectionCallback(std::function<void()> callback);

private:
    String m_ssid;
    String m_password;
    String m_hostname;
    bool m_connected;
    unsigned long m_lastConnectionAttempt;
    unsigned long m_connectionRetryInterval;
    int m_connectionAttempts;
    int m_maxConnectionAttempts;
    
    std::function<void(bool)> m_connectionCallback;
    std::function<void()> m_disconnectionCallback;
    
    void attemptConnection();
    void handleConnection();
    void handleDisconnection();
};

#endif // WIFI_MANAGER_H
