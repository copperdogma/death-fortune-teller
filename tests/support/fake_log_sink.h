#pragma once

#include "infra/log_sink.h"
#include <vector>
#include <string>
#include <cstdarg>
#include <cstdio>

struct CapturedLog {
    infra::LogLevel level;
    std::string tag;
    std::string message;
};

class FakeLogSink : public infra::ILogSink {
public:
    void log(infra::LogLevel level, const char *tag, const char *message) override {
        CapturedLog entry;
        entry.level = level;
        entry.tag = tag ? tag : "";
        entry.message = message ? message : "";
        entries.push_back(entry);
    }

    void clear() { entries.clear(); }

    std::vector<CapturedLog> entries;
};
