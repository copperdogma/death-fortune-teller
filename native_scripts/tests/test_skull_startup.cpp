#include "skull_startup.h"
#include "skull_wave.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <vector>

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
