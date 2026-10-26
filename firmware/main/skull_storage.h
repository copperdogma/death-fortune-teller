#pragma once
#include "esp_err.h"
#include "skull_config.h"

namespace skull::storage {
// Mounts the onboard slot read/write at the VFS level, but this application
// only opens existing files for reading and never formats on a mount failure.
esp_err_t mount_and_read_config(Config &config);
bool mounted();
// Idle-only read-only driver isolation against supplied initialized.wav reference.
bool probe_raw_reference();
// Boot retry only, before any audio file/producer exists. Never formats.
esp_err_t unmount_for_retry();
}
