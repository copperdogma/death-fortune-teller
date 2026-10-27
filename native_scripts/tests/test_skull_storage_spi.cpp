#include "skull_storage.h"
#include "esp_vfs_fat.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#ifndef SKULL_STORAGE_ROOT
#error Compile with SKULL_STORAGE_ROOT set to a temporary directory
#endif

namespace fs = std::filesystem;
namespace {
sdmmc_card_t fake_card{{4096, 512}, {2}};
esp_err_t bus_init_result = ESP_OK, bus_free_result = ESP_OK;
esp_err_t mount_result = ESP_OK, unmount_result = ESP_OK;
int bus_init_calls = 0, bus_free_calls = 0, mount_calls = 0, unmount_calls = 0;
spi_bus_config_t seen_bus{};
sdmmc_host_t seen_host{};
sdspi_device_config_t seen_slot{};
esp_vfs_fat_sdmmc_mount_config_t seen_mount{};
void write_config() {
    fs::create_directories(SKULL_STORAGE_ROOT "/config");
    std::ofstream(SKULL_STORAGE_ROOT "/config/config.txt") << "speaker_name=SPI Test\n";
}
}

esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *config, int dma) {
    assert(host == SPI2_HOST && dma == SPI_DMA_CH_AUTO);
    ++bus_init_calls;
    seen_bus = *config;
    return bus_init_result;
}
esp_err_t spi_bus_free(spi_host_device_t host) {
    assert(host == SPI2_HOST);
    ++bus_free_calls;
    return bus_free_result;
}
esp_err_t esp_vfs_fat_sdspi_mount(const char *root, const sdmmc_host_t *host,
        const sdspi_device_config_t *slot, const esp_vfs_fat_sdmmc_mount_config_t *mount,
        sdmmc_card_t **card) {
    assert(!std::strcmp(root, SKULL_STORAGE_ROOT));
    ++mount_calls;
    seen_host = *host; seen_slot = *slot; seen_mount = *mount;
    if (mount_result == ESP_OK) *card = &fake_card;
    return mount_result;
}
esp_err_t esp_vfs_fat_sdmmc_mount(const char *, const sdmmc_host_t *, const sdmmc_slot_config_t *,
        const esp_vfs_fat_sdmmc_mount_config_t *, sdmmc_card_t **) {
    assert(false && "SPI build must not use SDMMC mount");
    return ESP_FAIL;
}
esp_err_t esp_vfs_fat_sdcard_unmount(const char *root, sdmmc_card_t *card) {
    assert(!std::strcmp(root, SKULL_STORAGE_ROOT) && card == &fake_card);
    ++unmount_calls;
    return unmount_result;
}

int main() {
    using namespace skull::storage;
    fs::create_directories(SKULL_STORAGE_ROOT);
    write_config();
    assert(spi_mode());
    Config config{};

    bus_init_result = ESP_FAIL;
    assert(mount_and_read_config(config) == ESP_FAIL && !mounted());
    assert(bus_init_calls == 1 && mount_calls == 0 && bus_free_calls == 0);

    bus_init_result = ESP_OK;
    mount_result = ESP_FAIL;
    assert(mount_and_read_config(config) == ESP_FAIL && !mounted());
    assert(bus_init_calls == 2 && mount_calls == 1 && bus_free_calls == 1);
    assert(seen_host.max_freq_khz == 8000);
    assert(seen_bus.sclk_io_num == 14 && seen_bus.mosi_io_num == 15 && seen_bus.miso_io_num == 2);
    assert(seen_slot.host_id == SPI2_HOST && seen_slot.gpio_cs == 13);
    assert(!seen_mount.format_if_mount_failed);

    mount_result = ESP_OK;
    assert(mount_and_read_config(config) == ESP_OK && mounted());
    assert(bus_init_calls == 3 && mount_calls == 2);
    assert(config.speaker_name[0] && !std::strcmp(config.speaker_name, "SPI Test"));
    unsigned actual = 999;
    assert(set_card_clock(20000, actual) == ESP_ERR_NOT_SUPPORTED && actual == 0);
    assert(!probe_raw_reference());

    unmount_result = ESP_FAIL;
    assert(unmount_for_retry() == ESP_FAIL && mounted());
    assert(unmount_calls == 1 && bus_free_calls == 1);
    unmount_result = ESP_OK;
    assert(unmount_for_retry() == ESP_OK && !mounted());
    assert(unmount_calls == 2 && bus_free_calls == 2);
    std::puts("SPI storage lifecycle tests passed");
}
