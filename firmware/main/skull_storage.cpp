#include "skull_storage.h"
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <dirent.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#ifdef DEATH_SD_SPI_TRANSPORT
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#endif
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#if defined(ESP_PLATFORM) && !defined(DEATH_SD_SPI_TRANSPORT)
#include "ff.h"
#include "diskio_sdmmc.h"
#include "esp_heap_caps.h"
#include "death_audio_reference.h"
#endif

#ifndef SKULL_STORAGE_ROOT
#define SKULL_STORAGE_ROOT "/sdcard"
#endif

namespace skull::storage {
namespace {
constexpr char tag[] = "skull_sd";
bool is_mounted = false;
sdmmc_card_t *mounted_card = nullptr;
#if defined(ESP_PLATFORM) && !defined(DEATH_SD_SPI_TRANSPORT)
esp_err_t (*original_transaction)(int, sdmmc_command_t *) = nullptr;
uint32_t probe_r1_read = 0, probe_r1_status = 0;
unsigned probe_reads = 0, probe_statuses = 0, probe_transport_errors = 0, probe_command_errors = 0;
esp_err_t record_transaction(int slot, sdmmc_command_t *command) {
    const esp_err_t result = original_transaction(slot, command);
    if (command->opcode == 17 || command->opcode == 18) { ++probe_reads; probe_r1_read |= command->response[0]; }
    if (command->opcode == 13) { ++probe_statuses; probe_r1_status |= command->response[0]; }
    probe_transport_errors += result != ESP_OK;
    probe_command_errors += command->error != ESP_OK;
    return result;
}
#endif


}

bool mounted() { return is_mounted; }
bool spi_mode() {
#ifdef DEATH_SD_SPI_TRANSPORT
    return true;
#else
    return false;
#endif
}
bool probe_raw_reference() {
#ifdef ESP_PLATFORM
#ifdef DEATH_SD_SPI_TRANSPORT
    // This diagnostic wraps the native SDMMC host transaction callback.
    return false;
#else
    if (!is_mounted || !mounted_card || mounted_card->csd.sector_size != 512) return false;
    const unsigned drive = ff_diskio_get_pdrv_card(mounted_card);
    if (drive >= FF_VOLUMES) return false;
    char path[48]; std::snprintf(path, sizeof(path), "%u:/audio/initialized.wav", drive);
    // FIL embeds FF_MAX_SS scratch (4096 bytes in this build); keep it off
    // the diagnostic task stack, including nested filesystem/driver calls.
    struct FileMemory { FIL *value; ~FileMemory() { heap_caps_free(value); } };
    FileMemory memory{static_cast<FIL *>(heap_caps_malloc(sizeof(FIL), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))};
    if (!memory.value) return false;
    std::memset(memory.value, 0, sizeof(FIL));
    FIL &file = *memory.value;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    const bool seek = f_lseek(&file, skull::startup::reference_offset + 1) == FR_OK &&
        f_tell(&file) == skull::startup::reference_offset + 1;
    const FATFS *fs = file.obj.fs;
    uint64_t sector = 0;
    bool valid = seek && fs && fs->pdrv == drive && fs->csize && file.clust >= 2 && file.clust < fs->n_fatent;
    if (valid) {
        const unsigned within = (skull::startup::reference_offset / 512) % fs->csize;
        valid = within + 8 <= fs->csize;
        sector = fs->database + static_cast<uint64_t>(file.clust - 2) * fs->csize + within;
        valid = valid && sector + 8 <= mounted_card->csd.capacity && file.sect == sector;
        ESP_LOGI(tag, "SDRAW geometry FAT=%u cluster_sectors=%u cluster=%lu database=%llu sector=%llu", fs->fs_type, fs->csize,
                 static_cast<unsigned long>(file.clust), static_cast<unsigned long long>(fs->database), static_cast<unsigned long long>(sector));
    }
    f_close(&file);
    if (!valid) return false;
    auto *data = static_cast<uint8_t *>(heap_caps_malloc(sizeof(skull::startup::reference_window), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!data) return false;
    bool exact = true;
    original_transaction = mounted_card->host.do_transaction;
    mounted_card->host.do_transaction = record_transaction;
    for (unsigned pass = 0; pass < 4; ++pass) {
        probe_r1_read = probe_r1_status = 0;
        probe_reads = probe_statuses = probe_transport_errors = probe_command_errors = 0;
        esp_err_t result = ESP_OK;
        if (pass < 2) result = sdmmc_read_sectors(mounted_card, data, static_cast<size_t>(sector), 8);
        else for (unsigned i = 0; i < 8 && result == ESP_OK; ++i)
            result = sdmmc_read_sectors(mounted_card, data + 512 * i, static_cast<size_t>(sector + i), 1);
        unsigned mismatches = 0, bits = 0, first = sizeof(skull::startup::reference_window);
        uint32_t hash = 2166136261u;
        unsigned bit_counts[8]{};
        if (result == ESP_OK) for (unsigned i = 0; i < sizeof(skull::startup::reference_window); ++i) {
            hash = (hash ^ data[i]) * 16777619u;
            const unsigned difference = data[i] ^ skull::startup::reference_window[i];
            if (difference) {
                if (first == sizeof(skull::startup::reference_window)) first = i;
                ++mismatches; bits += __builtin_popcount(difference);
                for (unsigned bit = 0; bit < 8; ++bit) bit_counts[bit] += (difference >> bit) & 1u;
            }
        }
        exact = exact && result == ESP_OK && mismatches == 0;
        ESP_LOGI(tag, "SDRAW R1 read=%08lx status=%08lx reads=%u statuses=%u transport_errors=%u command_errors=%u",
                 static_cast<unsigned long>(probe_r1_read), static_cast<unsigned long>(probe_r1_status),
                 probe_reads, probe_statuses, probe_transport_errors, probe_command_errors);
        ESP_LOGI(tag, "SDRAW pass=%u sectors_per_command=%u result=%s hash=%08lx mismatch=%u bits=%u first_offset=%lu", pass+1, pass<2?8:1,
                 esp_err_to_name(result), static_cast<unsigned long>(hash), mismatches,bits,
                 static_cast<unsigned long>(first < sizeof(skull::startup::reference_window) ? skull::startup::reference_offset+first : 0));
        ESP_LOGI(tag, "SDRAW xor_bits=%u,%u,%u,%u,%u,%u,%u,%u",bit_counts[0],bit_counts[1],bit_counts[2],bit_counts[3],bit_counts[4],bit_counts[5],bit_counts[6],bit_counts[7]);
    }
    mounted_card->host.do_transaction = original_transaction;
    original_transaction = nullptr;
    heap_caps_free(data);
    return exact;
#endif
#else
    return false;
#endif
}

esp_err_t unmount_for_retry() {
    if (!is_mounted) return ESP_OK;
    const esp_err_t result = esp_vfs_fat_sdcard_unmount(SKULL_STORAGE_ROOT, mounted_card);
    if (result == ESP_OK) {
        is_mounted = false;
        mounted_card = nullptr;
#ifdef DEATH_SD_SPI_TRANSPORT
        const esp_err_t bus_result = spi_bus_free(SDSPI_DEFAULT_HOST);
        if (bus_result != ESP_OK) return bus_result;
#endif
    }
    return result;
}

esp_err_t set_card_clock(unsigned khz,unsigned &actual_khz) {
    actual_khz=0;
    if(!is_mounted||!mounted_card)return ESP_FAIL;
#ifdef DEATH_SD_SPI_TRANSPORT
    (void)khz;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if(khz!=4000&&khz!=10000&&khz!=20000)return ESP_ERR_INVALID_ARG;
    const esp_err_t result=sdmmc_host_set_card_clk(mounted_card->host.slot,khz);
    if(result!=ESP_OK)return result;
    int actual=0;
    const esp_err_t measured=sdmmc_host_get_real_freq(mounted_card->host.slot,&actual);
    if(measured==ESP_OK&&actual>0)actual_khz=static_cast<unsigned>(actual);
    ESP_LOGI(tag,"SDCLK requested_khz=%u actual_khz=%u result=%s",khz,actual_khz,esp_err_to_name(measured));
    return measured;
#endif
}

esp_err_t mount_and_read_config(Config &config) {
    config = Config{};
    if (!is_mounted) {
#ifdef DEATH_SD_SPI_TRANSPORT
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = 8000;
        // Keep SD SPI CRC enabled; ESP-IDF's SPI card init sends CMD59.
        spi_bus_config_t bus{};
        bus.mosi_io_num = GPIO_NUM_15;
        bus.miso_io_num = GPIO_NUM_2;
        bus.sclk_io_num = GPIO_NUM_14;
        bus.quadwp_io_num = GPIO_NUM_NC;
        bus.quadhd_io_num = GPIO_NUM_NC;
        bus.max_transfer_sz = 4096;
        esp_err_t result = spi_bus_initialize(SDSPI_DEFAULT_HOST, &bus, SDSPI_DEFAULT_DMA);
        if (result != ESP_OK) {
            ESP_LOGE(tag, "Onboard SDSPI bus init failed: %s", esp_err_to_name(result));
            return result;
        }
        sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot.host_id = SDSPI_DEFAULT_HOST;
        slot.gpio_cs = GPIO_NUM_13;
        esp_vfs_fat_sdmmc_mount_config_t mount{};
        mount.format_if_mount_failed = false;
        mount.max_files = 5;
        mount.allocation_unit_size = 16 * 1024;
        result = esp_vfs_fat_sdspi_mount(SKULL_STORAGE_ROOT, &host, &slot, &mount, &mounted_card);
        if (result != ESP_OK) {
            ESP_LOGE(tag, "Onboard SDSPI transport comparison at 8MHz failed: %s (no format)", esp_err_to_name(result));
            spi_bus_free(SDSPI_DEFAULT_HOST);
            return result;
        }
        is_mounted = true;
        sdmmc_card_print_info(stdout, mounted_card);
        ESP_LOGI(tag, "Onboard SDSPI mounted at 8MHz, pins SCLK=14 MOSI=15 MISO=2 CS=13; CRC enabled, capacity=%llu MB",
                 static_cast<unsigned long long>(mounted_card->csd.capacity) * mounted_card->csd.sector_size / (1024 * 1024));
#else
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        // Controlled comparison: exact qualified TwoSkulls SD clock.
        host.max_freq_khz = SDMMC_FREQ_DEFAULT;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
        esp_vfs_fat_sdmmc_mount_config_t mount{};
        mount.format_if_mount_failed = false;
        mount.max_files = 5;
        mount.allocation_unit_size = 16 * 1024;
        const esp_err_t result = esp_vfs_fat_sdmmc_mount(SKULL_STORAGE_ROOT, &host, &slot, &mount, &mounted_card);
        if (result != ESP_OK) {
            ESP_LOGE(tag, "Onboard SDMMC 1-bit baseline clock mount failed: %s (no format)", esp_err_to_name(result));
            return result;
        }
        is_mounted = true;
        sdmmc_card_print_info(stdout, mounted_card);
        ESP_LOGI(tag, "Onboard SDMMC mounted, capacity=%llu MB",
                 static_cast<unsigned long long>(mounted_card->csd.capacity) * mounted_card->csd.sector_size / (1024 * 1024));
#endif
    }
    FILE *file = std::fopen(SKULL_STORAGE_ROOT "/config/config.txt", "r");
    if (!file) file = std::fopen(SKULL_STORAGE_ROOT "/config.txt", "r");
    if (!file) return ESP_ERR_NOT_FOUND;
    const bool valid = parse_config(file, config);
    std::fclose(file);
    if (!valid) {
        ESP_LOGE(tag, "Valid Death settings and speaker_name required in config/config.txt or config.txt");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(tag, "Card config role=%s speaker='%s' volume=%u%%",
             config.primary ? "primary" : "secondary", config.speaker_name, config.volume_percent);
    static bool inventory_logged = false;
    if (inventory_logged) return ESP_OK;
    DIR *directory = opendir(SKULL_STORAGE_ROOT "/audio");
    unsigned wav_count = 0, script_count = 0;
    if (directory) {
        while (const dirent *entry = readdir(directory)) {
            if (entry->d_name[0] == '.') continue; // macOS AppleDouble metadata is not an audio asset.
            const char *extension = std::strrchr(entry->d_name, '.');
            if (extension && !strcasecmp(extension, ".wav")) ++wav_count;
            if (extension && !strcasecmp(extension, ".txt")) ++script_count;
        }
        closedir(directory);
    }
    ESP_LOGI(tag, "Read-only content inventory: %u WAVs, %u scripts", wav_count, script_count);
    inventory_logged = true;
    return ESP_OK;
}
}
