#include "logging_manager.h"

#ifdef ARDUINO

#include <ESPLogger.h>

static constexpr const char* TAG = "LoggingManager";

LoggingManager& LoggingManager::instance() {
    static LoggingManager s_instance;
    return s_instance;
}

LoggingManager::LoggingManager()
    : m_serial(nullptr),
      m_sdLogger(nullptr),
      m_serialForwardingEnabled(true),
      m_initialized(false),
      m_capacity(0),
      m_startupCapacity(0),
      m_count(0),
      m_head(0),
      m_sequence(0),
      m_bufferMutex(portMUX_INITIALIZER_UNLOCKED) {}

void LoggingManager::begin(HardwareSerial* serial, size_t bufferCapacity, size_t startupCapacity) {
    m_serial = serial;
    m_capacity = bufferCapacity;
    m_startupCapacity = startupCapacity;
    m_entries.clear();
    m_entries.resize(bufferCapacity);
    m_startupEntries.clear();
    m_startupEntries.reserve(startupCapacity);
    m_count = 0;
    m_head = 0;
    m_sequence = 0;
    m_initialized = true;

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_set_vprintf(&LoggingManager::vprintfHook);

    LOG_INFO(TAG, "LoggingManager initialized (capacity=%u, startup=%u)", static_cast<unsigned>(bufferCapacity),
             static_cast<unsigned>(startupCapacity));
}

void LoggingManager::setSdLogger(ESPLogger* logger) {
    portENTER_CRITICAL(&m_bufferMutex);
    m_sdLogger = logger;
    portEXIT_CRITICAL(&m_bufferMutex);
}

void LoggingManager::enableSerialForwarding(bool enabled) {
    m_serialForwardingEnabled = enabled;
}

void LoggingManager::registerListener(const std::function<void(const LogEntry&)>& listener) {
    portENTER_CRITICAL(&m_bufferMutex);
    m_listeners.push_back(listener);
    portEXIT_CRITICAL(&m_bufferMutex);
}

void LoggingManager::getEntriesSince(uint32_t lastSequence, std::vector<LogEntry>& out) const {
    portENTER_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    if (m_count == 0) {
        portEXIT_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
        return;
    }

    size_t startIndex = (m_head + m_capacity - m_count) % m_capacity;
    for (size_t i = 0; i < m_count; ++i) {
        const LogEntry& entry = m_entries[(startIndex + i) % m_capacity];
        if (entry.sequence > lastSequence && entry.sequence != 0) {
            out.push_back(entry);
        }
    }

    portEXIT_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
}

void LoggingManager::getStartupEntries(std::vector<LogEntry>& out) const {
    portENTER_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    out.insert(out.end(), m_startupEntries.begin(), m_startupEntries.end());
    portEXIT_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
}

uint32_t LoggingManager::latestSequence() const {
    portENTER_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    uint32_t seq = m_sequence;
    portEXIT_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    return seq;
}

size_t LoggingManager::entryCount() const {
    portENTER_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    size_t count = m_count;
    portEXIT_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    return count;
}

size_t LoggingManager::bufferCapacity() const {
    return m_capacity;
}

size_t LoggingManager::startupCount() const {
    portENTER_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    size_t count = m_startupEntries.size();
    portEXIT_CRITICAL(&const_cast<LoggingManager*>(this)->m_bufferMutex);
    return count;
}

void LoggingManager::log(LogLevel level, const char* tag, const char* fmt, ...) {
    if (!m_initialized) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int needed = vsnprintf(nullptr, 0, fmt, argsCopy);
    va_end(argsCopy);

    if (needed <= 0) {
        va_end(args);
        return;
    }

    std::vector<char> buffer(static_cast<size_t>(needed) + 1);
    vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);

    String message = String(levelToPrefix(level)) + "/" + tag + ": " + String(buffer.data());
    message.replace("\r", "");

    if (m_serialForwardingEnabled && m_serial) {
        m_serial->print(message);
        if (!message.endsWith("\n")) {
            m_serial->print("\n");
        }
    }

    pushLine(level, message);
}

int LoggingManager::vprintfHook(const char* fmt, va_list args) {
    return LoggingManager::instance().handleVprintf(fmt, args);
}

int LoggingManager::handleVprintf(const char* fmt, va_list args) {
    if (!m_initialized) {
        // If begin() not called yet, fall back to default Serial output
        char fallback[256];
        int len = vsnprintf(fallback, sizeof(fallback), fmt, args);
        if (len > 0 && m_serial) {
            m_serial->write(reinterpret_cast<const uint8_t*>(fallback), len);
        }
        return len;
    }

    va_list argsCopy;
    va_copy(argsCopy, args);
    int needed = vsnprintf(nullptr, 0, fmt, argsCopy);
    va_end(argsCopy);

    if (needed <= 0) {
        return needed;
    }

    std::vector<char> buffer(static_cast<size_t>(needed) + 1);
    vsnprintf(buffer.data(), buffer.size(), fmt, args);

    String line = String(buffer.data());
    LogLevel level = inferLevelFromLine(line);

    if (m_serialForwardingEnabled && m_serial) {
        m_serial->write(reinterpret_cast<const uint8_t*>(buffer.data()), needed);
    }

    // Remove trailing newline characters
    while (line.length() > 0 && (line.endsWith("\n") || line.endsWith("\r"))) {
        line.remove(line.length() - 1);
    }

    if (line.length() > 0) {
        pushLine(level, line);
    }

    return needed;
}

void LoggingManager::pushLine(LogLevel level, const String& line) {
    LogEntry entry;
    entry.timestamp = millis();
    entry.level = level;
    entry.message = line;

    portENTER_CRITICAL(&m_bufferMutex);
    entry.sequence = ++m_sequence;

    if (m_capacity == 0) {
        portEXIT_CRITICAL(&m_bufferMutex);
        return;
    }

    m_entries[m_head] = entry;
    m_head = (m_head + 1) % m_capacity;
    if (m_count < m_capacity) {
        ++m_count;
    }

    if (m_startupEntries.size() < m_startupCapacity) {
        pushStartup(entry);
    }

    if (m_sdLogger) {
        m_sdLogger->append(line.c_str(), true);
    }

    auto listeners = m_listeners;
    portEXIT_CRITICAL(&m_bufferMutex);

    for (auto& listener : listeners) {
        if (listener) {
            listener(entry);
        }
    }
}

void LoggingManager::pushStartup(const LogEntry& entry) {
    m_startupEntries.push_back(entry);
}

LogLevel LoggingManager::inferLevelFromLine(const String& line) {
    if (line.length() > 0) {
        char levelChar = line.charAt(0);
        if (levelChar == 'E') return LogLevel::Error;
        if (levelChar == 'W') return LogLevel::Warn;
        if (levelChar == 'I') return LogLevel::Info;
        if (levelChar == 'D') return LogLevel::Debug;
        if (levelChar == 'V') return LogLevel::Verbose;
    }
    return LogLevel::Info;
}

const char* LoggingManager::levelToPrefix(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose: return "V";
        case LogLevel::Debug:   return "D";
        case LogLevel::Info:    return "I";
        case LogLevel::Warn:    return "W";
        case LogLevel::Error:   return "E";
        default:                return "?";
    }
}

esp_log_level_t LoggingManager::toEspLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose: return ESP_LOG_VERBOSE;
        case LogLevel::Debug:   return ESP_LOG_DEBUG;
        case LogLevel::Info:    return ESP_LOG_INFO;
        case LogLevel::Warn:    return ESP_LOG_WARN;
        case LogLevel::Error:   return ESP_LOG_ERROR;
        default:                return ESP_LOG_INFO;
    }
}

#endif  // ARDUINO
