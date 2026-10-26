#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace skull::secondary::protocol {
constexpr size_t max_path = 240;
struct Assembly {
    uint8_t data[max_path]{};
    bool covered[max_path]{};
    size_t length{};
    bool active{}, invalid{};
    void reset() { *this = {}; }
    bool append(size_t offset, const uint8_t *value, size_t size) {
        active = true;
        if (!size || !value || offset > max_path || size > max_path - offset) { invalid = true; return false; }
        for (size_t i = 0; i < size; ++i)
            if (!value[i] || (covered[offset+i] && data[offset+i] != value[i])) { invalid = true; return false; }
        if (invalid) return false;
        for (size_t i = 0; i < size; ++i) { data[offset+i] = value[i]; covered[offset+i] = true; }
        if (offset + size > length) length = offset + size;
        return true;
    }
    bool complete() const {
        if (!active || invalid || !length) return false;
        for (size_t i = 0; i < length; ++i) if (!covered[i]) return false;
        return true;
    }
};
// Tokens are monotonically assigned; invalidation prevents reuse after a link
// generation changes, without exposing any callback-owned pointers.
struct Ticket {
    uint32_t serial{}, pending{};
    uint32_t issue() { if (!++serial) ++serial; return pending = serial; }
    void clear() { pending = 0; }
    bool matches(uint32_t token) const { return token && token == pending; }
};
}
