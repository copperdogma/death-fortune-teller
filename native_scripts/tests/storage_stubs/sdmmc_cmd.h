#pragma once
#include <cstdint>
struct sdmmc_card_t { struct { uint32_t capacity, sector_size; } csd; };
#include <cstdio>
constexpr int SDMMC_FREQ_HIGHSPEED = 40000;
inline void sdmmc_card_print_info(FILE *, const sdmmc_card_t *) {}
