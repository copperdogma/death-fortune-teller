#include "skull_startup.h"
#include "skull_wave.h"
#include "death_audio_reference.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#ifndef SKULL_STARTUP_PATH
#define SKULL_STARTUP_PATH "/sdcard/audio/initialized.wav"
#endif

namespace skull::startup {
namespace {
constexpr char tag[] = "skull_startup";
constexpr uint32_t max_bytes = 1024 * 1024;
// Death's Flip 5 clipped the first word at the TwoSkulls 800 ms preroll.
// Allow two seconds of stream silence for the sink to unmute before speech.
constexpr uint32_t preroll_frames = 88200;
enum class State { unavailable, ready, playing, complete, interrupted };
std::atomic<State> state{State::unavailable};
uint8_t *pcm = nullptr;
uint32_t bytes = 0;
constexpr uint32_t envelope_frames = 882; // 20 ms at 44.1 kHz
uint16_t *envelope = nullptr;
std::atomic<uint32_t> consumed_frames{0};
uint32_t cursor = 0, preroll = preroll_frames; // audio provider owns these
State reported = State::unavailable; // diagnostic task owns this
uint32_t fnv1a_update(uint32_t hash, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}
}

ProbeResult probe_file(bool single_sector) {
    ProbeResult result{};
    FILE *file = std::fopen(SKULL_STARTUP_PATH, "rb");
    if (!file) return result;
    std::setvbuf(file, nullptr, _IONBF, 0);
    wave::Info info{};
    if (!wave::inspect(file, info) || info.data_bytes > max_bytes) {
        std::fclose(file);
        return result;
    }
    const size_t chunk_size = single_sector ? 512 : 4096;
    auto *buffer = static_cast<uint8_t *>(heap_caps_malloc(chunk_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!buffer) {
        std::fclose(file);
        return result;
    }
    const int fd = fileno(file);
    const uint64_t pcm_begin = info.data_offset;
    const uint64_t file_end = pcm_begin + info.data_bytes;
    uint64_t offset = pcm_begin - pcm_begin % chunk_size;
    // Inspect already validated the extent. Read at most the PCM length plus
    // one alignment prefix; authored leading/trailing chunks are not scanned.
    bool ok = fd >= 0 && lseek(fd, static_cast<off_t>(offset), SEEK_SET) == static_cast<off_t>(offset);
    uint32_t hash = 2166136261u;
    unsigned bad_chunks = 0;
    while (ok && offset < file_end) {
        const size_t wanted = static_cast<size_t>(std::min<uint64_t>(chunk_size, file_end - offset));
        size_t received = 0;
        while (received < wanted) {
            const ssize_t count = read(fd, buffer + received, wanted - received);
            if (count <= 0) { ok = false; break; }
            received += static_cast<size_t>(count);
        }
        if (!ok) break;
        const uint32_t chunk_hash = fnv1a_update(2166136261u, buffer, received);
        const unsigned chunk_index = offset / chunk_size;
        if (!single_sector && chunk_index < sizeof(reference_chunk_hashes) / sizeof(reference_chunk_hashes[0]) &&
            chunk_hash != reference_chunk_hashes[chunk_index]) {
            if (++bad_chunks <= 4) ESP_LOGI(tag, "SDCHUNK offset=%lu got=%08lx expected=%08lx bytes=%u",
                static_cast<unsigned long>(offset), static_cast<unsigned long>(chunk_hash),
                static_cast<unsigned long>(reference_chunk_hashes[chunk_index]), static_cast<unsigned>(received));
        }
        const uint64_t pcm_begin = info.data_offset;
        const uint64_t pcm_end = pcm_begin + info.data_bytes;
        const uint64_t chunk_end = offset + received;
        const uint64_t begin = std::max(offset, pcm_begin);
        const uint64_t end = std::min(chunk_end, pcm_end);
        if (begin < end) {
            const auto *pcm_bytes = buffer + (begin - offset);
            const size_t length = static_cast<size_t>(end - begin);
            hash = fnv1a_update(hash, pcm_bytes, length);
            for (size_t i = 0; i + 1 < length; i += 2) {
                int16_t sample;
                std::memcpy(&sample, pcm_bytes + i, sizeof(sample));
                const uint16_t magnitude = sample == INT16_MIN ? 32768u :
                    static_cast<uint16_t>(sample < 0 ? -sample : sample);
                if (sample) ++result.nonzero_samples;
                result.peak = std::max(result.peak, magnitude);
            }
            result.bytes += static_cast<uint32_t>(length);
        }
        offset += received;
    }
    ESP_LOGI(tag, "SDCHUNK bad_chunks=%u", bad_chunks);
    heap_caps_free(buffer);
    std::fclose(file);
    result.ok = ok && result.bytes == info.data_bytes;
    result.hash = result.ok ? hash : 0;
    if (!result.ok) result = {};
    return result;
}

bool probe_window(bool single_sector) {
    constexpr size_t length = sizeof(reference_window);
    auto *a = static_cast<uint8_t *>(heap_caps_malloc(length, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    auto *b = static_cast<uint8_t *>(heap_caps_malloc(length, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!a || !b) { heap_caps_free(a); heap_caps_free(b); return false; }
    auto read_window = [single_sector](uint8_t *destination) {
        FILE *file = std::fopen(SKULL_STARTUP_PATH, "rb");
        if (!file) return false;
        std::setvbuf(file, nullptr, _IONBF, 0);
        const int fd = fileno(file);
        const bool seek = lseek(fd, reference_offset, SEEK_SET) == reference_offset;
        size_t count = 0;
        while (seek && count < sizeof(reference_window)) {
            const size_t wanted = single_sector ? 512 : sizeof(reference_window);
            const ssize_t received = read(fd, destination + count, wanted);
            if (received != static_cast<ssize_t>(wanted)) break;
            count += static_cast<size_t>(received);
        }
        std::fclose(file);
        return count == sizeof(reference_window);
    };
    const bool first_ok = read_window(a);
    const uint32_t first_hash = first_ok ? fnv1a_update(2166136261u, a, length) : 0;
    const bool second_ok = read_window(b);
    const uint32_t retained_hash = first_ok ? fnv1a_update(2166136261u, a, length) : 0;
    const uint32_t second_hash = second_ok ? fnv1a_update(2166136261u, b, length) : 0;
    bool exact = first_ok && second_ok;
    for (unsigned pass = 0; pass < 2; ++pass) {
        const auto *data = pass ? b : a;
        const bool ok = pass ? second_ok : first_ok;
        unsigned mismatches = 0, bits = 0, first = length;
        if (ok) for (unsigned i = 0; i < length; ++i) {
            const unsigned difference = data[i] ^ reference_window[i];
            if (difference) {
                if (first == length) first = i;
                ++mismatches; bits += __builtin_popcount(difference);
            }
        }
        exact = exact && !mismatches;
        ESP_LOGI(tag, "SDWINDOW pass=%u ok=%d fnv1a=%08lx mismatches=%u bits=%u first_offset=%lu expected=%02x actual=%02x", pass + 1, ok,
                 static_cast<unsigned long>(pass ? second_hash : first_hash), mismatches, bits,
                 static_cast<unsigned long>(first < length ? reference_offset + first : 0),
                 first < length ? reference_window[first] : 0, first < length ? data[first] : 0);
    }
    ESP_LOGI(tag, "SDWINDOW retained_first=%08lx unchanged=%d pair_equal=%d reference=025bf77e",
             static_cast<unsigned long>(retained_hash), first_hash == retained_hash,
             first_ok && second_ok && std::memcmp(a,b,length) == 0);
    heap_caps_free(a); heap_caps_free(b);
    return exact && first_hash == retained_hash;
}

bool load(bool primary, unsigned volume_percent) {
    if (pcm || state.load(std::memory_order_acquire) != State::unavailable) return false;
    // Death uses one startup clip; retain the identical PSRAM/provider pipeline.
    (void)primary;
    const char *path = SKULL_STARTUP_PATH;
    FILE *file = std::fopen(path, "rb");
    wave::Info info{};
    if (!file) { ESP_LOGE(tag, "Startup announcement missing: %s", path); return false; }
    const bool valid = wave::inspect(file, info);
    if (!valid || info.data_bytes > max_bytes) {
        std::fclose(file);
        ESP_LOGE(tag, "Startup WAV invalid/unsupported or exceeds 1 MiB: %s", path);
        return false;
    }
    auto *loaded = static_cast<uint8_t *>(heap_caps_malloc(info.data_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!loaded) { std::fclose(file); ESP_LOGE(tag, "Startup PCM PSRAM allocation failed"); return false; }
    // Classic ESP32 SDMMC cannot DMA into PSRAM. The SDK supports a
    // sector-at-a-time bounce fallback; stage bounded multi-sector reads in
    // internal DMA memory instead, then retain immutable PCM in PSRAM.
    constexpr size_t read_chunk = 4096;
    auto *staging = static_cast<uint8_t *>(heap_caps_malloc(read_chunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    uint32_t copied = 0;
    uint32_t staging_hash = 2166136261u;
    bool read = staging != nullptr;
    while (read && copied < info.data_bytes) {
        const size_t wanted = std::min<size_t>(read_chunk, info.data_bytes - copied);
        const size_t received = std::fread(staging, 1, wanted, file);
        read = received == wanted && !std::ferror(file);
        if (read) {
            staging_hash = fnv1a_update(staging_hash, staging, received);
            std::memcpy(loaded + copied, staging, received);
            copied += received;
        }
    }
    if (staging) heap_caps_free(staging);
    std::fclose(file);
    if (!read) {
        heap_caps_free(loaded);
        ESP_LOGE(tag, "Startup PCM read failed after %lu/%lu bytes (DMA staging=%d)",
                 static_cast<unsigned long>(copied), static_cast<unsigned long>(info.data_bytes), staging != nullptr);
        return false;
    }
    const uint32_t loaded_hash = fnv1a_update(2166136261u, loaded, info.data_bytes);
    ESP_LOGI(tag, "Startup raw PCM fnv1a staging=%08lx PSRAM=%08lx",
             static_cast<unsigned long>(staging_hash), static_cast<unsigned long>(loaded_hash));
    // Apply configured gain once outside the callback. No remote-volume or
    // speaker-wide volume change, and no callback-time analysis/SD access.
    const unsigned gain = std::min(volume_percent, 100u);
    if (gain != 100) {
        for (uint32_t i = 0; i < info.data_bytes; i += 2) {
            int16_t sample;
            std::memcpy(&sample, loaded + i, 2);
            sample = static_cast<int16_t>(static_cast<int32_t>(sample) * static_cast<int32_t>(gain) / 100);
            std::memcpy(loaded + i, &sample, 2);
        }
    }
    const uint32_t frames = info.data_bytes / 4;
    const uint32_t windows = (frames + envelope_frames - 1) / envelope_frames;
    envelope = static_cast<uint16_t *>(heap_caps_malloc(windows * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (envelope) {
        for (uint32_t window = 0; window < windows; ++window) {
            const uint32_t first = window * envelope_frames;
            const uint32_t count = std::min(envelope_frames, frames - first);
            uint64_t sum = 0;
            for (uint32_t i = 0; i < count * 2; ++i) {
                int16_t sample;
                std::memcpy(&sample, loaded + first * 4 + i * 2, 2);
                const int32_t wide = sample;
                sum += static_cast<uint64_t>(wide * wide);
            }
            envelope[window] = static_cast<uint16_t>(std::sqrt(static_cast<double>(sum) / (count * 2)));
        }
    } else ESP_LOGE(tag, "Startup envelope allocation failed; audio available without jaw animation");
    pcm = loaded; bytes = info.data_bytes;
    state.store(State::ready, std::memory_order_release);
    ESP_LOGI(tag, "Startup announcement loaded: role=%s PCM=%lu offset=%lu gain=%u%%",
             primary ? "primary" : "secondary", static_cast<unsigned long>(bytes),
             static_cast<unsigned long>(info.data_offset), gain);
    return true;
}

void cancel() {
    State current = state.load(std::memory_order_acquire);
    // Strong CAS cannot fail spuriously; the other owners only move ready to
    // playing or playing to a terminal state, so contention has bounded retries.
    while (current == State::ready || current == State::playing) {
        if (state.compare_exchange_strong(current, State::interrupted, std::memory_order_acq_rel)) return;
    }
}

void poll(bool stream_ready) {
    State current = state.load(std::memory_order_acquire);
    if (current == State::ready && stream_ready) {
        if (state.compare_exchange_strong(current, State::playing, std::memory_order_acq_rel))
            ESP_LOGI(tag, "Startup announcement starting (once per boot)");
    } else if (current == State::playing && !stream_ready) {
        state.compare_exchange_strong(current, State::interrupted, std::memory_order_acq_rel);
    }
    current = state.load(std::memory_order_acquire);
    if (current != reported) {
        if (current == State::complete) ESP_LOGI(tag, "Startup announcement PCM delivered; awaiting audible owner confirmation");
        if (current == State::interrupted) ESP_LOGW(tag, "Startup announcement cancelled/interrupted; no automatic replay");
        reported = current;
    }
}

bool provide(uint8_t *data, int32_t length) {
    if (!data || length <= 0) return false;
    if (state.load(std::memory_order_acquire) != State::playing) return false;
    uint32_t capacity = static_cast<uint32_t>(length / 4);
    const uint32_t silent = std::min(capacity, preroll);
    preroll -= silent; capacity -= silent;
    const uint32_t count = std::min(capacity, (bytes - cursor) / 4) * 4;
    if (count) std::memcpy(data + silent * 4, pcm + cursor, count);
    cursor += count;
    consumed_frames.store(cursor / 4, std::memory_order_relaxed);
    if (cursor == bytes) {
        State expected = State::playing;
        state.compare_exchange_strong(expected, State::complete, std::memory_order_release);
    }
    return true;
}

Animation animation() {
    if (state.load(std::memory_order_acquire) != State::playing) return {0, false};
    const uint32_t frames = consumed_frames.load(std::memory_order_relaxed);
    if (!frames || !envelope) return {0, false};
    return {envelope[(frames - 1) / envelope_frames], true};
}

bool finished() {
    const auto current = state.load(std::memory_order_acquire);
    return current == State::complete || current == State::interrupted || current == State::unavailable;
}

bool playing() {
    return state.load(std::memory_order_acquire) == State::playing;
}

bool busy() {
    const State current = state.load(std::memory_order_acquire);
    return current == State::ready || current == State::playing;
}
}
