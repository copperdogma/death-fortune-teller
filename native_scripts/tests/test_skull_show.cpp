// Actual controller and catalog, simulated transport/audio completion and clock:
// c++ -std=c++17 -Wall -Wextra -Werror -Iscripts/tests/show_stubs -Ifirmware/main \
// scripts/tests/test_skull_show.cpp firmware/main/skull_show.cpp \
// firmware/main/skull_catalog.cpp firmware/main/skull_wave.cpp -o /tmp/test-skull-show
// /tmp/test-skull-show
#include "skull_show.h"
#include "skull_catalog.h"
#include "skull_audio.h"
#include "skull_radio.h"
#include "skull_actuators.h"
#include "skull_secondary.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace mock {
int64_t clock_us = 1000000;
bool speaker = true, peer = true, scanning = false, discovery = false;
bool audio_busy = false, audio_ready = false, audio_complete = false, audio_error = false;
bool prepare_ok = true, start_ok = true;
esp_err_t request_status = ESP_OK;
skull::radio::PeerResult reply = skull::radio::PeerResult::idle;
unsigned prepares = 0, starts = 0, cancels = 0, requests = 0, flashes = 0;
bool secondary_connected = false, secondary_connected_event = false;
bool secondary_advertising = false, secondary_busy = false, request_available = false;
bool response_allowed = true;
unsigned responses_accepted = 0, responses_rejected = 0, secondary_polls = 0;
skull::secondary::Request incoming{};
const skull::catalog::Asset *prepared = nullptr;
std::vector<std::string> events;
std::vector<std::string> requested_paths;
skull::catalog::Catalog assets;
void tick(int64_t delta = 0, bool initialized = true, bool startup = false) {
    clock_us += delta;
    skull::show::tick(initialized, startup);
}
void setup(bool primary = true) {
    assets.count = 2;
    std::strcpy(assets.assets[0].path, "/audio/Skit - sample.wav");
    assets.assets[0].skit = true;
    std::strcpy(assets.assets[1].path, "/audio/Marco.wav");
    assets.count = 3;
    std::strcpy(assets.assets[2].path, "/audio/Polo.wav");
    skull::show::init(&assets, primary);
}
void enabled() { tick(); tick(10000000); }
void trigger_prepare() {
    assert(skull::show::trigger()); tick();
    assert(std::strcmp(skull::show::state_name(), "preparing") == 0);
    assert(prepares == 1 && starts == 0 && requests == 0 && flashes == 1);
}
void awaiting() {
    trigger_prepare(); audio_ready = true; tick();
    assert(requests == 1 && starts == 0);
    assert(std::strcmp(skull::show::state_name(), "awaiting-peer") == 0);
}
void finish() { audio_busy = false; audio_complete = true; tick(); }
void finish_greeting_marco() { finish(); tick(300000); }
void incoming_request(const char *path, uint32_t token = 42) {
    incoming.token = token; std::strcpy(incoming.path, path); request_available = true;
}
}
int64_t esp_timer_get_time() { return mock::clock_us; }
uint32_t esp_random() { return 0; }
namespace skull::audio {
bool prepare(const catalog::Asset *asset) {
    ++mock::prepares; mock::events.push_back("prepare");
    if (!mock::prepare_ok) return false;
    mock::prepared = asset; mock::audio_busy = true; mock::audio_ready = false;
    mock::audio_complete = false; return true;
}
bool ready() { return mock::audio_ready; }
bool busy() { return mock::audio_busy; }
bool complete() { return mock::audio_complete; }
bool error() { return mock::audio_error; }
bool start() { ++mock::starts; mock::events.push_back("start"); return mock::start_ok; }
void cancel() { ++mock::cancels; mock::events.push_back("cancel"); }
Stats stats() { return {}; }
}
namespace skull::radio {
bool stream_ready() { return mock::speaker; }
bool peer_ready() { return mock::peer; }
bool peer_scanning() { return mock::scanning; }
void set_peer_discovery(bool enabled) { mock::discovery = enabled; }
esp_err_t request_file(const char *path) {
    assert(mock::audio_ready && mock::prepared);
    if (!std::strcmp(path, "/audio/Polo.wav")) {
        assert(mock::audio_complete && !mock::audio_busy);
        assert(!std::strcmp(mock::prepared->path, "/audio/Marco.wav"));
    } else assert(std::strcmp(path, mock::prepared->path) == 0);
    mock::requested_paths.emplace_back(path);
    ++mock::requests; mock::events.push_back("request");
    if (mock::request_status == ESP_OK) mock::reply = PeerResult::pending;
    return mock::request_status;
}
PeerResult peer_result() { return mock::reply; }
void clear_peer_result() {
    if (mock::reply != PeerResult::pending) mock::reply = PeerResult::idle;
}
}
namespace skull::actuators { void diagnostic(unsigned count, bool repeat) {
    assert(count == 1 && !repeat); ++mock::flashes;
} }
namespace skull::secondary {
void poll() { ++mock::secondary_polls; }
bool ready() { return true; }
void advertising(bool enabled) { mock::secondary_advertising = enabled; }
bool connected() { return mock::secondary_connected; }
bool take_connected() {
    bool result = mock::secondary_connected_event;
    mock::secondary_connected_event = false; return result;
}
void busy(bool value) { mock::secondary_busy = value; }
bool take_request(Request &request) {
    if (!mock::request_available) return false;
    request = mock::incoming; mock::request_available = false; return true;
}
bool respond(uint32_t token, bool accept) {
    assert(token == mock::incoming.token);
    if (accept) {
        assert(mock::audio_ready);
        mock::events.push_back("respond-accept"); ++mock::responses_accepted;
    } else { mock::events.push_back("respond-reject"); ++mock::responses_rejected; }
    return mock::secondary_connected && mock::response_allowed;
}
}

static void boot_and_cooldown() {
    using namespace mock;
    setup(); tick(30000000, false); assert(!skull::show::trigger());
    assert(!skull::show::local(assets.assets[0].path));
    tick(0, false, true); assert(prepares == 0 && !discovery);
    tick(); assert(discovery && !skull::show::trigger());
    tick(9999999); assert(!skull::show::trigger());
    tick(1); assert(prepares == 0); // Hydrated/idle state never triggers a show.
    assert(skull::show::trigger());
}
static void readiness_gates() {
    using namespace mock;
    setup(); enabled(); speaker = false; tick(); assert(!skull::show::trigger());
    speaker = true; peer = false; tick(); assert(!skull::show::trigger());
    peer = true; audio_busy = true; tick(); assert(!skull::show::trigger());
    audio_busy = false; tick(0, true, true); assert(!skull::show::trigger());
    tick(); assert(prepares == 1 && !skull::show::trigger());
    assert(!std::strcmp(prepared->path, "/audio/Marco.wav"));
}
static void handshake_and_no_backlog() {
    using namespace mock;
    setup(); enabled(); awaiting();
    assert(!skull::show::trigger()); tick(); assert(starts == 0);
    reply = skull::radio::PeerResult::accepted; tick();
    assert(starts == 1 && events == std::vector<std::string>({"prepare", "request", "start"}));
    assert(!skull::show::trigger()); finish(); assert(!skull::show::busy());
    tick(9999999); assert(!skull::show::trigger());
    tick(1); assert(prepares == 1 && starts == 1 && requests == 1);
    assert(skull::show::trigger()); assert(!skull::show::trigger());
}
static void stale_trigger_consumed() {
    using namespace mock;
    setup(); enabled(); assert(skull::show::trigger());
    peer = false; tick(); peer = true; tick();
    assert(prepares == 1 && requests == 0); // Only a fresh greeting, no stale Home show.
    assert(!std::strcmp(prepared->path, "/audio/Marco.wav"));
    assert(!skull::show::trigger());
}
static void rejected() {
    using namespace mock;
    setup(); enabled(); awaiting(); reply = skull::radio::PeerResult::rejected; tick();
    assert(cancels == 1 && starts == 0 && skull::show::busy());
    tick(30000000); assert(prepares == 1 && requests == 1 && skull::show::busy());
    audio_busy = false; tick(); assert(!skull::show::busy());
    tick(10000000); assert(prepares == 1 && starts == 0); // No automatic replay.
}
static void peer_timeout_hold() {
    using namespace mock;
    setup(); enabled(); awaiting(); tick(5999999); assert(cancels == 0);
    tick(1); assert(cancels == 1 && starts == 0);
    audio_busy = false; tick(10000000); assert(skull::show::busy()); // Pending radio still owns request.
    reply = skull::radio::PeerResult::timed_out; tick(); assert(!skull::show::busy());
    tick(10000000); assert(prepares == 1 && requests == 1);
}
static void prebuffer_timeout() {
    using namespace mock;
    setup(); enabled(); trigger_prepare(); tick(4999999); assert(cancels == 0);
    tick(1); assert(cancels == 1 && requests == 0 && starts == 0);
}
static void peer_vanishes_during_prepare() {
    using namespace mock;
    setup(); enabled(); trigger_prepare(); peer = false; audio_ready = true; tick();
    assert(cancels == 1 && requests == 0 && starts == 0);
}
static void request_failure() {
    using namespace mock;
    setup(); enabled(); trigger_prepare(); request_status = ESP_FAIL; audio_ready = true; tick();
    assert(cancels == 1 && requests == 1 && starts == 0);
}
static void disconnect_during_playback() {
    using namespace mock;
    setup(); enabled(); awaiting(); reply = skull::radio::PeerResult::accepted; tick();
    speaker = false; tick(); assert(cancels == 1 && starts == 1);
    speaker = true; tick(30000000); assert(skull::show::busy() && starts == 1 && requests == 1);
    audio_busy = false; tick(); tick(10000000); assert(prepares == 1);
}
static void accepted_reply_after_peer_disconnect() {
    using namespace mock;
    setup(); enabled(); awaiting();
    reply = skull::radio::PeerResult::accepted; peer = false; tick();
    assert(cancels == 1 && starts == 0 && skull::show::busy());
    peer = true; tick(30000000); // Reconnection cannot start the prepared stale show.
    assert(starts == 0 && requests == 1 && skull::show::busy());
    audio_busy = false; tick(); tick(10000000);
    assert(skull::show::busy() && starts == 0 && prepares == 2 && requests == 1);
    assert(!std::strcmp(prepared->path, "/audio/Marco.wav")); // Fresh reconnect greeting only.
}
static void queued_trigger_counts_as_busy() {
    using namespace mock;
    setup(); enabled(); assert(!skull::show::busy());
    assert(skull::show::trigger()); assert(skull::show::busy());
    assert(!skull::show::local(assets.assets[0].path));
    peer = false; tick(); assert(!skull::show::busy() && prepares == 0);
}
static void local_diagnostic_bypasses_peer() {
    using namespace mock;
    setup(); tick(); peer = false;
    assert(!skull::show::local("/audio/missing.wav"));
    assert(skull::show::local(assets.assets[0].path));
    assert(!skull::show::local(assets.assets[1].path));
    audio_ready = true; tick(); assert(starts == 1 && requests == 0 && flashes == 0);
    finish(); tick(20000000); assert(!skull::show::trigger());
}
static void named_paired_keeps_handshake() {
    using namespace mock;
    setup();
    assert(!skull::show::paired(assets.assets[0].path));
    enabled();
    assert(!skull::show::paired("/audio/missing.wav"));
    assert(!skull::show::paired(assets.assets[1].path)); // Not a skit.
    assert(skull::show::paired(assets.assets[0].path));
    assert(prepares == 1 && requests == 0 && starts == 0 && flashes == 1);
    assert(!skull::show::trigger() && !skull::show::paired(assets.assets[0].path));
    audio_ready = true; tick(); assert(requests == 1 && starts == 0);
    reply = skull::radio::PeerResult::accepted; tick(); assert(starts == 1);
    finish(); assert(!skull::show::paired(assets.assets[0].path));
    tick(10000000); assert(skull::show::paired(assets.assets[0].path));
}
static void named_paired_rechecks_peer_and_pending() {
    using namespace mock;
    setup(); enabled(); peer = false;
    assert(!skull::show::paired(assets.assets[0].path) && prepares == 0);
    peer = true; tick(); assert(skull::show::trigger());
    assert(!skull::show::paired(assets.assets[0].path));
    tick(); assert(prepares == 1); // Existing Matter request keeps ownership.
}
static void secondary_rejects_named_paired() {
    using namespace mock;
    setup(false); enabled();
    assert(!skull::show::paired(assets.assets[0].path) && prepares == 0);
}
static void secondary_never_autostarts() {
    using namespace mock;
    setup(false); scanning = true; enabled(); tick(30000000);
    assert(!discovery && !skull::show::trigger() && prepares == 0);
    assert(skull::show::local(assets.assets[0].path));
}
static void marco_requires_initialized_scan_interval() {
    using namespace mock;
    setup(); peer = false; scanning = true; tick(30000000, false);
    assert(prepares == 0); tick(); tick(4999999); assert(prepares == 0);
    tick(1); assert(prepares == 1 && std::strcmp(prepared->path, "/audio/Marco.wav") == 0);
    audio_ready = true; tick(); assert(starts == 1 && requests == 0);
}
static void scan_interval_resets_after_pause() {
    using namespace mock;
    setup(); peer = false; scanning = true; tick(); tick(4000000);
    scanning = false; tick(); scanning = true; tick();
    tick(4999999); assert(prepares == 0); tick(1); assert(prepares == 1);
}
static void marco_cadence_continues_while_playing() {
    using namespace mock;
    setup(); peer = false; scanning = true; tick(); tick(5000000);
    audio_ready = true; tick(); assert(discovery && starts == 1);
    tick(1000000); finish(); // A one-second clip does not reset the scan cadence.
    tick(3999999); assert(prepares == 1);
    tick(1); assert(prepares == 2 && requests == 0);
}
static void marco_drops_busy_due_time_without_backlog() {
    using namespace mock;
    setup(); peer = false; scanning = true; tick(); tick(5000000);
    audio_ready = true; tick(); tick(5000000); // The previous Marco is still playing.
    assert(prepares == 1 && starts == 1 && discovery);
    tick(1000000); finish(); assert(prepares == 1); // No late queued Marco.
    tick(3999999); assert(prepares == 1);
    tick(1); assert(prepares == 2);
}
static void secondary_startup_advertising_gate() {
    using namespace mock;
    setup(false); tick(10000000, false);
    assert(secondary_polls == 1 && secondary_busy && !secondary_advertising && prepares == 0);
    // Once initialization finished, an independent active tone still marks the
    // server busy; advertising may remain enabled while it rejects requests.
    tick(0, true, true); assert(secondary_advertising && secondary_busy);
    tick(); assert(secondary_advertising && !secondary_busy && prepares == 0);
}
static void secondary_connection_is_silent_until_explicit_polo() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; secondary_connected_event = true; tick();
    assert(prepares == 0 && starts == 0);
    incoming_request("/audio/Polo.wav"); tick();
    assert(prepares == 1 && std::strcmp(prepared->path, "/audio/Polo.wav") == 0);
    audio_ready = true; tick(); assert(starts == 1 && requests == 0 && responses_accepted == 1);
    assert(events == std::vector<std::string>({"prepare", "respond-accept", "start"}));
    finish(); tick(20000000); assert(prepares == 1);
}
static void secondary_connection_without_speaker_no_backlog() {
    using namespace mock;
    setup(false); tick(); speaker = false; secondary_connected = true; secondary_connected_event = true;
    tick(); assert(prepares == 0); speaker = true; tick();
    assert(prepares == 0 && starts == 0);
}
static void secondary_request_accept_before_start() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; incoming_request(assets.assets[0].path); tick();
    assert(prepares == 1 && starts == 0 && responses_accepted == 0);
    audio_ready = true; tick();
    assert(starts == 1 && responses_accepted == 1 && requests == 0);
    assert(events == std::vector<std::string>({"prepare", "respond-accept", "start"}));
}
static void secondary_rejects_unknown_asset() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; incoming_request("/audio/unknown.wav"); tick();
    assert(prepares == 0 && responses_rejected == 1 && starts == 0);
    tick(20000000); assert(prepares == 0);
}
static void secondary_expired_response_cancels() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; incoming_request(assets.assets[0].path); tick();
    response_allowed = false; audio_ready = true; tick();
    assert(starts == 0 && cancels == 1 && skull::show::busy());
    response_allowed = true; tick(10000000); assert(starts == 0 && prepares == 1);
    audio_busy = false; tick(); tick(20000000); assert(starts == 0 && prepares == 1);
}
static void secondary_disconnect_during_prepare() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; incoming_request(assets.assets[0].path); tick();
    secondary_connected = false; audio_ready = true; tick();
    assert(starts == 0 && cancels == 1);
    secondary_connected = true; audio_busy = false; tick(); tick(20000000);
    assert(starts == 0 && prepares == 1);
}
static void secondary_busy_request_rejected_without_backlog() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; incoming_request(assets.assets[0].path); tick();
    audio_ready = true; tick(); assert(starts == 1);
    incoming_request(assets.assets[0].path, 43); tick();
    assert(prepares == 1 && responses_rejected == 1);
    finish(); tick(20000000); assert(prepares == 1 && starts == 1);
}
static void secondary_prebuffer_deadline_rejects() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; incoming_request(assets.assets[0].path); tick();
    tick(2499999); assert(cancels == 0 && responses_rejected == 0);
    tick(1); assert(cancels == 1 && responses_rejected == 1 && starts == 0);
    audio_ready = true; tick(); assert(starts == 0); // Late buffering cannot revive the request.
    audio_busy = false; tick(); tick(20000000); assert(prepares == 1 && starts == 0);
}
static void secondary_failed_prepare_rejected() {
    using namespace mock;
    setup(false); tick(); secondary_connected = true; prepare_ok = false;
    incoming_request(assets.assets[0].path); tick();
    assert(prepares == 1 && responses_rejected == 1 && !skull::show::busy() && starts == 0);
    prepare_ok = true; tick(20000000); assert(prepares == 1);
}
static void greeting_startup_and_pcm_ordering() {
    using namespace mock;
    peer = false; setup(); tick(0, false); peer = true; tick(20000000, false, true);
    assert(prepares == 0 && requests == 0 && !skull::show::trigger());
    tick(); assert(prepares == 1 && skull::show::busy());
    assert(!skull::show::trigger() && !skull::show::paired(assets.assets[0].path));
    assert(!skull::show::local(assets.assets[0].path));
    audio_ready = true; tick(); assert(starts == 1 && requests == 0);
    tick(1000000); assert(requests == 0); // No early Polo while Marco PCM remains.
    finish(); assert(requests == 0);
    tick(299999); assert(requests == 0 && !skull::show::trigger());
    tick(1); assert(requests == 1 && starts == 1);
    assert(requested_paths == std::vector<std::string>({"/audio/Polo.wav"}));
    assert(events == std::vector<std::string>({"prepare", "start", "request"}));
    assert(!std::strcmp(skull::show::state_name(), "awaiting-polo"));
    assert(!skull::show::trigger() && !skull::show::paired(assets.assets[0].path));
    reply = skull::radio::PeerResult::accepted; tick();
    assert(!skull::show::busy() && prepares == 1 && starts == 1);
    tick(9999999); assert(!skull::show::trigger()); tick(1);
    assert(prepares == 1 && requests == 1 && skull::show::trigger());
}
static void greeting_rejected_no_replay() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick(); audio_ready = true; tick(); finish_greeting_marco();
    reply = skull::radio::PeerResult::rejected; tick();
    assert(cancels == 1 && starts == 1 && requests == 1);
    tick(); tick(30000000);
    assert(!skull::show::busy() && prepares == 1 && starts == 1 && requests == 1);
}
static void greeting_timeout_waits_for_transport_terminal() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick(); audio_ready = true; tick(); finish_greeting_marco();
    tick(5999999); assert(cancels == 0); tick(1); assert(cancels == 1);
    tick(20000000); assert(skull::show::busy() && requests == 1 && starts == 1);
    reply = skull::radio::PeerResult::timed_out; tick(); tick(10000000);
    assert(!skull::show::busy() && prepares == 1 && starts == 1 && requests == 1);
}
static void greeting_request_queue_failure_no_replay() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick(); audio_ready = true; tick();
    request_status = ESP_FAIL; finish_greeting_marco(); assert(cancels == 1 && requests == 1 && starts == 1);
    tick(); tick(30000000); assert(!skull::show::busy() && prepares == 1 && requests == 1);
}
static void greeting_peer_loss_before_start_cancels() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick();
    peer = false; tick(); assert(cancels == 1 && starts == 0 && requests == 0);
    audio_busy = false; tick(); tick(30000000); assert(prepares == 1 && starts == 0);
}
static void greeting_peer_loss_after_start_finishes_without_polo() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick(); audio_ready = true; tick();
    peer = false; tick(); assert(cancels == 0 && starts == 1);
    finish(); tick(20000000); assert(!skull::show::busy() && requests == 0 && prepares == 1);
}
static void greeting_reuses_inflight_scan_marco() {
    using namespace mock;
    peer = false; setup(); scanning = true; tick(); tick(5000000);
    audio_ready = true; tick(); assert(prepares == 1 && starts == 1);
    peer = true; scanning = false; tick(); assert(prepares == 1 && requests == 0);
    finish_greeting_marco(); assert(requests == 1 && starts == 1 && prepares == 1);
    reply = skull::radio::PeerResult::accepted; tick(); tick(20000000);
    assert(prepares == 1 && starts == 1 && requests == 1);
}
static void greeting_peer_loss_during_tail_discards_polo() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick(); audio_ready = true; tick(); finish();
    peer = false; tick(100000); tick(30000000);
    assert(requests == 0 && starts == 1 && prepares == 1 && !skull::show::busy());
}
static void greeting_ack_after_peer_loss_cannot_succeed() {
    using namespace mock;
    peer = false; setup(); tick(); peer = true; tick(); audio_ready = true; tick(); finish_greeting_marco();
    peer = false; reply = skull::radio::PeerResult::accepted; tick();
    assert(cancels == 1 && requests == 1 && starts == 1);
    tick(30000000); assert(!skull::show::busy() && prepares == 1);
}
static void greeting_waits_for_speaker_without_blocking_retry() {
    using namespace mock;
    peer = false; speaker = false; setup(); tick(); peer = true; tick();
    assert(!skull::show::busy() && !audio_busy && prepares == 0 && requests == 0);
    tick(300000000); // Exhausted automatic reconnect attempts must not wedge retry.
    assert(!skull::show::busy() && !audio_busy && prepares == 0);
    assert(!skull::show::trigger() && !skull::show::paired(assets.assets[0].path));
    assert(!skull::show::local(assets.assets[0].path));
    // The console's existing !show::busy() && !audio::busy() branch permits
    // manual retry. Simulate the successful reconnection it causes.
    speaker = true; tick(); assert(skull::show::busy() && prepares == 1);
    assert(!std::strcmp(prepared->path, "/audio/Marco.wav"));
    audio_ready = true; tick(); finish_greeting_marco();
    assert(starts == 1 && requests == 1);
    reply = skull::radio::PeerResult::accepted; tick(); tick(30000000);
    assert(!skull::show::busy() && starts == 1 && prepares == 1 && requests == 1);
}
static void dormant_greeting_disconnected_peer_is_discarded() {
    using namespace mock;
    peer = false; speaker = false; setup(); tick(); peer = true; tick();
    assert(!skull::show::busy()); peer = false; tick(); speaker = true; tick();
    tick(30000000); assert(prepares == 0 && starts == 0 && requests == 0 && !skull::show::busy());
}
static void missing_greeting_asset_does_not_retry() {
    using namespace mock;
    peer = false; setup(); assets.count = 1; tick(); peer = true; tick();
    tick(30000000); assert(prepares == 0 && requests == 0 && !skull::show::busy());
}
static void paired_peer_loss_after_start_finishes_locally() {
    using namespace mock;
    setup(); enabled(); awaiting(); reply = skull::radio::PeerResult::accepted; tick();
    peer = false; reply = skull::radio::PeerResult::disconnected; tick();
    assert(starts == 1 && cancels == 0 && skull::show::busy());
    finish(); tick(20000000); assert(!skull::show::busy() && prepares == 1 && starts == 1 && requests == 1);
}
int main() {
    struct Test { const char *name; void (*run)(); };
    const Test tests[] = {
        {"boot/cooldown", boot_and_cooldown}, {"readiness gates", readiness_gates},
        {"prepare/request/ACK/start and no backlog", handshake_and_no_backlog},
        {"stale trigger discarded", stale_trigger_consumed}, {"rejection/no replay", rejected},
        {"peer timeout/cancel hold", peer_timeout_hold}, {"prebuffer timeout", prebuffer_timeout},
        {"peer lost while buffering", peer_vanishes_during_prepare}, {"request rejected", request_failure},
        {"speaker disconnect/no replay", disconnect_during_playback},
        {"accepted reply after disconnect canceled", accepted_reply_after_peer_disconnect},
        {"queued trigger owns busy state", queued_trigger_counts_as_busy},
        {"explicit local bypass", local_diagnostic_bypasses_peer},
        {"named pair keeps readiness/ACK/cooldown gates", named_paired_keeps_handshake},
        {"named pair rechecks peer and pending trigger", named_paired_rechecks_peer_and_pending},
        {"secondary rejects named pair", secondary_rejects_named_paired},
        {"secondary passive", secondary_never_autostarts},
        {"Marco initialized/5-second scan", marco_requires_initialized_scan_interval},
        {"scan pause resets interval", scan_interval_resets_after_pause},
        {"Marco cadence persists during playback", marco_cadence_continues_while_playing},
        {"busy Marco drops due time without backlog", marco_drops_busy_due_time_without_backlog},
        {"secondary startup advertising gate", secondary_startup_advertising_gate},
        {"secondary connection silent; explicit Polo accepted", secondary_connection_is_silent_until_explicit_polo},
        {"secondary speakerless connection no backlog", secondary_connection_without_speaker_no_backlog},
        {"secondary prepare/accept/start ordering", secondary_request_accept_before_start},
        {"secondary unknown asset rejected", secondary_rejects_unknown_asset},
        {"secondary expired token cancels", secondary_expired_response_cancels},
        {"secondary disconnected prepare cancels", secondary_disconnect_during_prepare},
        {"secondary busy request no backlog", secondary_busy_request_rejected_without_backlog},
        {"secondary prebuffer deadline rejects", secondary_prebuffer_deadline_rejects},
        {"secondary failed prepare rejected", secondary_failed_prepare_rejected},
        {"greeting startup/PCM/Polo/ACK/cooldown ordering", greeting_startup_and_pcm_ordering},
        {"greeting rejected no replay", greeting_rejected_no_replay},
        {"greeting timeout waits transport terminal", greeting_timeout_waits_for_transport_terminal},
        {"greeting request queue failure no replay", greeting_request_queue_failure_no_replay},
        {"greeting disconnect before start cancels", greeting_peer_loss_before_start_cancels},
        {"greeting disconnect after start finishes without Polo", greeting_peer_loss_after_start_finishes_without_polo},
        {"greeting reuses scan Marco without repeat", greeting_reuses_inflight_scan_marco},
        {"greeting peer loss during tail discards Polo", greeting_peer_loss_during_tail_discards_polo},
        {"greeting ACK after peer loss canceled", greeting_ack_after_peer_loss_cannot_succeed},
        {"dormant greeting leaves speaker retry available", greeting_waits_for_speaker_without_blocking_retry},
        {"dormant greeting discarded on peer loss", dormant_greeting_disconnected_peer_is_discarded},
        {"missing Marco does not retry greeting", missing_greeting_asset_does_not_retry},
        {"paired show peer loss after start finishes locally", paired_peer_loss_after_start_finishes_locally},
    };
    // Controller globals intentionally live for one firmware boot. Fork starts
    // each independent scenario from pristine process state without test-only
    // production reset hooks; supported on our macOS and Linux host runners.
    for (const auto &test : tests) {
        std::fflush(stdout);
        const pid_t child = fork(); assert(child >= 0);
        if (!child) { test.run(); _exit(0); }
        int status = 0; assert(waitpid(child, &status, 0) == child);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            std::fprintf(stderr, "FAILED: %s (status %d)\n", test.name, status); return 1;
        }
        std::printf("PASS: %s\n", test.name);
    }
    std::printf("skull_show: %zu deterministic controller scenarios passed\n", sizeof(tests) / sizeof(tests[0]));
}
