#include "skull_audio.h"
#include "skull_catalog.h"
#include "skull_wave.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <limits>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifndef SKULL_AUDIO_CLOSE
#define SKULL_AUDIO_CLOSE std::fclose
#endif

#ifndef SKULL_AUDIO_SD_ROOT
#define SKULL_AUDIO_SD_ROOT "/sdcard"
#endif

namespace skull::audio {
namespace {
constexpr unsigned block_frames = 882, block_count = 8;
// ESP-IDF FatFs speed guidance: larger stdio buffering avoids repeated small
// VFS reads. Retain one PSRAM buffer for the sole worker FILE lifetime.
constexpr size_t io_buffer_bytes = 8192;
struct Block {
    uint8_t pcm[block_frames * 4];
    uint32_t frames;
    uint16_t rms;
    bool speaking;
    bool last;
};
enum class State { idle, requested, loading, ready, playing, complete, failed, cancelling, acknowledged };
std::atomic<State> state{State::idle};
// Sole worker owns the FILE. Remains true through fopen/fclose, including the
// final-close window after the provider has already consumed the last block.
std::atomic<bool> file_active{false};
std::atomic<const catalog::Asset *> asset{nullptr};
std::atomic<uint32_t> underruns{0}, frames{0}, envelope{0}, stack_water{0};
std::atomic<uint32_t> produced_blocks{0}, read_us_total{0}, read_us_max{0};
std::atomic<uint32_t> process_us_total{0}, process_us_max{0}, refill_gap_us_max{0};
std::atomic<uint32_t> raw_pcm_fnv1a{2166136261u};
std::atomic<uint32_t> role_frames{0}, input_nonzero_samples{0}, output_nonzero_samples{0}, output_peak{0};
Block *blocks = nullptr;
char *io_buffer = nullptr;
std::atomic<bool> initialized{false};
QueueHandle_t free_queue = nullptr, ready_queue = nullptr;
StaticQueue_t free_control, ready_control;
uint8_t free_storage[block_count], ready_storage[block_count];
TaskHandle_t worker_handle = nullptr;
unsigned gain = 100;
char role = 'A';
// These two variables belong exclusively to the single provider callback.
int current = -1;
uint32_t offset = 0;

void reset_queues() {
    xQueueReset(free_queue);
    xQueueReset(ready_queue);
    for (uint8_t i = 0; i < block_count; ++i) xQueueSend(free_queue, &i, 0);
}
// One producer writes these counters; stats() reads atomic snapshots. Saturating
// 32-bit totals avoid callback-unfriendly/non-lock-free 64-bit atomics on ESP32.
void add_time(std::atomic<uint32_t> &total, std::atomic<uint32_t> &maximum, uint32_t us) {
    const uint32_t old = total.load(std::memory_order_relaxed);
    total.store(us > UINT32_MAX - old ? UINT32_MAX : old + us, std::memory_order_relaxed);
    maximum.store(std::max(maximum.load(std::memory_order_relaxed), us), std::memory_order_relaxed);
}
uint32_t elapsed_us(int64_t start, int64_t end) {
    return static_cast<uint32_t>(std::min<int64_t>(UINT32_MAX, std::max<int64_t>(0, end - start)));
}
void worker(void *) {
    FILE *file = nullptr;
    uint32_t position = 0, total = 0;
    bool exhausted = false;
    int64_t last_publication_us = 0;
    unsigned refill_burst = 0;
    for (;;) {
        bool produced_this_iteration = false;
        stack_water.store(uxTaskGetStackHighWaterMark(nullptr), std::memory_order_relaxed);
        State s = state.load(std::memory_order_acquire);
        if (s == State::acknowledged) {
            if (file) SKULL_AUDIO_CLOSE(file);
            file = nullptr;
            file_active.store(false, std::memory_order_release);
            reset_queues();
            envelope.store(0);
            state.store(State::idle, std::memory_order_release);
        } else if (s == State::requested) {
            // Claimed only by this worker. Cancellation can supersede any phase.
            State expected = State::requested;
            if (!state.compare_exchange_strong(expected, State::loading)) continue;
            if (file) SKULL_AUDIO_CLOSE(file);
            file = nullptr;
            reset_queues();
            position = 0; exhausted = false; last_publication_us = 0;
            const auto *a = asset.load(std::memory_order_acquire);
            char path[160];
            const int length = std::snprintf(path, sizeof(path), SKULL_AUDIO_SD_ROOT "%s%s",
                                            a->path[0] == '/' ? "" : "/", a->path);
            wave::Info info{};
            file_active.store(true, std::memory_order_release);
            if (length <= 0 || length >= static_cast<int>(sizeof(path)) ||
                !(file = std::fopen(path, "rb")) ||
                std::setvbuf(file, io_buffer, _IOFBF, io_buffer_bytes) != 0 ||
                !wave::inspect(file, info) ||
                info.data_bytes == 0 || info.data_bytes != a->wave.data_bytes ||
                info.data_offset != a->wave.data_offset) {
                if (file) SKULL_AUDIO_CLOSE(file);
                file = nullptr;
                file_active.store(false, std::memory_order_release);
                expected = State::loading;
                state.compare_exchange_strong(expected, State::failed);
            } else total = info.data_bytes / 4;
        } else if ((s == State::loading || s == State::ready || s == State::playing) && !exhausted && file) {
            uint8_t index;
            if (xQueueReceive(free_queue, &index, 0) == pdTRUE) {
                Block &b = blocks[index];
                b.frames = std::min<uint32_t>(block_frames, total - position);
                b.last = position + b.frames == total;
                uint64_t sum = 0;
                bool any_speaking = false;
                const int64_t read_start_us = esp_timer_get_time();
                const bool read_ok = std::fread(b.pcm, 4, b.frames, file) == b.frames && !std::ferror(file);
                const int64_t read_end_us = esp_timer_get_time();
                add_time(read_us_total, read_us_max, elapsed_us(read_start_us, read_end_us));
                if (!read_ok) {
                    xQueueSend(free_queue, &index, 0);
                    State expected = state.load(std::memory_order_acquire);
                    while (expected == State::loading || expected == State::ready || expected == State::playing) {
                        if (state.compare_exchange_weak(expected, State::failed)) break;
                    }
                    // A callback may still own a block; failed must be cancelled
                    // and acknowledged before another prepare can reset queues.
                    SKULL_AUDIO_CLOSE(file); file = nullptr;
                    file_active.store(false, std::memory_order_release);
                } else {
                    uint32_t hash = raw_pcm_fnv1a.load(std::memory_order_relaxed);
                    for (uint32_t byte = 0; byte < b.frames * 4; ++byte)
                        hash = (hash ^ b.pcm[byte]) * 16777619u;
                    raw_pcm_fnv1a.store(hash, std::memory_order_relaxed);
                    const auto *a = asset.load(std::memory_order_acquire);
                    uint32_t block_role_frames = 0, input_nonzero = 0, output_nonzero = 0, peak = 0;
                    for (uint32_t frame = 0; frame < b.frames; ++frame) {
                        const bool speak = catalog::speaking(*a, role, position + frame);
                        any_speaking |= speak;
                        block_role_frames += speak;
                        for (unsigned channel = 0; channel < 2; ++channel) {
                            int16_t sample;
                            uint8_t *p = b.pcm + frame * 4 + channel * 2;
                            std::memcpy(&sample, p, 2);
                            input_nonzero += sample != 0;
                            sample = speak ? static_cast<int16_t>(static_cast<int32_t>(sample) * static_cast<int32_t>(gain) / 100) : 0;
                            std::memcpy(p, &sample, 2);
                            output_nonzero += sample != 0;
                            const int32_t wide = sample;
                            peak = std::max<uint32_t>(peak, wide < 0 ? -wide : wide);
                            sum += static_cast<uint64_t>(wide * wide);
                        }
                    }
                    b.rms = static_cast<uint16_t>(std::sqrt(static_cast<double>(sum) / (b.frames * 2)));
                    b.speaking = any_speaking;
                    role_frames.fetch_add(block_role_frames, std::memory_order_relaxed);
                    input_nonzero_samples.fetch_add(input_nonzero, std::memory_order_relaxed);
                    output_nonzero_samples.fetch_add(output_nonzero, std::memory_order_relaxed);
                    output_peak.store(std::max(output_peak.load(std::memory_order_relaxed), peak), std::memory_order_relaxed);
                    const int64_t process_end_us = esp_timer_get_time();
                    add_time(process_us_total, process_us_max, elapsed_us(read_end_us, process_end_us));
                    produced_blocks.fetch_add(1, std::memory_order_relaxed);
                    position += b.frames;
                    exhausted = b.last;
                    xQueueSend(ready_queue, &index, 0);
                    produced_this_iteration = true;
                    const int64_t publication_us = esp_timer_get_time();
                    if (last_publication_us && state.load(std::memory_order_relaxed) == State::playing) {
                        const uint32_t gap = elapsed_us(last_publication_us, publication_us);
                        refill_gap_us_max.store(std::max(refill_gap_us_max.load(std::memory_order_relaxed), gap), std::memory_order_relaxed);
                    }
                    last_publication_us = publication_us;
                    if (exhausted) {
                        SKULL_AUDIO_CLOSE(file); file = nullptr;
                        file_active.store(false, std::memory_order_release);
                    }
                    if (exhausted || uxQueueMessagesWaiting(ready_queue) == block_count) {
                        State expected = State::loading;
                        state.compare_exchange_strong(expected, State::ready, std::memory_order_release);
                    }
                }
            }
        }
        // Refill at most two blocks before blocking. One fixed 10 ms sleep
        // per 20 ms block left too little budget for measured SD + DSP work.
        // Bounded bursts amortize the sleep while retaining idle/watchdog time.
        if (produced_this_iteration && ++refill_burst < 2 &&
            uxQueueMessagesWaiting(free_queue) > 0) continue;
        refill_burst = 0;
        // At the project's 100 Hz tick, 2 ms rounds to zero. Block for at
        // least one tick after every burst and every non-producing iteration.
        vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(2)));
    }
}
}

bool init(unsigned gain_percent, bool primary) {
    if (blocks) return false;
    gain = std::min(gain_percent, 100u);
    role = primary ? 'A' : 'B';
    blocks = static_cast<Block *>(heap_caps_malloc(sizeof(Block) * block_count, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!blocks) return false;
    io_buffer = static_cast<char *>(heap_caps_malloc(io_buffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!io_buffer) { heap_caps_free(blocks); blocks = nullptr; return false; }
    free_queue = xQueueCreateStatic(block_count, sizeof(uint8_t), free_storage, &free_control);
    ready_queue = xQueueCreateStatic(block_count, sizeof(uint8_t), ready_storage, &ready_control);
    reset_queues();
    if (xTaskCreate(worker, "skull_audio", 4096, nullptr, 5, &worker_handle) != pdPASS) {
        heap_caps_free(io_buffer); io_buffer = nullptr;
        heap_caps_free(blocks); blocks = nullptr;
        return false;
    }
    initialized.store(true, std::memory_order_release);
    return true;
}
bool set_gain_percent(unsigned percent) {
    if (percent > 100 || !quiescent()) return false;
    gain = percent;
    return true;
}
unsigned gain_percent() { return gain; }
bool prepare(const catalog::Asset *next) {
    if (!initialized.load(std::memory_order_acquire) || !next) return false;
    const State s = state.load(std::memory_order_acquire);
    // A read failure may leave a callback-owned block. It needs cancel first.
    if (s != State::idle && s != State::complete) return false;
    asset.store(next, std::memory_order_release);
    frames.store(0); underruns.store(0); envelope.store(0);
    raw_pcm_fnv1a.store(2166136261u);
    produced_blocks.store(0); read_us_total.store(0); read_us_max.store(0);
    process_us_total.store(0); process_us_max.store(0); refill_gap_us_max.store(0);
    role_frames.store(0); input_nonzero_samples.store(0); output_nonzero_samples.store(0); output_peak.store(0);
    state.store(State::requested, std::memory_order_release);
    return true;
}
bool ready() { return state.load(std::memory_order_acquire) == State::ready; }
const catalog::Asset *prepared() { return asset.load(std::memory_order_acquire); }
bool start() {
    State expected = State::ready;
    return state.compare_exchange_strong(expected, State::playing, std::memory_order_acq_rel);
}
void cancel() {
    State s = state.load(std::memory_order_acquire);
    while (s != State::idle && s != State::complete && s != State::cancelling && s != State::acknowledged) {
        if (state.compare_exchange_weak(s, State::cancelling, std::memory_order_acq_rel)) break;
    }
}
bool busy() {
    const auto s = state.load(std::memory_order_acquire);
    return s != State::idle && s != State::complete && s != State::failed;
}
bool quiescent() {
    return !busy() && !file_active.load(std::memory_order_acquire);
}
bool playing() { return state.load(std::memory_order_acquire) == State::playing; }
bool complete() { return state.load(std::memory_order_acquire) == State::complete; }
bool error() { return state.load(std::memory_order_acquire) == State::failed; }

bool provide(uint8_t *data, int32_t length) {
    if (!initialized.load(std::memory_order_acquire) || !data || length <= 0) return false;
    const State s = state.load(std::memory_order_acquire);
    if (s == State::cancelling) {
        // Only this callback can release its partial block. The worker drains
        // all other queued blocks only after this acknowledgment.
        if (current >= 0) {
            const uint8_t index = static_cast<uint8_t>(current);
            xQueueSend(free_queue, &index, 0);
            current = -1; offset = 0;
        }
        envelope.store(0, std::memory_order_relaxed);
        state.store(State::acknowledged, std::memory_order_release);
        return true;
    }
    if (s != State::playing) return false;
    uint32_t capacity = static_cast<uint32_t>(length / 4), written = 0;
    unsigned traversed = 0;
    // Even a malformed huge request cannot make callback work exceed one FIFO.
    while (capacity && traversed++ < block_count) {
        if (current < 0) {
            uint8_t index;
            if (xQueueReceive(ready_queue, &index, 0) != pdTRUE) {
                underruns.fetch_add(1, std::memory_order_relaxed);
                envelope.store(0, std::memory_order_relaxed);
                break;
            }
            current = index; offset = 0;
        }
        Block &b = blocks[current];
        const uint32_t count = std::min(capacity, b.frames - offset);
        std::memcpy(data + written * 4, b.pcm + offset * 4, count * 4);
        offset += count; written += count; capacity -= count;
        frames.fetch_add(count, std::memory_order_relaxed);
        envelope.store(b.rms | (b.speaking ? 0x10000u : 0), std::memory_order_relaxed);
        if (offset == b.frames) {
            const bool last = b.last;
            const uint8_t index = static_cast<uint8_t>(current);
            current = -1; offset = 0;
            xQueueSend(free_queue, &index, 0);
            if (last) {
                State expected = State::playing;
                state.compare_exchange_strong(expected, State::complete, std::memory_order_release);
                break;
            }
        }
    }
    if (capacity && state.load(std::memory_order_acquire) == State::playing && traversed > block_count) {
        underruns.fetch_add(1, std::memory_order_relaxed);
        envelope.store(0, std::memory_order_relaxed);
    }
    return true;
}
Animation animation() {
    if (!playing()) return {0, false};
    const uint32_t value = envelope.load(std::memory_order_relaxed);
    return {static_cast<uint16_t>(value), (value & 0x10000u) != 0};
}
Stats stats() {
    return {underruns.load(), frames.load(), stack_water.load(),
            state.load(std::memory_order_acquire) == State::cancelling,
            produced_blocks.load(), read_us_total.load(), read_us_max.load(),
            process_us_total.load(), process_us_max.load(), refill_gap_us_max.load(),
            role_frames.load(), input_nonzero_samples.load(), output_nonzero_samples.load(), output_peak.load(), raw_pcm_fnv1a.load()};
}
}
