#include "skull_startup.h"
#include "skull_wave.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <fstream>
#include <string>
#include <vector>

namespace {
void write_le16(std::ofstream &file, uint16_t value) {
    const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8)};
    file.write(bytes, sizeof(bytes));
}

void write_le32(std::ofstream &file, uint32_t value) {
    const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8),
                          static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
    file.write(bytes, sizeof(bytes));
}

void write_silent_wav(const std::string &path, uint32_t pcm_bytes) {
    std::ofstream file(path, std::ios::binary);
    assert(file);
    file.write("RIFF", 4); write_le32(file, pcm_bytes + 36); file.write("WAVEfmt ", 8);
    write_le32(file, 16); write_le16(file, 1); write_le16(file, 2);
    write_le32(file, 44100); write_le32(file, 176400); write_le16(file, 4); write_le16(file, 16);
    file.write("data", 4); write_le32(file, pcm_bytes);
    const char silence[4096]{};
    for (uint32_t written = 0; written < pcm_bytes;) {
        const uint32_t count = std::min<uint32_t>(sizeof(silence), pcm_bytes - written);
        file.write(silence, count);
        written += count;
    }
    assert(file);
}

uint32_t hash_zero_bytes(size_t count) {
    uint32_t hash = 2166136261u;
    while (count--) hash *= 16777619u;
    return hash;
}
}

int main(int argc, char **argv) {
    assert(argc == 3);
    const char *mode = argv[1];
    const char *fixture = argv[2];
    FILE *source = std::fopen(fixture, "rb");
    assert(source);
    skull::wave::Info info{};
    assert(skull::wave::inspect(source, info));
    std::vector<uint8_t> expected(info.data_bytes);
    assert(std::fread(expected.data(), 1, expected.size(), source) == expected.size());
    std::fclose(source);
    if (!std::strcmp(mode, "psram-fail")) startup_test_fail_at = 1;
    if (!std::strcmp(mode, "staging-fail")) startup_test_fail_at = 2;
    if (!std::strcmp(mode, "probe-alloc-fail")) startup_test_fail_at = 1;
    if (!std::strcmp(mode, "probe") || !std::strcmp(mode, "probe-alloc-fail")) {
        const auto first = skull::startup::probe_file();
        if (!std::strcmp(mode, "probe-alloc-fail")) {
            assert(!first.ok && first.bytes == 0);
            return 0;
        }
        const auto second = skull::startup::probe_file();
        assert(skull::startup::probe_window());
        assert(skull::startup::probe_window(true));
        assert(skull::startup::probe_file(true).hash == first.hash);
        const auto arbitrary = skull::startup::probe_asset_file(fixture);
        assert(arbitrary.ok && arbitrary.bytes == first.bytes && arbitrary.hash == first.hash);
        const auto stdio = skull::startup::probe_asset_stdio(fixture);
        assert(stdio.ok && stdio.bytes == first.bytes && stdio.hash == first.hash &&
               stdio.nonzero_samples == first.nonzero_samples && stdio.peak == first.peak);
        startup_test_fail_at = startup_test_allocations.load() + 1;
        const auto stdio_buffer_failure = skull::startup::probe_asset_stdio(fixture);
        assert(!stdio_buffer_failure.ok && stdio_buffer_failure.bytes == 0);
        startup_test_fail_at = startup_test_allocations.load() + 2;
        const auto pcm_buffer_failure = skull::startup::probe_asset_stdio(fixture);
        assert(!pcm_buffer_failure.ok && pcm_buffer_failure.bytes == 0);
        startup_test_fail_at = 0;
        const std::string large_fixture = std::string(fixture) + ".large.wav";
        constexpr uint32_t large_pcm_bytes = 1024 * 1024 + 4;
        write_silent_wav(large_fixture, large_pcm_bytes);
        const auto large = skull::startup::probe_asset_file(large_fixture.c_str());
        assert(large.ok && large.bytes == large_pcm_bytes && large.hash == hash_zero_bytes(large_pcm_bytes));
        assert(large.nonzero_samples == 0 && large.peak == 0);
        const auto large_stdio = skull::startup::probe_asset_stdio(large_fixture.c_str());
        assert(large_stdio.ok && large_stdio.bytes == large_pcm_bytes &&
               large_stdio.hash == hash_zero_bytes(large_pcm_bytes) &&
               large_stdio.nonzero_samples == 0 && large_stdio.peak == 0);
        const std::string oversized_fixture = std::string(fixture) + ".oversized.wav";
        write_silent_wav(oversized_fixture, 8 * 1024 * 1024 + 4);
        const auto oversized = skull::startup::probe_asset_file(oversized_fixture.c_str());
        assert(!oversized.ok && oversized.bytes == 0 && oversized.hash == 0);
        const auto oversized_stdio = skull::startup::probe_asset_stdio(oversized_fixture.c_str());
        assert(!oversized_stdio.ok && oversized_stdio.bytes == 0 && oversized_stdio.hash == 0);
        uint32_t expected_hash = 2166136261u;
        uint32_t nonzero = 0;
        uint16_t peak = 0;
        for (size_t i = 0; i < expected.size(); ++i)
            expected_hash = (expected_hash ^ expected[i]) * 16777619u;
        for (size_t i = 0; i < expected.size(); i += 2) {
            int16_t sample;
            std::memcpy(&sample, expected.data() + i, 2);
            if (sample) ++nonzero;
            const uint16_t magnitude = sample == INT16_MIN ? 32768u :
                static_cast<uint16_t>(sample < 0 ? -sample : sample);
            peak = std::max(peak, magnitude);
        }
        assert(first.ok && second.ok);
        assert(first.hash == expected_hash && second.hash == expected_hash);
        assert(first.bytes == expected.size() && first.nonzero_samples == nonzero && first.peak == peak);
        assert(first.bytes == 425700 && first.hash == 0xd6d8b5d5u);
        return 0;
    }
    if (!std::strcmp(mode, "psram-fail") || !std::strcmp(mode, "staging-fail")) {
        assert(!skull::startup::load(true, 100));
        assert(!skull::startup::busy() && skull::startup::finished());
        return 0;
    }

    assert(skull::startup::load(true, 100));
    assert((startup_test_staging_caps & (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)) ==
           (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    assert(skull::startup::busy());
    if (!std::strncmp(mode, "cancel-", 7)) {
        const int calls_before_cancel = startup_test_allocations.load();
        std::vector<uint8_t> buffer(88200 * 4, 0xA5);
        if (std::strcmp(mode, "cancel-ready")) {
            skull::startup::poll(true);
            assert(skull::startup::playing());
            // Consume only part of the preroll for cancel-preroll; consume all
            // silence and a real PCM block for cancel-audio.
            const int32_t amount = !std::strcmp(mode, "cancel-preroll") ? 512 :
                static_cast<int32_t>(buffer.size());
            assert(skull::startup::provide(buffer.data(), amount));
            if (!std::strcmp(mode, "cancel-audio")) {
                assert(skull::startup::provide(buffer.data(), 882 * 4));
                assert(std::memcmp(buffer.data(), expected.data(), 882 * 4) == 0);
            }
        }
        skull::startup::cancel();
        assert(skull::startup::finished() && !skull::startup::busy() && !skull::startup::playing());
        const auto animation = skull::startup::animation();
        assert(!animation.speaking && animation.rms == 0);
        std::fill(buffer.begin(), buffer.end(), 0xA5);
        assert(!skull::startup::provide(buffer.data(), static_cast<int32_t>(buffer.size())));
        assert(std::all_of(buffer.begin(), buffer.end(), [](uint8_t b) { return b == 0xA5; }));
        skull::startup::poll(false);
        skull::startup::poll(true);
        skull::startup::cancel();
        assert(skull::startup::finished() && !skull::startup::playing());
        assert(!skull::startup::provide(buffer.data(), 512));
        assert(startup_test_allocations.load() == calls_before_cancel);
        return 0;
    }
    skull::startup::poll(true);
    std::vector<uint8_t> silence(88200 * 4, 0xA5);
    const int calls_before_callback = startup_test_allocations.load();
    assert(skull::startup::provide(silence.data(), static_cast<int32_t>(silence.size())));
    assert(std::all_of(silence.begin(), silence.end(), [](uint8_t b) { return b == 0xA5; }));
    assert(startup_test_allocations.load() == calls_before_callback);
    std::vector<uint8_t> output(expected.size());
    assert(skull::startup::provide(output.data(), static_cast<int32_t>(output.size())));
    assert(std::memcmp(output.data(), expected.data(), expected.size()) == 0);
    assert(startup_test_allocations.load() == calls_before_callback);
    assert(!skull::startup::busy() && skull::startup::finished());
    skull::startup::cancel();
    skull::startup::poll(true);
    assert(skull::startup::finished() && !skull::startup::playing());
    return 0;
}
