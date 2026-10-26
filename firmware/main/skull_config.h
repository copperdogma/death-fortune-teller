#pragma once
#include <cstdio>
#include <cstdint>

namespace skull::storage {
struct Config {
    char speaker_name[80]{};
    char secondary_mac[18]{}; // Retained only for source compatibility; Death has no peer.
    bool primary = true; // Death is always the only application controller.
    unsigned volume_percent = 100;
    unsigned servo_us_min = 1400, servo_us_max = 1600;
    bool servo_reverse = false;
    uint32_t finger_wait_ms = 6000;
    uint32_t snap_delay_min_ms = 1000, snap_delay_max_ms = 3000, cooldown_ms = 12000;
    unsigned printer_baud = 9600;
    char fortunes_json[128] = "/printer/fortunes_littlekid.json";
};
// Read-only Death config: speaker required, role unnecessary, unknown legacy
// keys ignored. Failure resets output to safe defaults. Inline # comments accepted.
bool parse_config(FILE *file, Config &config);
}
