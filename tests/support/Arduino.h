#pragma once

#include <cstdint>
#include "shim_string.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;

inline unsigned long millis() {
    return 0;
}

inline void delay(unsigned long) {}

#ifndef FILE_READ
#define FILE_READ "r"
#endif

template <typename T>
inline T constrain(T value, T low, T high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}
