#include "skull_catalog.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <limits>
#include <sys/stat.h>
#include <memory>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace skull::catalog {
namespace {
bool extension(const char *name, const char *suffix) {
    const size_t n = std::strlen(name), s = std::strlen(suffix);
    if (n < s) return false;
    for (size_t i = 0; i < s; ++i) {
        char c = name[n - s + i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != suffix[i]) return false;
    }
    return true;
}
char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
    char *end = text + std::strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = 0;
    return text;
}
bool integer(const char *text, uint32_t &value) {
    if (!*text) return false;
    uint64_t result = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        result = result * 10 + (*text - '0');
        if (result > UINT32_MAX) return false;
    }
    value = static_cast<uint32_t>(result);
    return true;
}
bool script(FILE *file, Asset &asset) {
    char buffer[256];
    uint32_t previous_end = 0;
    bool first_line = true;
    while (std::fgets(buffer, sizeof buffer, file)) {
        // A full buffer without a newline could be an overlong authored line.
        if (!std::strchr(buffer, '\n') && !std::feof(file)) return false;
        char *line = buffer;
        if (first_line && std::strlen(line) >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf) line += 3;
        first_line = false;
        line = trim(line);
        if (!*line) continue;
        char *fields[4];
        size_t count = 0;
        for (;;) {
            if (count == 4) return false;
            fields[count++] = line;
            char *comma = std::strchr(line, ',');
            if (!comma) break;
            *comma = 0;
            line = comma + 1;
        }
        for (size_t i = 0; i < count; ++i) fields[i] = trim(fields[i]);
        if (count < 3 || std::strlen(fields[0]) != 1 ||
            (fields[0][0] != 'A' && fields[0][0] != 'B')) return false;
        uint32_t start, duration;
        if (!integer(fields[1], start) || !integer(fields[2], duration) ||
            !duration || duration > UINT32_MAX - start || start < previous_end ||
            asset.line_count == max_lines) return false;
        if (count == 4) {
            char *end = nullptr;
            errno = 0;
            const double jaw = std::strtod(fields[3], &end);
            if (!*fields[3] || *end || errno == ERANGE || !std::isfinite(jaw)) return false;
        }
        const uint32_t end_ms = start + duration;
        const uint64_t first = (static_cast<uint64_t>(start) * 44100 + 999) / 1000;
        const uint64_t end = (static_cast<uint64_t>(end_ms) * 44100 + 999) / 1000;
        const uint32_t frames = asset.wave.data_bytes / 4;
        if (first >= frames) return false;
        if (end > frames) asset.clipped_timing = true;
        asset.lines[asset.line_count++] = {
            static_cast<uint32_t>(first), static_cast<uint32_t>(std::min<uint64_t>(end, frames)), fields[0][0]};
        previous_end = end_ms;
    }
    return !std::ferror(file) && asset.line_count > 0;
}
void reject(const char *name, const char *reason) {
    std::fprintf(stderr, "catalog: skipping %s: %s\n", name, reason);
}
double weight(const Asset &asset, uint64_t now) {
    const uint64_t elapsed = now >= asset.last_played_ms ? now - asset.last_played_ms : 0;
    return std::log1p(static_cast<double>(elapsed)) / (static_cast<double>(asset.play_count) + 1.0);
}
}

namespace {
struct ScanScratch {
    char nested[512], relative[path_capacity], actual[512];
    struct stat info;
};
struct FreeScratch {
    void operator()(ScanScratch *p) const {
#ifdef ESP_PLATFORM
        heap_caps_free(p);
#else
        std::free(p);
#endif
    }
};
// Keep recursive path storage off the internal main-task stack. It is needed
// only during boot scanning, before radios start.
// Bounded recursion permits Death category directories without following links.
bool scan_directory(Catalog &catalog, const char *directory, const char *relative, unsigned depth) {
    if (!directory || depth > 4) return false;
#ifdef ESP_PLATFORM
    auto *memory = static_cast<ScanScratch *>(heap_caps_malloc(sizeof(ScanScratch), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    auto *memory = static_cast<ScanScratch *>(std::malloc(sizeof(ScanScratch)));
#endif
    std::unique_ptr<ScanScratch, FreeScratch> scratch(memory);
    if (!scratch) { reject(directory, "scan scratch allocation failed"); return false; }
    DIR *dir = opendir(directory);
    if (!dir) { reject(directory, "cannot open directory"); return false; }
    while (dirent *entry = readdir(dir)) {
        const char *name = entry->d_name;
        if (name[0] == '.') continue; // Ignore macOS AppleDouble metadata.
        auto &nested = scratch->nested;
        auto &relative_path = scratch->relative;
        if (std::snprintf(nested, sizeof nested, "%s/%s", directory, name) >= static_cast<int>(sizeof nested) ||
            std::snprintf(relative_path, sizeof relative_path, "%s/%s", relative, name) >= static_cast<int>(sizeof relative_path)) {
            reject(name, "path too long"); continue;
        }
        auto &info = scratch->info;
#ifdef ESP_PLATFORM
        // ESP-IDF FAT VFS supports stat; FAT cards have no symbolic links.
        if (stat(nested, &info) != 0) continue;
#else
        if (lstat(nested, &info) != 0) continue;
#endif
        if (S_ISDIR(info.st_mode)) { scan_directory(catalog, nested, relative_path, depth + 1); continue; }
        if (!S_ISREG(info.st_mode) || !extension(name, ".wav")) continue;
        if (catalog.count == max_assets) { reject(name, "asset capacity reached"); continue; }
        Asset &asset = catalog.assets[catalog.count];
        asset.path[0] = 0;
        asset.wave = {};
        asset.line_count = 0;
        asset.skit = false;
        asset.clipped_timing = false;
        asset.play_count = 0;
        asset.last_played_ms = 0;
        auto &actual = scratch->actual;
        if (std::snprintf(actual, sizeof actual, "%s/%s", directory, name) >= static_cast<int>(sizeof actual) ||
            std::snprintf(asset.path, sizeof asset.path, "%s", relative_path) >= static_cast<int>(sizeof asset.path)) {
            reject(name, "path too long"); continue;
        }
        FILE *file = std::fopen(actual, "rb");
        const bool valid = file && wave::inspect(file, asset.wave);
        if (file) std::fclose(file);
        if (!valid) { reject(name, "unsupported WAV"); continue; }
        asset.skit = std::strncmp(name, "Skit", 4) == 0;
        const size_t length = std::strlen(actual);
        std::memcpy(actual + length - 4, ".txt", 5);
        file = std::fopen(actual, "r");
        if (file) {
            const bool parsed = script(file, asset);
            std::fclose(file);
            if (!parsed) { reject(name, "invalid script"); continue; }
        } else if (asset.skit) { reject(name, "missing script"); continue; }
        if (asset.clipped_timing)
            std::fprintf(stderr, "catalog: %s: script end clipped to audio EOF (authored file unchanged)\n", name);
        ++catalog.count;
    }
    closedir(dir);
    return catalog.count != 0;
}

} // namespace

bool scan(Catalog &catalog, const char *directory) {
    catalog.count = 0;
    catalog.last_selected = -1;
    return scan_directory(catalog, directory, "/audio", 0);
}

bool speaking(const Asset &asset, char role, uint32_t frame) {
    if (!asset.skit) return true;
    if (frame >= asset.wave.data_bytes / 4) return false;
    if (role != 'A' && role != 'B') return false;
    for (uint16_t i = 0; i < asset.line_count; ++i) {
        const auto &line = asset.lines[i];
        if (frame < line.first_frame) break;
        if (frame < line.end_frame) return line.role == role;
    }
    return false;
}

int select(Catalog &catalog, uint64_t now, uint32_t entropy) {
    size_t order[max_assets], count = 0;
    for (size_t i = 0; i < catalog.count; ++i)
        if (catalog.assets[i].skit) order[count++] = i;
    if (!count) return -1;
    std::sort(order, order + count, [&](size_t a, size_t b) {
        const double wa = weight(catalog.assets[a], now), wb = weight(catalog.assets[b], now);
        return wa == wb ? std::strcmp(catalog.assets[a].path, catalog.assets[b].path) < 0 : wa > wb;
    });
    size_t pool[3], pool_count = 0;
    for (size_t i = 0; i < count && pool_count < 3; ++i)
        if (count == 1 || static_cast<int>(order[i]) != catalog.last_selected) pool[pool_count++] = order[i];
    const int selected = static_cast<int>(pool[entropy % pool_count]);
    auto &asset = catalog.assets[selected];
    if (asset.play_count != UINT32_MAX) ++asset.play_count;
    asset.last_played_ms = now;
    catalog.last_selected = selected;
    return selected;
}
}
