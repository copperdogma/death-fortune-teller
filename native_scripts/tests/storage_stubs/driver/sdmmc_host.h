#pragma once
#include "../esp_err.h"
struct sdmmc_host_t { int max_freq_khz; };
struct sdmmc_slot_config_t { int width; unsigned flags; };
#define SDMMC_HOST_DEFAULT() sdmmc_host_t{}
#define SDMMC_SLOT_CONFIG_DEFAULT() sdmmc_slot_config_t{}
constexpr unsigned SDMMC_SLOT_FLAG_INTERNAL_PULLUP = 1;

constexpr int SDMMC_FREQ_DEFAULT = 20000;
esp_err_t sdmmc_host_set_card_clk(int,unsigned);
esp_err_t sdmmc_host_get_real_freq(int,int *);
