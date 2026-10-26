#pragma once
namespace skull::faultlog {
// After nvs_flash_init, before storage mount. Bounded PSRAM allocation; failure
// leaves normal serial logging operational. No partition/NVS erasure.
void init();
// Sole control owner. idle must exclude audio/startup/show/tone/printer work.
// No remount/unmount; retry an unavailable mounted card once per 60 seconds.
void poll(bool idle,bool card_mounted);
void status();
void dump();
// Idle CLI fixture, exercises production capture/persistence without corruption.
void test_fault();
}
