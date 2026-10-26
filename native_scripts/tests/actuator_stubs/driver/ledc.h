#pragma once
#include <cstdint>
#include "esp_err.h"
enum ledc_mode_t { LEDC_LOW_SPEED_MODE };
enum ledc_channel_t { LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2 };
enum ledc_timer_t { LEDC_TIMER_0, LEDC_TIMER_1 };
enum ledc_timer_bit_t { LEDC_TIMER_8_BIT = 8, LEDC_TIMER_16_BIT = 16 };
enum ledc_clk_cfg_t { LEDC_USE_APB_CLK };
enum ledc_intr_type_t { LEDC_INTR_DISABLE };
struct ledc_timer_config_t {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
};
struct ledc_channel_config_t {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_channel_t channel;
    ledc_intr_type_t intr_type;
    ledc_timer_t timer_sel;
    uint32_t duty;
    int hpoint;
};
esp_err_t ledc_timer_config(const ledc_timer_config_t *);
esp_err_t ledc_channel_config(const ledc_channel_config_t *);
esp_err_t ledc_set_duty(ledc_mode_t, ledc_channel_t, uint32_t);
esp_err_t ledc_update_duty(ledc_mode_t, ledc_channel_t);
