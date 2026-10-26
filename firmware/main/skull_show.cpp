#include "skull_show.h"
#include "skull_audio.h"
#include "skull_catalog.h"
#include "skull_radio.h"
#include "skull_actuators.h"
#include "skull_secondary.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <atomic>
#include <cstring>

namespace skull::show {
namespace {
constexpr char tag[] = "skull_show";
enum class State { idle, preparing, awaiting_peer, greeting_tail, awaiting_polo, playing, cancelling };
State state = State::idle;
catalog::Catalog *assets = nullptr;
const catalog::Asset *selected = nullptr;
bool primary = true, initialization_seen = false, coordinated = false;
bool discovery_enabled = false;
bool peer_was_ready = false, greeting_pending = false, greeting_marco = false;
uint32_t incoming_token = 0;
std::atomic<bool> accepting{false}, requested{false};
int64_t deadline = 0, cooldown_until = 0, marco_at = 0;
int64_t now() { return esp_timer_get_time(); }
void discovery(bool enabled) {
    if (enabled != discovery_enabled) {
        discovery_enabled = enabled;
        radio::set_peer_discovery(enabled);
    }
}
const catalog::Asset *find(const char *path) {
    if (!assets || !path) return nullptr;
    for (size_t i = 0; i < assets->count; ++i)
        if (!std::strcmp(path, assets->assets[i].path)) return &assets->assets[i];
    return nullptr;
}
bool begin(const catalog::Asset *next, bool pair) {
    if (!next || state != State::idle || !radio::stream_ready() || audio::busy()) return false;
    if (!audio::prepare(next)) return false;
    selected = next; coordinated = pair;
    accepting.store(false);
    discovery(!pair && !std::strcmp(next->path, "/audio/Marco.wav"));
    deadline = now() + 5000000;
    state = State::preparing;
    ESP_LOGI(tag, "Preparing %s [%s]", selected->path, pair ? "paired show" : "local/standalone");
    return true;
}
void abort(const char *reason) {
    accepting.store(false);
    greeting_pending = greeting_marco = false;
    ESP_LOGW(tag, "Playback canceled: %s; no automatic replay", reason);
    if (incoming_token) { secondary::respond(incoming_token, false); incoming_token = 0; }
    audio::cancel();
    state = State::cancelling;
}
}

void init(catalog::Catalog *content, bool is_primary) {
    assets = content; primary = is_primary;
    // Snapshot an already established connection; detect subsequent readiness
    // edges only after startup. On device init runs before radio connection.
    peer_was_ready = radio::peer_ready();
}
bool trigger() {
    if (!accepting.exchange(false)) return false;
    bool expected = false;
    return requested.compare_exchange_strong(expected, true);
}
bool local(const char *path) {
    if (!initialization_seen || requested.load() || greeting_pending) return false;
    return begin(find(path), false);
}
bool paired(const char *path) {
    const auto *next = find(path);
    if (!primary || !initialization_seen || !next || !next->skit || requested.load() || greeting_pending ||
        !accepting.exchange(false)) return false;
    if (!radio::peer_ready() || now() < cooldown_until || !begin(next, true)) return false;
    actuators::diagnostic(1);
    return true;
}
bool busy() {
    // A dormant greeting cannot make progress without the speaker. Leave the
    // idle controller available to the console's connection-retry command;
    // greeting_pending still gates local/paired/Home playback until recovery.
    return state != State::idle || requested.load() || (greeting_pending && radio::stream_ready());
}
const char *state_name() {
    switch (state) {
    case State::idle: return "idle";
    case State::preparing: return "preparing";
    case State::awaiting_peer: return "awaiting-peer";
    case State::greeting_tail: return "greeting-tail";
    case State::awaiting_polo: return "awaiting-polo";
    case State::playing: return "playing";
    case State::cancelling: return "canceling";
    }
    return "invalid";
}
void tick(bool initialization_finished, bool startup_active) {
    const int64_t time = now();
    if (!primary) {
        secondary::busy(!initialization_finished || startup_active || state != State::idle || audio::busy());
        secondary::poll();
        secondary::advertising(initialization_finished);
    }
    if (!initialization_seen && initialization_finished) {
        initialization_seen = true;
        cooldown_until = time + 10000000;
        ESP_LOGI(tag, "Initialization finished; direct peer discovery enabled while idle");
    }
    if (!initialization_seen || startup_active) {
        accepting.store(false); discovery(false); return;
    }
    if (primary) {
        const bool peer = radio::peer_ready();
        if (peer && !peer_was_ready) {
            greeting_pending = true;
            // A Marco already in flight during discovery is the audible first
            // half of this same greeting; never restart its PCM on connection.
            greeting_marco = (state == State::preparing || state == State::playing) &&
                selected && !coordinated && !std::strcmp(selected->path, "/audio/Marco.wav");
            accepting.store(false);
            ESP_LOGI(tag, "Jeff ready; greeting requires audible Marco before requesting Polo");
        } else if (!peer && peer_was_ready) {
            greeting_pending = false;
            if ((greeting_marco && state == State::preparing) || state == State::greeting_tail ||
                state == State::awaiting_polo)
                abort("Jeff disconnected during greeting");
            // Once PCM started, finish locally as for legacy skits; no Polo
            // request or stale greeting is retained across this disconnection.
            greeting_marco = false;
        }
        peer_was_ready = peer;
    }
    if (!primary) {
        // Connecting alone is silent. Polo travels through the same explicit
        // filename/prebuffer/ACK/start path as every other legacy request.
        secondary::take_connected();
        secondary::Request incoming{};
        if (secondary::take_request(incoming)) {
            if (state == State::idle && radio::stream_ready() && begin(find(incoming.path), false)) {
                incoming_token = incoming.token;
                deadline = time + 2500000; // Below the server's 3 s and primary's 5 s bounds.
            } else secondary::respond(incoming.token, false);
        }
    }
    if (state != State::idle && state != State::cancelling &&
        (!radio::stream_ready() || audio::error())) abort("speaker unavailable or SD/PCM failure");
    if (state == State::preparing) {
        if (audio::ready()) {
            if (coordinated) {
                radio::clear_peer_result();
                if (!radio::peer_ready() || radio::request_file(selected->path) != ESP_OK)
                    abort("Jeff unavailable before request");
                else { state = State::awaiting_peer; deadline = time + 6000000; }
            } else if (incoming_token) {
                if (!secondary::connected() || !secondary::respond(incoming_token, true))
                    abort("secondary request expired/disconnected or ACK could not queue");
                else {
                    incoming_token = 0;
                    if (audio::start()) {
                        state = State::playing;
                        ESP_LOGI(tag, "Secondary ACK queued; playing %s", selected->path);
                    } else abort("secondary prepared audio could not start");
                }
            } else if (audio::start()) {
                state = State::playing;
                ESP_LOGI(tag, "Playing %s", selected->path);
            } else abort("prepared audio could not start");
        } else if (time >= deadline) abort("local prebuffer timeout");
    } else if (state == State::awaiting_peer) {
        const auto reply = radio::peer_result();
        if (reply == radio::PeerResult::accepted) {
            if (!radio::peer_ready()) abort("Jeff disconnected after ACK");
            else if (audio::start()) {
                state = State::playing;
                ESP_LOGI(tag, "Jeff ACK/readback accepted; local playback %s (not proof of Jeff sound)", selected->path);
            } else abort("local start failed after peer reply");
        } else if (reply != radio::PeerResult::pending || time >= deadline)
            abort("Jeff rejected/disconnected/timed out");
    } else if (state == State::greeting_tail && time >= deadline) {
        greeting_pending = false;
        if (!radio::peer_ready() || radio::request_file("/audio/Polo.wav") != ESP_OK)
            abort("Polo request unavailable after Marco");
        else {
            state = State::awaiting_polo;
            deadline = time + 6000000;
            ESP_LOGI(tag, "Marco PCM complete plus 300 ms tail guard; requesting /audio/Polo.wav from Jeff");
        }
    } else if (state == State::awaiting_polo) {
        const auto reply = radio::peer_result();
        if (reply == radio::PeerResult::accepted && radio::peer_ready()) {
            ESP_LOGI(tag, "Polo ACK/readback accepted after Marco (not proof of Jeff sound)");
            radio::clear_peer_result();
            cooldown_until = time + 10000000;
            state = State::idle;
        } else if (reply != radio::PeerResult::pending || time >= deadline)
            abort("Polo rejected/disconnected/timed out");
    } else if (state == State::playing && audio::complete()) {
        const auto stats = audio::stats();
        ESP_LOGI(tag, "Playback complete: %s frames=%lu underruns=%lu", selected->path,
                 static_cast<unsigned long>(stats.frames), static_cast<unsigned long>(stats.underruns));
        cooldown_until = time + 10000000;
        radio::clear_peer_result();
        state = State::idle;
        if (greeting_marco) {
            greeting_marco = false;
            // PCM EOF precedes SBC/render queue drain. A bounded nonblocking
            // guard gives Marco's tail room before Polo, though JBL acoustic
            // latency is unmeasured and is not proven by this timer.
            state = State::greeting_tail;
            deadline = time + 300000;
        }
    } else if (state == State::cancelling && !audio::busy() &&
               radio::peer_result() != radio::PeerResult::pending) {
        radio::clear_peer_result();
        cooldown_until = time + 10000000;
        state = State::idle;
    }
    bool marco_due = false;
    if (primary && radio::peer_scanning()) {
        if (!marco_at) marco_at = time + 5000000;
        if (time >= marco_at) { marco_due = true; marco_at = time + 5000000; }
    } else marco_at = 0;
    if (state == State::idle) {
        discovery(primary);
        if (greeting_pending) {
            // Greetings precede Home/named triggers. A trigger racing the peer
            // edge is discarded, never deferred until after this exchange.
            requested.store(false); accepting.store(false);
            if (radio::stream_ready() && !audio::busy()) {
                if (begin(find("/audio/Marco.wav"), false)) greeting_marco = true;
                else {
                    greeting_pending = false;
                    cooldown_until = time + 10000000;
                    ESP_LOGW(tag, "Greeting unavailable: Marco missing or preparation failed; no replay");
                }
            }
            return;
        }
        const bool can_trigger = primary && assets && radio::stream_ready() && radio::peer_ready() &&
            !audio::busy() && time >= cooldown_until;
        accepting.store(can_trigger && !requested.load());
        if (requested.exchange(false)) {
            accepting.store(false);
            if (can_trigger) {
                const int index = catalog::select(*assets, static_cast<uint64_t>(time / 1000), esp_random());
                if (index >= 0 && begin(&assets->assets[index], true)) actuators::diagnostic(1);
                else ESP_LOGW(tag, "Trigger refused: no playable skit or prepare busy");
            } else ESP_LOGW(tag, "Trigger consumed: busy/cooldown/speaker/peer unavailable");
        } else if (marco_due && radio::stream_ready()) {
            begin(find("/audio/Marco.wav"), false);
        }
    } else {
        accepting.store(false);
        discovery(primary && selected && !coordinated && !std::strcmp(selected->path, "/audio/Marco.wav"));
    }
}
}
