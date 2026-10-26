#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <cstddef>
#include <cstdint>
using uart_port_t = int;
constexpr uart_port_t UART_NUM_1 = 1;
constexpr int UART_DATA_8_BITS = 8;
constexpr int UART_PARITY_DISABLE = 0;
constexpr int UART_STOP_BITS_1 = 1;
constexpr int UART_HW_FLOWCTRL_DISABLE = 0;
constexpr int UART_SCLK_DEFAULT = 0;
constexpr int UART_PIN_NO_CHANGE = -1;
struct uart_config_t { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; };
esp_err_t uart_driver_install(uart_port_t, int, int, int, void *, int);
esp_err_t uart_param_config(uart_port_t, const uart_config_t *);
esp_err_t uart_set_pin(uart_port_t, int, int, int, int);
esp_err_t uart_driver_delete(uart_port_t);
int uart_write_bytes(uart_port_t, const void *, size_t);
esp_err_t uart_wait_tx_done(uart_port_t, TickType_t);
