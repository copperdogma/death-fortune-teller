#ifndef REMOTE_DEBUG_MANAGER_H
#define REMOTE_DEBUG_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "logging_manager.h"

class BluetoothController;

class RemoteDebugManager {
public:
    RemoteDebugManager();
    
    bool begin(int port = 23);
    void update();
    bool isEnabled() const;
    bool hasClient();
    
    // Send debug messages
    void print(const String& message);
    void println(const String& message);
    void printf(const char* format, ...);
    
    // Connection callbacks
    void setConnectionCallback(std::function<void()> callback);
    void setDisconnectionCallback(std::function<void()> callback);
    void setAutoStreaming(bool enabled);
    bool isAutoStreaming() const;
    void setBluetoothController(BluetoothController* controller);
    
private:
    WiFiServer* m_server;
    WiFiClient m_client;
    bool m_enabled;
    int m_port;
    bool m_autoStreaming;
    uint32_t m_lastBroadcastSequence;
    BluetoothController* m_bluetooth;
    
    std::function<void()> m_connectionCallback;
    std::function<void()> m_disconnectionCallback;
    
    void handleClient();
    void sendToClient(const String& message);
    void processCommand(const String& command);
    void streamNewEntries();
    void sendStartupLog();
    void sendRollingLog();
};

#endif // REMOTE_DEBUG_MANAGER_H
