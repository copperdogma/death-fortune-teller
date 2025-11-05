#ifndef INFRA_LOG_SINK_H
#define INFRA_LOG_SINK_H

#include <cstdarg>

namespace infra {

enum class LogLevel : unsigned char {
    Verbose = 0,
    Debug,
    Info,
    Warn,
    Error
};

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void log(LogLevel level, const char *tag, const char *message) = 0;
};

void setLogSink(ILogSink *sink);
ILogSink *getLogSink();
void emitLog(LogLevel level, const char *tag, const char *fmt, ...);

} // namespace infra

#endif // INFRA_LOG_SINK_H
