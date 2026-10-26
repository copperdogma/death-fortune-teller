#pragma once
#include <cstdint>
namespace skull::catalog { struct Asset; }
namespace skull::audio {
// Control-task API. Asset lifetime must extend through completion/cancellation.
bool init(unsigned gain_percent, bool primary);
bool prepare(const catalog::Asset *asset);
// Control task only, between files. Diagnostic gain; no card/NVS changes.
bool set_gain_percent(unsigned percent);
unsigned gain_percent();
bool ready();
const catalog::Asset *prepared();
bool start();
// Nonblocking. Cancellation completes only after a provider callback acknowledges
// it; if the transport stops calling provide(), busy() remains true. No replay.
void cancel();
bool busy();
// Control task only, with no concurrent prepare(): no active request or worker
// file open/read/close, including final close after PCM delivery completes.
bool quiescent();
bool playing();
bool complete();
bool error();
// Single A2DP provider only. Caller zeroes destination. No SD, allocation,
// analysis, blocking waits, or logging here. Partial frames remain zero.
bool provide(uint8_t *data, int32_t length);
struct Animation { uint16_t rms; bool speaking; };
Animation animation();
struct Stats {
    uint32_t underruns;
    uint32_t frames;
    uint32_t stack_high_water;
    bool cancel_waiting;
    // Per-clip producer measurements. Totals saturate at UINT32_MAX us.
    // Read includes fread; processing includes gain/muting/RMS, not queue wait.
    uint32_t blocks;
    uint32_t read_us_total;
    uint32_t read_us_max;
    uint32_t process_us_total;
    uint32_t process_us_max;
    // Publication-to-publication gap while playing: includes scheduler delay
    // and waiting for free blocks, so interpret alongside read/process times.
    uint32_t refill_gap_us_max;
    // Produced (including prebuffered), not necessarily delivered, PCM.
    uint32_t role_frames;
    uint32_t input_nonzero_samples;
    uint32_t output_nonzero_samples;
    uint32_t output_peak;
    uint32_t raw_pcm_fnv1a; // Diagnostic hash of raw file PCM before gain/muting.
};
Stats stats();
}
