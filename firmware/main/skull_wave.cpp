#include "skull_wave.h"

#include <climits>
#include <cstring>

namespace skull::wave {
namespace {
uint16_t le16(const unsigned char *p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t le32(const unsigned char *p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool seek(FILE *file, uint64_t offset) {
    return offset <= static_cast<uint64_t>(LONG_MAX) &&
           std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0;
}
}

bool inspect(FILE *file, Info &info) {
    info = {};
    if (!file || std::fseek(file, 0, SEEK_END) != 0) return false;
    const long length = std::ftell(file);
    if (length < 12 || !seek(file, 0)) return false;

    unsigned char header[12];
    if (std::fread(header, 1, sizeof header, file) != sizeof header ||
        std::memcmp(header, "RIFF", 4) != 0 ||
        std::memcmp(header + 8, "WAVE", 4) != 0) return false;

    // Widen before adding: malicious chunk lengths must never wrap offsets.
    const uint64_t end = 8ULL + le32(header + 4);
    if (end < 12 || end != static_cast<uint64_t>(length)) return false;

    bool have_format = false;
    bool have_data = false;
    Info candidate{};
    uint64_t position = 12;
    while (position < end) {
        if (end - position < 8 || !seek(file, position)) return false;
        unsigned char chunk[8];
        if (std::fread(chunk, 1, sizeof chunk, file) != sizeof chunk) return false;
        const uint32_t size = le32(chunk + 4);
        const uint64_t payload = position + 8;
        const uint64_t next = payload + size + (size & 1U);
        if (next > end) return false;

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (have_format || size < 16) return false;
            unsigned char format[16];
            if (std::fread(format, 1, sizeof format, file) != sizeof format ||
                le16(format) != 1 || le16(format + 2) != 2 ||
                le32(format + 4) != 44100 || le32(format + 8) != 176400 ||
                le16(format + 12) != 4 || le16(format + 14) != 16) return false;
            have_format = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            if (have_data || size == 0 || size % 4 != 0 || payload > UINT32_MAX)
                return false;
            candidate = {static_cast<uint32_t>(payload), size};
            have_data = true;
        }
        position = next;
    }
    if (!have_format || !have_data || !seek(file, candidate.data_offset)) return false;
    info = candidate;
    return true;
}
}
