#pragma once

#include <cstdint>
#include "esp_err.h"

namespace skull::actuators {
struct Stats {
    uint32_t deferred_updates;
    uint64_t max_pending_us;
    uint8_t pending_channels; // Bit 0 jaw, bit 1 left eye, bit 2 right eye.
    bool stuck; // At least one currently pending channel observed for >=100 ms.
};
// Same diagnostic-task owner as tick(); no cross-task synchronization.
Stats stats();
// Boot storage diagnostics only: initializes GPIO32/33 lights without touching
// the jaw timer/channel/GPIO. Idempotent; later init() retains the active lights.
esp_err_t init_lights();
// Adds jaw GPIO23 (50 Hz) after configure(), retaining the existing LED outputs.
// Call before starting the diagnostic task.
esp_err_t init();
// Set calibrated pulse limits before normal control; default safe 1400..1600 us.
// ServoESP32 maps these limits over 0..180 degrees, reversing with 180-angle.
// Only logical 3..77 degrees are commanded, preserving Death's endpoint margin.
esp_err_t configure(unsigned minimum_us, unsigned maximum_us, bool reverse);
// Diagnostic task only, approximately every 20 ms. RMS uses signed 16-bit PCM
// units. Speech/audio_busy cancel the boot check and idle breathing. Idle jaw
// breathing opens to 30 degrees over 2 s, holds 100 ms, closes over 2 s, and
// starts every 7 s (first interval begins at the first tick). Eyes remain dim.
// jaw_override >=0 takes priority over speech, idle checks and breathing.
void tick(uint16_t rms, bool speaking, bool audio_busy = false, int jaw_override = -1);
// Repeat the gentle check; invoke from the same task as tick(), while idle.
void test();
// Same control-task owner as tick(); may also be set during boot before that
// task starts. Overrides only eye brightness: count flashes, 200 ms on/200 ms
// off each, optionally repeating after an additional 2000 ms dark gap. The first
// tick anchors time; delayed ticks skip elapsed phases. Speech never clears a
// diagnostic. A one-shot returns to speech/idle brightness; count 0 clears it.
void diagnostic(unsigned count, bool repeat = false);
// Full pattern duration; include the inter-group gap for boot error retries.
uint32_t diagnostic_duration_ms(unsigned count, bool include_gap = true);
}
