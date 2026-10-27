#pragma once
#include <cstdint>

namespace skull::startup {
struct ProbeResult {
    bool ok;
    uint32_t hash;
    uint32_t bytes;
    uint32_t nonzero_samples;
    uint16_t peak;
};
// Read-only task-context SD probe. Hash/statistics are computed directly
// from the file through internal DMA memory, before any PSRAM copy.
ProbeResult probe_file(bool single_sector = false);
// Read-only task-context probe for any full WAV path, up to 8 MiB of PCM.
// Uses aligned 4096-byte internal DMA reads and does not compare against the
// initialized.wav reference chunks.
ProbeResult probe_asset_file(const char *full_path);
// Read-only stdio-path comparison for any WAV path, up to 8 MiB of PCM.
// Uses an 8192-byte PSRAM stdio buffer and 882-frame PSRAM reads.
ProbeResult probe_asset_stdio(const char *full_path);
// Idle task only: two separately retained internal buffers versus fixed reference.
bool probe_window(bool single_sector = false);
// Boot/task context only. Retains immutable PCM in PSRAM for this boot.
bool load(bool primary, unsigned volume_percent);
// Called by the diagnostic task. Starts once after media readiness; cancels
// remaining speech after a link loss instead of replaying a stale tail.
void poll(bool stream_ready);
// Thread-safe terminal cancellation: silences subsequent provider callbacks and
// prevents replay. An already in-flight callback/sink buffer cannot be recalled.
// Retains immutable PCM; never frees callback-owned memory or changes cursors.
void cancel();
// Callback context: bounded copy only; caller has already zeroed output.
bool provide(uint8_t *data, int32_t length);
bool busy();
bool finished(); // Terminal, including a missing or interrupted announcement.
bool playing(); // Includes startup preroll, but not waiting for the speaker.
struct Animation { uint16_t rms; bool speaking; };
// Consumed PCM cursor + precomputed envelope; called from control task only.
Animation animation();
}
