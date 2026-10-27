#include "skull_pwm_probe.h"
#include "skull_pwm_analysis.h"

#include <algorithm>
#include <new>
#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"

namespace skull::pwm_probe {
namespace {
constexpr char tag[] = "skull_pwm_probe";
constexpr int64_t duration_us = 8000000;
constexpr size_t ring_size = 64, history_size = 1024, command_size = 512;
struct Command { int64_t cpu_us; uint32_t duty; };
struct Context {
    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    analysis::Edge ring[ring_size]{};
    uint32_t head = 0, tail = 0, dropped = 0;
    analysis::Edge *history = nullptr;
    Command *commands = nullptr;
    size_t samples = 0, command_count = 0;
    bool truncated = false, cancelled = false, finishing = false, cleanup_error_logged = false, have_gpio = false;
    int64_t started = 0;
    uint32_t resolution = 0;
    gpio_io_config_t original_gpio{};
};
Context *context = nullptr;
mcpwm_cap_timer_handle_t timer = nullptr;
mcpwm_cap_channel_handle_t channels[2]{};
bool timer_enabled = false, timer_started = false, channel_enabled[2]{};

bool captured(mcpwm_cap_channel_handle_t, const mcpwm_capture_event_data_t *data, void *user)
{
    auto *ctx = static_cast<Context *>(user);
    const analysis::Edge edge{data->cap_value, data->cap_edge == MCPWM_CAP_EDGE_POS,
                              esp_timer_get_time()};
    portENTER_CRITICAL_ISR(&ctx->lock);
    if (ctx->head - ctx->tail < ring_size) ctx->ring[ctx->head++ % ring_size] = edge;
    else ++ctx->dropped;
    portEXIT_CRITICAL_ISR(&ctx->lock);
    return false;
}

void drain()
{
    for (;;) {
        analysis::Edge edge{};
        portENTER_CRITICAL(&context->lock);
        const bool available = context->tail != context->head;
        if (available) edge = context->ring[context->tail++ % ring_size];
        portEXIT_CRITICAL(&context->lock);
        if (!available) break;
        if (context->samples < history_size) context->history[context->samples++] = edge;
        else context->truncated = true;
    }
}

bool cleanup()
{
    auto checked = [](esp_err_t error) {
        if (error == ESP_OK) return true;
        if (!context->cleanup_error_logged) ESP_LOGE(tag, "Capture cleanup deferred: %s", esp_err_to_name(error));
        context->cleanup_error_logged = true;
        return false;
    };
    // Disable/free all interrupt sources before releasing callback-owned memory.
    for (int i = 0; i < 2; ++i) if (channel_enabled[i]) {
        if (!checked(mcpwm_capture_channel_disable(channels[i]))) return false;
        channel_enabled[i] = false;
    }
    if (timer_started) {
        if (!checked(mcpwm_capture_timer_stop(timer))) return false;
        timer_started = false;
    }
    for (auto &channel : channels) if (channel) {
        if (!checked(mcpwm_del_capture_channel(channel))) return false;
        channel = nullptr;
    }
    if (timer_enabled) {
        if (!checked(mcpwm_capture_timer_disable(timer))) return false;
        timer_enabled = false;
    }
    if (timer) {
        if (!checked(mcpwm_del_capture_timer(timer))) return false;
        timer = nullptr;
    }
    return true;
}

void finish()
{
    if (!cleanup()) return; // Preserve context until interrupt teardown succeeds.
    drain();
    // The public gpio_input_disable wrapper is private to the GPIO driver.
    // Its LL helper changes only the input-buffer-enable bit, not LEDC routing.
    if (context->have_gpio && !context->original_gpio.ie)
        gpio_ll_input_disable(&GPIO, GPIO_NUM_23);
    gpio_io_config_t after{};
    const bool got_gpio = context->have_gpio && gpio_get_io_config(GPIO_NUM_23, &after) == ESP_OK;
    const auto &before = context->original_gpio;
    const bool output_unchanged = got_gpio && before.fun_sel == after.fun_sel &&
        before.sig_out == after.sig_out && before.oe == after.oe && before.drv == after.drv &&
        before.od == after.od && before.oe_ctrl_by_periph == after.oe_ctrl_by_periph &&
        before.oe_inv == after.oe_inv && before.pu == after.pu && before.pd == after.pd && before.ie == after.ie;
    ESP_LOGI(tag, "PWM_CAPTURE hz=%lu edges=%u ring_dropped=%lu history_truncated=%d cancelled=%d output_unchanged=%d",
             static_cast<unsigned long>(context->resolution), static_cast<unsigned>(context->samples),
             static_cast<unsigned long>(context->dropped), context->truncated, context->cancelled, output_unchanged);
    if (context->have_gpio && !output_unchanged) ESP_LOGE(tag, "GPIO23 output configuration comparison failed; no output writes performed by probe");
    if (!context->cancelled) {
        // Two capture channels may deliver callbacks in a different order from the
        // hardware edges. Sort by the hardware timestamp, not ISR receipt time.
        if (context->samples > 1) std::sort(context->history, context->history + context->samples, analysis::before);
        const auto summary = analysis::inspect(context->history, context->samples, context->resolution, [](const analysis::Cycle &){});
        size_t index = 0, traced = 0, anomalies = 0;
        const size_t stride = std::max<size_t>(1, summary.period.count / 14);
        analysis::inspect(context->history, context->samples, context->resolution,
            [&](const analysis::Cycle &cycle) {
                if (traced < 16 && (index % stride == 0 || index + 1 == summary.period.count)) {
                    ++traced;
                    ESP_LOGI(tag,
                    "PWM_CYCLE rise=%lu fall=%lu period=%lu high=%lu receipt_us=%lld",
                    static_cast<unsigned long>(cycle.rise_tick), static_cast<unsigned long>(cycle.fall_tick),
                    static_cast<unsigned long>(cycle.period_ticks), static_cast<unsigned long>(cycle.high_ticks),
                    static_cast<long long>(cycle.rise_cpu_us));
                }
                if (anomalies < 16 && (cycle.high_ticks < context->resolution / 2000 || cycle.high_ticks > context->resolution / 400)) {
                    ++anomalies;
                    ESP_LOGW(tag, "PWM_OUTSIDE_500_2500US rise=%lu high=%lu", static_cast<unsigned long>(cycle.rise_tick), static_cast<unsigned long>(cycle.high_ticks));
                }
                ++index;
            });
        const auto &period = summary.period; const auto &high = summary.high;
        uint32_t max_command_step = 0;
        for (size_t i = 1; i < context->command_count; ++i) {
            const auto a = context->commands[i-1].duty, b = context->commands[i].duty;
            max_command_step = std::max(max_command_step, a > b ? a-b : b-a);
        }
        ESP_LOGI(tag, "PWM_SUMMARY valid=%u period_ticks=%lu..%lu high_ticks=%lu..%lu high_step_ticks_max=%lu command_step_duty_max=%lu repeated_edges=%u gaps=%u invalid_widths=%u (capture gaps do not prove missing PWM)",
                 static_cast<unsigned>(period.count),
                 static_cast<unsigned long>(period.count ? period.minimum : 0), static_cast<unsigned long>(period.maximum),
                 static_cast<unsigned long>(high.count ? high.minimum : 0), static_cast<unsigned long>(high.maximum),
                 static_cast<unsigned long>(summary.max_adjacent_high_step), static_cast<unsigned long>(max_command_step),
                 static_cast<unsigned>(summary.repeated_edges), static_cast<unsigned>(summary.gaps),
                 static_cast<unsigned>(summary.invalid_widths));
        const size_t command_stride = std::max<size_t>(1, (context->command_count + 15) / 16);
        for (size_t i = 0; i < context->command_count; i += command_stride) ESP_LOGI(tag,
            "PWM_COMMAND receipt_us=%lld duty=%lu/65536 period_us=20000",
            static_cast<long long>(context->commands[i].cpu_us), static_cast<unsigned long>(context->commands[i].duty));
    }
    heap_caps_free(context->history); heap_caps_free(context->commands);
    context->~Context(); heap_caps_free(context); context = nullptr;
}

esp_err_t failed_start(esp_err_t error)
{
    context->cancelled = context->finishing = true;
    finish();
    return error;
}
}

bool active() { return context != nullptr; }

esp_err_t start(uint32_t commanded_duty)
{
    if (context || commanded_duty == 0 || commanded_duty >= 65536) return ESP_ERR_INVALID_STATE;
    void *memory = heap_caps_malloc(sizeof(Context), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!memory) return ESP_ERR_NO_MEM;
    context = new(memory) Context{};
    context->history = static_cast<analysis::Edge *>(heap_caps_malloc(history_size * sizeof(analysis::Edge), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    context->commands = static_cast<Command *>(heap_caps_malloc(command_size * sizeof(Command), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!context->history || !context->commands) return failed_start(ESP_ERR_NO_MEM);
    esp_err_t error = gpio_get_io_config(GPIO_NUM_23, &context->original_gpio);
    if (error != ESP_OK) return failed_start(error);
    context->have_gpio = true;
    if (!context->original_gpio.oe) return failed_start(ESP_ERR_INVALID_STATE);
    mcpwm_capture_timer_config_t timer_config{};
    timer_config.group_id = 0;
    timer_config.clk_src = MCPWM_CAPTURE_CLK_SRC_APB;
    error = mcpwm_new_capture_timer(&timer_config, &timer);
    if (error != ESP_OK) return failed_start(error);
    error = mcpwm_capture_timer_get_resolution(timer, &context->resolution);
    if (error != ESP_OK) return failed_start(error);
    // Half-wrap must exceed the complete window for modular timestamp sorting.
    if (!context->resolution || context->resolution > 200000000U) return failed_start(ESP_ERR_NOT_SUPPORTED);
    const mcpwm_capture_event_callbacks_t callbacks{.on_cap = captured};
    for (int i = 0; i < 2; ++i) {
        mcpwm_capture_channel_config_t config{};
        config.gpio_num = GPIO_NUM_23; config.prescale = 1;
        config.flags.pos_edge = i == 0; config.flags.neg_edge = i == 1;
        config.flags.pull_up = context->original_gpio.pu;
        config.flags.pull_down = context->original_gpio.pd;
        // Output is already enabled. Leave io_loop_back clear: this diagnostic
        // requires no write even to GPIO output-enable, only the input path.
        error = mcpwm_new_capture_channel(timer, &config, &channels[i]);
        if (error != ESP_OK) return failed_start(error);
        error = mcpwm_capture_channel_register_event_callbacks(channels[i], &callbacks, context);
        if (error != ESP_OK) return failed_start(error);
    }
    error = mcpwm_capture_timer_enable(timer);
    if (error != ESP_OK) return failed_start(error);
    timer_enabled = true;
    error = mcpwm_capture_timer_start(timer);
    if (error != ESP_OK) return failed_start(error);
    timer_started = true;
    for (int i = 0; i < 2; ++i) {
        error = mcpwm_capture_channel_enable(channels[i]);
        if (error != ESP_OK) return failed_start(error);
        channel_enabled[i] = true;
    }
    context->started = esp_timer_get_time();
    context->commands[context->command_count++] = Command{context->started, commanded_duty};
    ESP_LOGI(tag, "Passive GPIO23 PWM capture started: 8 s, hz=%lu, two edge channels; jaw output unchanged",
             static_cast<unsigned long>(context->resolution));
    return ESP_OK;
}

void poll(uint32_t commanded_duty, bool idle)
{
    if (!context) return;
    if (!idle) context->cancelled = context->finishing = true;
    if (!context->finishing) {
        drain();
        if (!context->command_count || context->commands[context->command_count - 1].duty != commanded_duty) {
            if (context->command_count < command_size) context->commands[context->command_count++] = Command{esp_timer_get_time(), commanded_duty};
            else context->truncated = true;
        }
        if (esp_timer_get_time() - context->started >= duration_us || context->truncated)
            context->finishing = true;
    }
    if (context->finishing) finish();
}

void cancel()
{
    if (!context) return;
    context->cancelled = context->finishing = true;
    finish();
}
}
