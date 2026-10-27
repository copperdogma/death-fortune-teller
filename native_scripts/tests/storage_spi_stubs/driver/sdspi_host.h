#pragma once
#include "driver/spi_master.h"
constexpr spi_host_device_t SDSPI_DEFAULT_HOST = SPI2_HOST;
constexpr int SDSPI_DEFAULT_DMA = SPI_DMA_CH_AUTO;
struct sdspi_device_config_t {
    spi_host_device_t host_id = SDSPI_DEFAULT_HOST;
    int gpio_cs = GPIO_NUM_NC;
};
#define SDSPI_HOST_DEFAULT() sdmmc_host_t{20000}
#define SDSPI_DEVICE_CONFIG_DEFAULT() sdspi_device_config_t{}
