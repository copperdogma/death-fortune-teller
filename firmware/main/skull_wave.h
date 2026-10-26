#pragma once

#include <cstdint>
#include <cstdio>

namespace skull::wave {
struct Info {
    uint32_t data_offset = 0;
    uint32_t data_bytes = 0;
};

// Inspect a seekable RIFF/WAVE file without allocating memory. Only PCM,
// 44,100 Hz, stereo, 16-bit audio is accepted; RIFF must span the entire file.
// On success, file is positioned
// at the first PCM byte. On failure, info is cleared; file position is unspecified.
bool inspect(FILE *file, Info &info);
}
