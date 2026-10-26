#include "skull_storage.h"
#include "esp_vfs_fat.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cstdio>
#ifndef SKULL_STORAGE_ROOT
#error Compile this host test with SKULL_STORAGE_ROOT set to an isolated temporary directory
#endif
namespace fs = std::filesystem;
namespace {
int mounts = 0, unmounts = 0;
esp_err_t mount_result = ESP_FAIL, unmount_result = ESP_OK;
sdmmc_card_t fake_card{{4096, 512}};
void defaults(const skull::storage::Config &config) {
    assert(!*config.speaker_name && !*config.secondary_mac);
    assert(config.primary && config.volume_percent == 100);
}
void write_config(const char *text) { std::ofstream(SKULL_STORAGE_ROOT "/config.txt") << text; }
}
esp_err_t esp_vfs_fat_sdmmc_mount(const char *root, const sdmmc_host_t *host,
        const sdmmc_slot_config_t *slot, const esp_vfs_fat_sdmmc_mount_config_t *options,
        sdmmc_card_t **card) {
    assert(!std::strcmp(root, SKULL_STORAGE_ROOT));
    assert(!options->format_if_mount_failed);
    assert(host->max_freq_khz == 20000 && slot->width == 1);
    ++mounts;
    if (mount_result == ESP_OK) *card = &fake_card;
    return mount_result;
}
esp_err_t esp_vfs_fat_sdcard_unmount(const char *root, sdmmc_card_t *card) {
    assert(!std::strcmp(root, SKULL_STORAGE_ROOT) && card == &fake_card);
    ++unmounts;
    return unmount_result;
}
int main() {
    using namespace skull::storage;
    fs::create_directories(SKULL_STORAGE_ROOT "/audio");
    Config config;
    std::strcpy(config.speaker_name, "stale"); config.volume_percent = 2;
    assert(mount_and_read_config(config) == ESP_FAIL && !mounted()); defaults(config);
    assert(mounts == 1);
    assert(unmount_for_retry() == ESP_OK && unmounts == 0);
    mount_result = ESP_OK;
    assert(mount_and_read_config(config) == ESP_ERR_NOT_FOUND && mounted()); defaults(config);
    assert(mounts == 2);
    write_config("role=wrong\nspeaker_name=Anthony\nspeaker_volume=20\n");
    assert(mount_and_read_config(config) == ESP_ERR_INVALID_ARG && mounted()); defaults(config);
    assert(mounts == 2);
    write_config("role=primary\nspeaker_name=Anthony\nspeaker_volume=20\nsecondary_mac_address=12:34:56:78:9a:bc\n");
    assert(mount_and_read_config(config) == ESP_OK && mounted());
    assert(config.primary && config.volume_percent == 20 && *config.secondary_mac);
    assert(mounts == 2);
    write_config("role=primary\n");
    assert(mount_and_read_config(config) == ESP_ERR_INVALID_ARG); defaults(config);
    write_config("speaker_name=Death\n");
    assert(mount_and_read_config(config) == ESP_OK);
    assert(config.primary && config.volume_percent == 100 && !*config.secondary_mac);
    assert(mounts == 2);
    // The documented Death /config/config.txt takes priority over root legacy config.
    fs::create_directories(SKULL_STORAGE_ROOT "/config");
    std::ofstream(SKULL_STORAGE_ROOT "/config/config.txt") << "speaker_name=Nested Death\n";
    assert(mount_and_read_config(config) == ESP_OK);
    assert(!std::strcmp(config.speaker_name, "Nested Death"));
    // An invalid preferred file must not silently fall back to a valid old root file.
    std::ofstream(SKULL_STORAGE_ROOT "/config/config.txt") << "speaker_name=\n";
    assert(mount_and_read_config(config) == ESP_ERR_INVALID_ARG); defaults(config);
    fs::remove(SKULL_STORAGE_ROOT "/config/config.txt");
    unmount_result = ESP_FAIL;
    assert(unmount_for_retry() == ESP_FAIL && mounted());
    assert(mount_and_read_config(config) == ESP_OK && mounts == 2);
    unmount_result = ESP_OK;
    assert(unmount_for_retry() == ESP_OK && !mounted());
    assert(unmounts == 2);
    assert(mount_and_read_config(config) == ESP_OK && mounted() && mounts == 3);
    assert(unmount_for_retry() == ESP_OK && !mounted());
    std::puts("storage recovery tests passed");
}
