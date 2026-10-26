#pragma once
#include <cstdint>
#include "esp_err.h"

namespace skull::radio {
using PcmCallback = int32_t (*)(uint8_t *, int32_t);
struct Config { const char *speaker_name; const char *secondary_mac; bool primary = true; };
enum class PeerResult { idle, pending, accepted, rejected, timed_out, disconnected };
struct Stats {
    uint32_t audio_callbacks, audio_bytes, dropped_events, peer_requests, peer_accepted;
    bool faulted;
    uint32_t stack_free_min;
};
// Owns one BTDM controller/Bluedroid host. NVS must already be initialized.
esp_err_t init(PcmCallback callback);
// Install after Matter has registered its BLE GAP callback, before start().
esp_err_t attach_matter_gap();
// Secondary role has no Matter node and owns its standalone GAP dispatcher.
esp_err_t attach_standalone_gap();
esp_err_t start(const Config &config);
bool speaker_connected();
bool stream_ready();
bool peer_ready();
// Disabled by default. Queue an idle discovery policy change without touching
// the shared host or an existing peer connection. Call after init().
void set_peer_discovery(bool enabled);
// Controller-confirmed scanning snapshot, for the idle Marco timing rule.
bool peer_scanning();
// Raw legacy Jeff filename; accepted means notify + matching readback only.
esp_err_t request_file(const char *filename);
PeerResult peer_result();
void clear_peer_result();
Stats stats();
// Application calls only while idle, with its own backoff. Never retransmits a file.
void retry_connections();
}
