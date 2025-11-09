#include "remote_debug_manager.h"
#include "ota_manager.h"
#include "bluetooth_controller.h"
#include "esp_system.h"
#include "infra/log_sink.h"

static constexpr const char* TAG = "RemoteDebug";

RemoteDebugManager::RemoteDebugManager()
    : m_server(nullptr),
      m_enabled(false),
      m_port(23),
      m_autoStreaming(false),
      m_lastBroadcastSequence(0) {}

bool RemoteDebugManager::begin(int port) {
    m_port = port;

    if (WiFi.status() != WL_CONNECTED) {
        infra::emitLog(infra::LogLevel::Warn, TAG, "Cannot start telnet server (WiFi disconnected)");
        return false;
    }

    if (!m_server) {
        m_server = new WiFiServer(m_port);
    }

    m_server->begin();
    m_enabled = true;
    m_lastBroadcastSequence = LoggingManager::instance().latestSequence();

    infra::emitLog(infra::LogLevel::Info, TAG,
                   "Telnet server started on %d (connect with: telnet %s %d)",
                   m_port, WiFi.localIP().toString().c_str(), m_port);

    return true;
}

void RemoteDebugManager::update() {
    if (!m_enabled || !m_server) {
        return;
    }

    handleClient();

    if (m_autoStreaming && hasClient()) {
        streamNewEntries();
    }
}

bool RemoteDebugManager::isEnabled() const {
    return m_enabled;
}

bool RemoteDebugManager::hasClient() {
    return m_client && m_client.connected() > 0;
}

void RemoteDebugManager::print(const String& message) {
    if (hasClient()) {
        sendToClient(message);
    }
}

void RemoteDebugManager::println(const String& message) {
    if (hasClient()) {
        sendToClient(message + "\n");
    }
}

void RemoteDebugManager::printf(const char* format, ...) {
    if (hasClient()) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        sendToClient(String(buffer));
    }
}

void RemoteDebugManager::setConnectionCallback(std::function<void()> callback) {
    m_connectionCallback = callback;
}

void RemoteDebugManager::setDisconnectionCallback(std::function<void()> callback) {
    m_disconnectionCallback = callback;
}

void RemoteDebugManager::setAutoStreaming(bool enabled) {
    m_autoStreaming = enabled;
}

bool RemoteDebugManager::isAutoStreaming() const {
    return m_autoStreaming;
}

void RemoteDebugManager::setBluetoothController(BluetoothController* controller) {
    m_bluetooth = controller;
}

void RemoteDebugManager::handleClient() {
    if (!m_client || !m_client.connected()) {
        WiFiClient pending = m_server->available();
        if (pending) {
            infra::emitLog(infra::LogLevel::Debug, TAG,
                           "Telnet pending from %s (connected=%d, available=%d)",
                           pending.remoteIP().toString().c_str(),
                           pending.connected(), pending.available());
        }

        if (m_client) {
            m_client.stop();
            if (m_disconnectionCallback) {
                m_disconnectionCallback();
            }
        }

        m_client = pending;
        if (m_client) {
            infra::emitLog(infra::LogLevel::Info, TAG, "Telnet client connected from %s",
                           m_client.remoteIP().toString().c_str());
            m_lastBroadcastSequence = LoggingManager::instance().latestSequence();

            if (m_connectionCallback) {
                m_connectionCallback();
            }

            m_client.println("🛜 RemoteDebug connected");
            m_client.println("Commands: status, wifi, ota, log, startup, head N, tail N, stream on|off, bluetooth on|off, reboot, help");
            m_client.println("🛜 Hint: run 'startup' to replay boot log; use 'log' for rolling buffer.");
        }
    } else {
        if (m_client.available()) {
            String command = m_client.readStringUntil('\n');
            command.trim();
            processCommand(command);
        }
    }
}

void RemoteDebugManager::processCommand(const String& command) {
    if (command.equalsIgnoreCase("status")) {
        size_t total = LoggingManager::instance().entryCount();
        size_t capacity = LoggingManager::instance().bufferCapacity();
        size_t startup = LoggingManager::instance().startupCount();
        OTAManager* ota = OTAManager::instance();
        const char* otaStatus = "disabled";
        if (ota && ota->isEnabled()) {
            otaStatus = ota->isPasswordProtected() ? "protected" : "enabled";
        } else if (ota && ota->disabledForMissingPassword()) {
            otaStatus = "disabled (password missing)";
        }

        m_client.printf("🛜 Status: WiFi=%s, OTA=%s, Stream=%s\n",
                        WiFi.isConnected() ? "connected" : "disconnected",
                        otaStatus,
                        m_autoStreaming ? "on" : "off");
        m_client.printf("🛜 Log buffer: %u/%u entries, Startup log: %u lines\n",
                        static_cast<unsigned>(total),
                        static_cast<unsigned>(capacity),
                        static_cast<unsigned>(startup));
    } else if (command.equalsIgnoreCase("wifi")) {
        m_client.printf("🛜 WiFi: %s (%s)\n",
                        WiFi.isConnected() ? "connected" : "disconnected",
                        WiFi.localIP().toString().c_str());
    } else if (command.equalsIgnoreCase("ota")) {
        OTAManager* ota = OTAManager::instance();
        if (ota && ota->isEnabled()) {
            m_client.println("🛜 OTA: Ready for updates on port 3232 (password protected)");
        } else if (ota && ota->disabledForMissingPassword()) {
            m_client.println("🛜 OTA: Disabled — configure ota_password in config.txt");
        } else {
            m_client.println("🛜 OTA: Disabled");
        }
    } else if (command.equalsIgnoreCase("log")) {
        sendRollingLog();
    } else if (command.equalsIgnoreCase("startup")) {
        sendStartupLog();
    } else if (command.startsWith("head")) {
        int lines = command.substring(4).toInt();
        if (lines <= 0) lines = 10;
        std::vector<LogEntry> entries;
        uint32_t latest = LoggingManager::instance().latestSequence();
        uint32_t startSeq = (lines >= static_cast<int>(latest)) ? 0 : (latest - static_cast<uint32_t>(lines));
        LoggingManager::instance().getEntriesSince(startSeq, entries);
        if (entries.size() > static_cast<size_t>(lines)) {
            entries.erase(entries.begin(), entries.end() - lines);
        }
        m_client.printf("🛜 Last %d lines:\n", lines);
        for (const auto& entry : entries) {
            m_client.printf("[%lu ms] %s\n", static_cast<unsigned long>(entry.timestamp), entry.message.c_str());
        }
    } else if (command.startsWith("tail")) {
        int lines = command.substring(4).toInt();
        if (lines <= 0) lines = 10;
        std::vector<LogEntry> entries;
        LoggingManager::instance().getStartupEntries(entries);
        m_client.printf("🛜 First %d lines:\n", lines);
        for (size_t i = 0; i < entries.size() && i < static_cast<size_t>(lines); ++i) {
            const auto& entry = entries[i];
            m_client.printf("[%lu ms] %s\n", static_cast<unsigned long>(entry.timestamp), entry.message.c_str());
        }
    } else if (command.startsWith("stream")) {
        if (command.endsWith("on")) {
            m_autoStreaming = true;
            m_client.println("🛜 Streaming enabled");
        } else if (command.endsWith("off")) {
            m_autoStreaming = false;
            m_client.println("🛜 Streaming disabled");
        } else {
            m_client.println("🛜 Usage: stream on|off");
        }
    } else if (command.equalsIgnoreCase("bluetooth") || command.startsWith("bluetooth ")) {
        if (!m_bluetooth) {
            m_client.println("🛜 Bluetooth controller unavailable");
            return;
        }
        String arg = command.substring(String("bluetooth").length());
        arg.trim();
        if (arg.length() == 0 || arg.equalsIgnoreCase("status")) {
            m_client.printf("🛜 Bluetooth: %s, Connection: %s\n",
                            m_bluetooth->isManuallyDisabled() ? "disabled" : "enabled",
                            m_bluetooth->isA2dpConnected() ? "connected" : "disconnected");
        } else if (arg.equalsIgnoreCase("off") || arg.equalsIgnoreCase("disable")) {
            bool changed = m_bluetooth->manualDisable();
            m_client.println(changed ? "🛜 Bluetooth manually disabled" : "🛜 Bluetooth already disabled or unavailable");
        } else if (arg.equalsIgnoreCase("on") || arg.equalsIgnoreCase("enable")) {
            bool changed = m_bluetooth->manualEnable();
            m_client.println(changed ? "🛜 Bluetooth enabled" : "🛜 Bluetooth already enabled or unavailable");
        } else {
            m_client.println("🛜 Usage: bluetooth [status|on|off]");
        }
    } else if (command.equalsIgnoreCase("reboot") || command.equalsIgnoreCase("restart")) {
        m_client.println("🛜 Rebooting in 1 second…");
        m_client.flush();
        delay(1000);
        esp_restart();
    } else if (command.equalsIgnoreCase("help")) {
        m_client.println("🛜 Available commands:");
        m_client.println("  status        - Show system status");
        m_client.println("  wifi          - Show Wi-Fi information");
        m_client.println("  ota           - Show OTA status");
        m_client.println("  log           - Dump rolling log buffer");
        m_client.println("  startup       - Dump startup log buffer");
        m_client.println("  head [N]      - Show last N log entries");
        m_client.println("  tail [N]      - Show first N startup log entries");
        m_client.println("  stream on|off - Toggle live streaming");
        m_client.println("  bluetooth [on|off|status] - Manage Bluetooth controller");
        m_client.println("  reboot        - Restart the skull safely");
        m_client.println("  help          - Show this help");
    } else {
        m_client.println("🛜 Unknown command. Type 'help' for available commands.");
    }
}

void RemoteDebugManager::sendToClient(const String& message) {
    if (m_client && m_client.connected()) {
        m_client.print(message);
    }
}

void RemoteDebugManager::streamNewEntries() {
    std::vector<LogEntry> entries;
    LoggingManager::instance().getEntriesSince(m_lastBroadcastSequence, entries);
    if (entries.empty()) {
        return;
    }

    for (const auto& entry : entries) {
        m_client.printf("[%lu ms] %s\n", static_cast<unsigned long>(entry.timestamp), entry.message.c_str());
        m_lastBroadcastSequence = entry.sequence;
    }
}

void RemoteDebugManager::sendStartupLog() {
    std::vector<LogEntry> entries;
    LoggingManager::instance().getStartupEntries(entries);
    if (entries.empty()) {
        m_client.println("🛜 Startup log empty");
        return;
    }

    m_client.printf("🛜 Startup log (%u lines):\n", static_cast<unsigned>(entries.size()));
    for (const auto& entry : entries) {
        m_client.printf("[%lu ms] %s\n", static_cast<unsigned long>(entry.timestamp), entry.message.c_str());
    }
}

void RemoteDebugManager::sendRollingLog() {
    std::vector<LogEntry> entries;
    LoggingManager::instance().getEntriesSince(0, entries);
    if (entries.empty()) {
        m_client.println("🛜 Rolling log empty");
        return;
    }

    m_client.printf("🛜 Rolling log (%u entries):\n", static_cast<unsigned>(entries.size()));
    for (const auto& entry : entries) {
        m_client.printf("[%lu ms] %s\n", static_cast<unsigned long>(entry.timestamp), entry.message.c_str());
        m_lastBroadcastSequence = entry.sequence;
    }
}
