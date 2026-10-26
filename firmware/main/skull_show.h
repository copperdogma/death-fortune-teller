#pragma once
#include <cstdint>
namespace skull::catalog { struct Catalog; }
namespace skull::show {
void init(catalog::Catalog *assets, bool primary);
// After startup, a primary peer-readiness edge plays Marco to local PCM EOF,
// allows a 300 ms output-tail guard (speaker acoustic latency is unmeasured),
// requests raw /audio/Polo.wav, and waits for bounded legacy ACK/readback.
// Secondary connections alone are silent; Polo requires that explicit request.
// Safe from Matter's system task: bounded single pending trigger, no backlog.
bool trigger();
// Diagnostic/control task only. Local playback deliberately bypasses the peer
// handshake, so it is available for single-board acceptance before Jeff returns.
bool local(const char *path);
// Diagnostic task only: choose one skit, retaining normal peer/readiness,
// cooldown and ACK-before-start gates. Never queues or bypasses the handshake.
bool paired(const char *path);
void tick(bool initialization_finished, bool startup_active);
// Active playback/handshake or queued work with a usable speaker. A greeting
// waiting idle for an unavailable speaker leaves manual connection retry free.
bool busy();
const char *state_name();
}
