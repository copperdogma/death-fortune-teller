#pragma once
#include "driver/gpio.h"
using spi_host_device_t = int;
constexpr spi_host_device_t SPI2_HOST = 2;
constexpr int SPI_DMA_CH_AUTO = 0;
struct spi_bus_config_t {
    int mosi_io_num = GPIO_NUM_NC;
    int miso_io_num = GPIO_NUM_NC;
    int sclk_io_num = GPIO_NUM_NC;
    int quadwp_io_num = GPIO_NUM_NC;
    int quadhd_io_num = GPIO_NUM_NC;
    int max_transfer_sz = 0;
};
esp_err_t spi_bus_initialize(spi_host_device_t, const spi_bus_config_t *, int);
esp_err_t spi_bus_free(spi_host_device_t);
