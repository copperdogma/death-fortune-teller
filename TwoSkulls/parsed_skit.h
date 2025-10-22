#pragma once

#include <Arduino.h>
#include <vector>

struct ParsedSkitLine {
    size_t lineNumber;
    char speaker;
    unsigned long timestamp;
    unsigned long duration;
    float jawPosition;
};

struct ParsedSkit {
    String audioFile;
    String txtFile;
    std::vector<ParsedSkitLine> lines;
};