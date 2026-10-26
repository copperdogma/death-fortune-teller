#include "skull_config.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using skull::storage::Config;
using skull::storage::parse_config;
bool parse(const std::string &text, Config &config) {
    FILE *file = std::tmpfile(); assert(file);
    assert(std::fwrite(text.data(), 1, text.size(), file) == text.size());
    std::rewind(file); bool result = parse_config(file, config); std::fclose(file); return result;
}
void defaults(const Config &config) {
    assert(!*config.speaker_name && !*config.secondary_mac);
    assert(config.primary && config.volume_percent == 100);
    assert(config.servo_us_min == 1400 && config.servo_us_max == 1600 && !config.servo_reverse);
    assert(config.finger_wait_ms == 6000 && config.cooldown_ms == 12000);
}
int main() {
    Config config; defaults(config);
    assert(parse("speaker_name=JBL\n", config)); assert(config.primary);
    assert(parse("\xef\xbb\xbf# comment\r\n role=primary\r\n speaker_name = JBL Flip 5 \r\nspeaker_volume=80\nignored_future_key = yes\n", config));
    assert(config.primary && !std::strcmp(config.speaker_name, "JBL Flip 5") && config.volume_percent == 80);
    assert(parse("speaker_name=JBL\nservo_us_min=900\nservo_us_max=2200\nservo_reverse=true\ncap_threshold=0.10   # legacy sensor setting\nfinger_cycles_init=0xFFFF\nsnap_delay_min_ms=0\nsnap_delay_max_ms=10\nfortunes_json=/fortunes/little_kid_fortunes.json\n", config));
    assert(config.servo_us_min == 900 && config.servo_us_max == 2200 && config.servo_reverse);
    assert(config.snap_delay_min_ms == 0 && config.snap_delay_max_ms == 10);
    for (const char *bad : {"role=secondary", "role=unknown", "speaker_volume=101", "speaker_volume=-1", "speaker_volume=4294967296000000", "servo_us_min=2200\nservo_us_max=900", "servo_us_min=499", "servo_us_max=2501", "servo_reverse=1", "snap_delay_min_ms=3001\nsnap_delay_max_ms=3000", "finger_wait_ms=300001", "fortunes_json=/../bad", "fortunes_json=relative", "printer_baud=19200"}) {
        assert(!parse(std::string("speaker_name=JBL\n") + bad + "\n", config)); defaults(config);
    }
    assert(!parse("role=primary\n", config)); defaults(config);
    assert(!parse("speaker_name= \n", config)); defaults(config);
    assert(!parse(std::string("speaker_name=") + std::string(80,'a') + "\n", config)); defaults(config);
    assert(!parse_config(nullptr, config)); defaults(config);
    FILE *actual = std::fopen("sd-card-files/config.txt", "r"); assert(actual);
    assert(parse_config(actual, config)); std::fclose(actual);
    assert(config.servo_us_min == 900 && config.servo_us_max == 2200 && !config.servo_reverse);
    std::puts("Death config tests passed");
}
