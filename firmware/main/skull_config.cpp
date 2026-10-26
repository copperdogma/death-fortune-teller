#include "skull_config.h"
#include <cctype>
#include <cstring>

namespace skull::storage {
namespace {
char *trim(char *text) {
    while (*text && std::isspace(static_cast<unsigned char>(*text))) ++text;
    char *end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) --end;
    *end = 0;
    return text;
}
bool copy(char *dest, size_t size, const char *source) {
    if (std::strlen(source) >= size) return false;
    std::strcpy(dest, source);
    return true;
}
bool number(const char *text, uint32_t &result, uint32_t low, uint32_t high, bool hex = false) {
    if (!*text || *text == '-' || *text == '+') return false;
    if (hex && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    else hex = false;
    if (!*text) return false;
    uint64_t value = 0;
    for (; *text; ++text) {
        unsigned digit;
        if (*text >= '0' && *text <= '9') digit = *text - '0';
        else if (hex && *text >= 'a' && *text <= 'f') digit = *text - 'a' + 10;
        else if (hex && *text >= 'A' && *text <= 'F') digit = *text - 'A' + 10;
        else return false;
        value = value * (hex ? 16 : 10) + digit;
        if (value > high) return false;
    }
    if (value < low) return false;
    result = static_cast<uint32_t>(value);
    return true;
}
}
bool parse_config(FILE *file, Config &config) {
    config = Config{};
    if (!file) return false;
    Config candidate;
    bool first_line = true;
    char buffer[256];
    while (std::fgets(buffer, sizeof buffer, file)) {
        if (!std::strchr(buffer, '\n') && !std::feof(file)) return false;
        char *line = buffer;
        if (first_line && std::strlen(line) >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf) line += 3;
        first_line = false;
        char *key = trim(line);
        if (!*key || *key == '#') continue;
        char *equals = std::strchr(key, '=');
        if (!equals) return false;
        *equals = 0;
        char *value = trim(equals + 1);
        if (char *comment = std::strstr(value, " #")) { *comment = 0; value = trim(value); }
        key = trim(key);
        if (!*key) return false;
        uint32_t n = 0;
        if (!std::strcmp(key, "speaker_name")) {
            if (!copy(candidate.speaker_name, sizeof candidate.speaker_name, value)) return false;
        } else if (!std::strcmp(key, "role")) {
            // A secondary config is a wiring/config mistake for this single-board prop.
            if (std::strcmp(value, "primary") && std::strcmp(value, "death")) return false;
        } else if (!std::strcmp(key, "secondary_mac_address")) {
            if (!copy(candidate.secondary_mac, sizeof candidate.secondary_mac, value)) return false;
        } else if (!std::strcmp(key, "speaker_volume")) {
            if (!number(value, n, 0, 100)) return false;
            candidate.volume_percent = n;
        } else if (!std::strcmp(key, "servo_us_min") || !std::strcmp(key, "servo_us_max")) {
            if (!number(value, n, 500, 2500)) return false;
            if (!std::strcmp(key, "servo_us_min")) candidate.servo_us_min = n; else candidate.servo_us_max = n;
        } else if (!std::strcmp(key, "servo_reverse")) {
            if (std::strcmp(value, "true") && std::strcmp(value, "false")) return false;
            candidate.servo_reverse = !std::strcmp(value, "true");
        } else if (!std::strcmp(key, "finger_wait_ms") ||
                   !std::strcmp(key, "snap_delay_min_ms") || !std::strcmp(key, "snap_delay_max_ms") || !std::strcmp(key, "cooldown_ms")) {
            if (!number(value, n, 0, 300000)) return false;
            if (!std::strcmp(key, "finger_wait_ms")) candidate.finger_wait_ms = n;
            else if (!std::strcmp(key, "snap_delay_min_ms")) candidate.snap_delay_min_ms = n;
            else if (!std::strcmp(key, "snap_delay_max_ms")) candidate.snap_delay_max_ms = n;
            else candidate.cooldown_ms = n;
        } else if (!std::strcmp(key, "printer_baud")) {
            // The first qualified output path is deliberately fixed at 9600.
            if (!number(value, n, 9600, 9600)) return false;
            candidate.printer_baud = n;
        } else if (!std::strcmp(key, "fortunes_json")) {
            if (value[0] != '/' || std::strstr(value, "..") ||
                !copy(candidate.fortunes_json, sizeof candidate.fortunes_json, value)) return false;
        }
        // Unknown but well-formed legacy settings remain extensible.
    }
    if (std::ferror(file) || !*candidate.speaker_name || candidate.servo_us_min >= candidate.servo_us_max ||
        candidate.snap_delay_min_ms > candidate.snap_delay_max_ms) return false;
    config = candidate;
    return true;
}
}
