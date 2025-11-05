#include "log_sink.h"
#include <cstdarg>
#include <cstdio>
#ifndef UNIT_TEST
#include "logging_manager.h"
#endif

namespace infra {

static ILogSink *g_sink = nullptr;

void setLogSink(ILogSink *sink) {
    g_sink = sink;
}

ILogSink *getLogSink() {
    return g_sink;
}

void emitLog(LogLevel level, const char *tag, const char *fmt, ...) {
    if (!fmt) {
        return;
    }
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (g_sink) {
        g_sink->log(level, tag ? tag : "", buffer);
        return;
    }
#ifndef UNIT_TEST
    LoggingManager::instance().log(static_cast<::LogLevel>(level), tag ? tag : "", "%s", buffer);
#endif
}

} // namespace infra
