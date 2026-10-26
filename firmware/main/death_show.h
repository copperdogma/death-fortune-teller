#pragma once
#include <cstdint>
namespace skull::catalog {struct Catalog;}
namespace skull::death {
struct Config {
    uint32_t cooldown_ms=12000;
    const char *fortunes_path="/sdcard/printer/fortunes_littlekid.json";
};
// Boot/control-task only. Assets remain valid for application lifetime.
void init(catalog::Catalog*, const Config &settings=Config{});
// Thread-safe, one pending command; rejected commands are never replayed later.
bool trigger();
// Dead Stop is momentary: mode becomes Waiting immediately; Start stays gated
// until cancellation ownership is quiescent. Automatic failures retain Start
// through cleanup and cooldown; only explicit Dead Stop bypasses cooldown.
enum class Mode {waiting,start,dead_stop};
Mode mode();
void cancel();
// Control-task only. All SD work stays outside Bluetooth/Matter callbacks.
void tick(bool initialization_finished,bool startup_active);
bool local(const char *path);
bool busy();
const char *state_name();
// Control-task snapshot, retained across later shows until reboot/init. No SD writes.
struct Interruption {
    uint32_t count=0;
    int64_t uptime_us=0;
    const char *reason="none";
    const char *phase="none";
    const char *asset="none";
};
Interruption last_interruption();
// -1 audio/breathing, 0 closed; caller applies calibrated pulse limits.
int jaw_override();
}
