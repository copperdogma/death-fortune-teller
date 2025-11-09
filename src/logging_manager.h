#ifndef LOGGING_MANAGER_H
#define LOGGING_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <vector>

#ifdef ARDUINO
#include <esp_log.h>
#else
#include <cstdarg>
using esp_log_level_t = int;
#endif

#include "infra/log_sink.h"

class ESPLogger;

enum class LogLevel : uint8_t {
    Verbose = 0,
    Debug,
    Info,
    Warn,
    Error
};

#ifdef ARDUINO

struct LogEntry {
    uint32_t timestamp;
    LogLevel level;
    String message;
    uint32_t sequence;
};

class LoggingManager {
public:
    static LoggingManager& instance();

    void begin(HardwareSerial* serial, size_t bufferCapacity = 2000, size_t startupCapacity = 500);
    void setSdLogger(ESPLogger* logger);
    void enableSerialForwarding(bool enabled);

    void registerListener(const std::function<void(const LogEntry&)>& listener);

    void getEntriesSince(uint32_t lastSequence, std::vector<LogEntry>& out) const;
    void getStartupEntries(std::vector<LogEntry>& out) const;
    uint32_t latestSequence() const;
    size_t entryCount() const;
    size_t bufferCapacity() const;
    size_t startupCount() const;

    void log(LogLevel level, const char* tag, const char* fmt, ...);

    static int vprintfHook(const char* fmt, va_list args);

private:
    LoggingManager();

    int handleVprintf(const char* fmt, va_list args);
    void pushLine(LogLevel level, const String& line);
    void pushStartup(const LogEntry& entry);
    static LogLevel inferLevelFromLine(const String& line);
    static const char* levelToPrefix(LogLevel level);
    static esp_log_level_t toEspLevel(LogLevel level);

    HardwareSerial* m_serial;
    ESPLogger* m_sdLogger;
    bool m_serialForwardingEnabled;
    bool m_initialized;

    size_t m_capacity;
    size_t m_startupCapacity;
    size_t m_count;
    size_t m_head;
    uint32_t m_sequence;

    std::vector<LogEntry> m_entries;
    std::vector<LogEntry> m_startupEntries;
    std::vector<std::function<void(const LogEntry&)>> m_listeners;

    portMUX_TYPE m_bufferMutex;
};

#else  // !ARDUINO

struct LogEntry {
    uint32_t timestamp = 0;
    LogLevel level = LogLevel::Info;
    String message;
    uint32_t sequence = 0;
};

class LoggingManager {
public:
    static LoggingManager& instance() {
        static LoggingManager s_instance;
        return s_instance;
    }

    void begin(HardwareSerial*, size_t = 0, size_t = 0) {}
    void setSdLogger(ESPLogger*) {}
    void enableSerialForwarding(bool) {}

    void registerListener(const std::function<void(const LogEntry&)>&) {}

    void getEntriesSince(uint32_t, std::vector<LogEntry>&) const {}
    void getStartupEntries(std::vector<LogEntry>&) const {}
    uint32_t latestSequence() const { return 0; }
    size_t entryCount() const { return 0; }
    size_t bufferCapacity() const { return 0; }
    size_t startupCount() const { return 0; }

    void log(LogLevel, const char*, const char*, ...) {}

    static int vprintfHook(const char*, va_list) { return 0; }

private:
    LoggingManager() = default;
};

#endif  // ARDUINO

#define LOG_VERBOSE(tag, fmt, ...) do { infra::emitLog(infra::LogLevel::Verbose, tag, fmt, ##__VA_ARGS__); } while (0)
#define LOG_DEBUG(tag, fmt, ...)   do { infra::emitLog(infra::LogLevel::Debug, tag, fmt, ##__VA_ARGS__); } while (0)
#define LOG_INFO(tag, fmt, ...)    do { infra::emitLog(infra::LogLevel::Info, tag, fmt, ##__VA_ARGS__); } while (0)
#define LOG_WARN(tag, fmt, ...)    do { infra::emitLog(infra::LogLevel::Warn, tag, fmt, ##__VA_ARGS__); } while (0)
#define LOG_ERROR(tag, fmt, ...)   do { infra::emitLog(infra::LogLevel::Error, tag, fmt, ##__VA_ARGS__); } while (0)

#endif // LOGGING_MANAGER_H
