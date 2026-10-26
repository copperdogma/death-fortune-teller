#pragma once
#include "skull_wave.h"
#include <cstddef>
#include <cstdint>

namespace skull::catalog {
constexpr size_t max_assets = 32;
constexpr size_t max_lines = 128;
constexpr size_t path_capacity = 128;
struct Interval { uint32_t first_frame, end_frame; char role; };
struct Asset {
    char path[path_capacity]{};
    wave::Info wave{};
    Interval lines[max_lines]{};
    uint16_t line_count = 0;
    bool skit = false;
    bool clipped_timing = false;
    uint32_t play_count = 0;
    uint64_t last_played_ms = 0;
};
struct Catalog {
    Asset assets[max_assets]{};
    size_t count = 0;
    int last_selected = -1;
};
// Caller supplies storage (PSRAM on device); scan resets prior statistics. Bad
// assets are diagnosed and skipped. Returns true if any supported asset remains.
bool scan(Catalog &catalog, const char *audio_directory);
bool speaking(const Asset &asset, char role, uint32_t frame);
// Returns an asset index, or -1 if no scripted skit exists. Selection updates
// statistics once. Uniform entropy selects among the three highest weights.
int select(Catalog &catalog, uint64_t now_ms, uint32_t entropy);
}
