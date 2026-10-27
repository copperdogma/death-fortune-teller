#pragma once
#include "esp_err.h"
#include "skull_config.h"

namespace skull::storage {
// Mounts the onboard slot read/write at the VFS level, but this application
// only opens existing files for reading and never formats on a mount failure.
esp_err_t mount_and_read_config(Config &config);
bool mounted();
// True only in firmware compiled with DEATH_SD_SPI_TRANSPORT=ON.
bool spi_mode();
// Control-owner only, with every SD/audio/printer owner quiescent. Volatile
// diagnostic setting; boot remains at its configured baseline.
esp_err_t set_card_clock(unsigned khz, unsigned &actual_khz);
// Idle-only read-only driver isolation against supplied initialized.wav reference.
bool probe_raw_reference();
// Boot retry only, before any audio file/producer exists. Never formats.
esp_err_t unmount_for_retry();
}
