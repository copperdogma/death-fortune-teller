#include "skull_actuators.h"

#include <algorithm>
#include "driver/ledc.h"
#include "esp_private/periph_ctrl.h"
#include "hal/ledc_ll.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace skull::actuators {
namespace {
constexpr char tag[] = "skull_actuators";
constexpr ledc_mode_t mode = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t jaw_channel = LEDC_CHANNEL_0;
constexpr ledc_channel_t eye_left_channel = LEDC_CHANNEL_1;
constexpr ledc_channel_t eye_right_channel = LEDC_CHANNEL_2;
constexpr uint32_t dim = 100;
constexpr uint32_t bright = 255;
constexpr int jaw_max = 80;
unsigned minimum_us = 1400, maximum_us = 1600;
bool reverse_direction = false;
constexpr int64_t check_half_us = 600000;
constexpr int64_t breathing_interval_us = 7000000;
constexpr int64_t breathing_movement_us = 2000000;
constexpr int64_t breathing_hold_us = 100000;

bool ready = false; // Jaw initialized.
bool lights_ready = false;
bool peripheral_reset_done = false;
bool check_pending = false;
int64_t check_started = -1;
int64_t last_tick = -1;
int64_t last_breath_started = -1;
int64_t breath_started = -1;
double smoothed_amplitude = 0;
int previous_jaw = 0;
uint32_t last_jaw_duty = 0;
uint32_t programmed_jaw_duty = 0;
JawMode selected_jaw_mode = JawMode::normal;
bool jaw_pulses_stopped = false;
uint32_t last_eye_duty[2] = {dim, dim};
Stats telemetry = {};
int64_t pending_since[3] = {-1, -1, -1};
bool pending_warned[3] = {};
unsigned diagnostic_count = 0;
bool diagnostic_repeat = false;
int64_t diagnostic_started = -1;
constexpr int64_t blink_half_us = 200000;
constexpr int64_t diagnostic_gap_us = 2000000;

// Preserve Death's ServoESP32 calibrated mapping, including its 3-degree
// endpoint margin. Min/max pulses span library angles 0..180, not app 0..80.
uint32_t jaw_duty(int degrees)
{
    int bounded = std::clamp(degrees, 3, jaw_max - 3);
    if (reverse_direction) bounded = 180 - bounded;
    const uint32_t pulse_us = minimum_us + bounded * (maximum_us - minimum_us) / 180;
    return (pulse_us * 65536U + 10000U) / 20000U;
}

// Retain every calibrated integer-angle pulse exactly, but avoid whole-degree
// stair steps during slow gestures. Interpolate between those proven points;
// the final quantization is only the hardware's 16-bit PWM resolution.
uint32_t gesture_duty(double degrees)
{
    const double bounded = std::clamp(degrees, 3.0, static_cast<double>(jaw_max - 3));
    const int lower = static_cast<int>(bounded);
    const double fraction = bounded - lower;
    return static_cast<uint32_t>(jaw_duty(lower) * (1.0 - fraction) +
                                 jaw_duty(lower + 1) * fraction + 0.5);
}

double gesture_angle(int64_t elapsed, int64_t duration)
{
    const double t = std::clamp(static_cast<double>(elapsed) / duration, 0.0, 1.0);
    // Smoothstep starts/stops with zero requested velocity while retaining the
    // existing duration, midpoint and endpoints. Speech keeps its own smoothing.
    return 30.0 * t * t * (3.0 - 2.0 * t);
}

esp_err_t configure_timer(ledc_timer_t timer, ledc_timer_bit_t bits, uint32_t hz)
{
    ledc_timer_config_t config = {};
    config.speed_mode = mode;
    config.duty_resolution = bits;
    config.timer_num = timer;
    config.freq_hz = hz;
    config.clk_cfg = LEDC_USE_APB_CLK;
    return ledc_timer_config(&config);
}

esp_err_t configure_channel(int gpio, ledc_channel_t channel,
                            ledc_timer_t timer, uint32_t duty)
{
    ledc_channel_config_t config = {};
    config.gpio_num = gpio;
    config.speed_mode = mode;
    config.channel = channel;
    config.intr_type = LEDC_INTR_DISABLE;
    config.timer_sel = timer;
    config.duty = duty;
    config.hpoint = 0;
    return ledc_channel_config(&config);
}

esp_err_t checked(esp_err_t error, const char *operation)
{
    if (error != ESP_OK) {
        ESP_LOGE(tag, "%s failed: %s; actuator updates disabled", operation,
                 esp_err_to_name(error));
        ready = false;
        lights_ready = false;
    }
    return error;
}

bool update_pending(ledc_channel_t channel, int64_t now)
{
    // Classic ESP32 only: IDF's LL function busy-waits on this register with
    // interrupts disabled. Single task owns these channels, so a cleared bit
    // cannot become newly pending between this check and our own update.
    auto *hw = LEDC_LL_GET_HW();
    const bool pending = hw->channel_group[mode].channel[channel].conf1.duty_start != 0;
    if (pending) {
        telemetry.pending_channels |= 1U << channel;
        if (pending_since[channel] < 0) pending_since[channel] = now;
        const uint64_t duration = now - pending_since[channel];
        telemetry.max_pending_us = std::max(telemetry.max_pending_us, duration);
        if (duration >= 100000 && !pending_warned[channel]) {
            pending_warned[channel] = true;
            ESP_LOGW(tag, "LEDC channel %d duty_start pending >=100 ms; updates deferred", channel);
        }
    } else {
        telemetry.pending_channels &= ~(1U << channel);
        if (pending_since[channel] >= 0) {
            telemetry.max_pending_us = std::max(telemetry.max_pending_us,
                static_cast<uint64_t>(now - pending_since[channel]));
        }
        pending_since[channel] = -1;
        pending_warned[channel] = false;
    }
    telemetry.stuck = pending_warned[0] || pending_warned[1] || pending_warned[2];
    return pending;
}

enum class WriteResult { applied, deferred, failed };
WriteResult write_duty(ledc_channel_t channel, uint32_t duty)
{
    // Guard BEFORE set_duty as well: reprogramming an active change can upset
    // the peripheral's internal logic even before update_duty enters its wait.
    if (update_pending(channel, esp_timer_get_time())) {
        ++telemetry.deferred_updates;
        return WriteResult::deferred;
    }
    if (checked(ledc_set_duty(mode, channel, duty), "set duty") != ESP_OK) return WriteResult::failed;
    if (checked(ledc_update_duty(mode, channel), "update duty") != ESP_OK) return WriteResult::failed;
    return WriteResult::applied;
}
}

esp_err_t configure(unsigned min_us, unsigned max_us, bool reverse)
{
    if (min_us < 500 || max_us > 2500 || min_us >= max_us) return ESP_ERR_INVALID_ARG;
    minimum_us = min_us;
    maximum_us = max_us;
    reverse_direction = reverse;
    // Configuration runs on the same control task, or at boot before it starts.
    // Force the next tick to publish even if the logical jaw angle is unchanged.
    last_jaw_duty = 0;
    return ESP_OK;
}

esp_err_t init_lights()
{
    if (lights_ready) return ESP_OK;
    // Reset once, before any channel starts. A later init() must not reset
    // lights which are already reporting boot/storage failures.
    if (!peripheral_reset_done) {
        periph_module_reset(PERIPH_LEDC_MODULE);
        peripheral_reset_done = true;
    }
    esp_err_t error = configure_timer(LEDC_TIMER_1, LEDC_TIMER_8_BIT, 5000);
    if (checked(error, "eye timer init") != ESP_OK) return error;
    error = configure_channel(32, eye_left_channel, LEDC_TIMER_1, dim);
    if (checked(error, "left eye channel init") != ESP_OK) return error;
    error = configure_channel(33, eye_right_channel, LEDC_TIMER_1, dim);
    if (checked(error, "right eye channel init") != ESP_OK) return error;
    last_eye_duty[0] = last_eye_duty[1] = dim;
    last_tick = -1;
    lights_ready = true;
    return ESP_OK;
}

esp_err_t init()
{
    if (ready) return ESP_OK;
    esp_err_t error = init_lights();
    if (error != ESP_OK) return error;
    error = configure_timer(LEDC_TIMER_0, LEDC_TIMER_16_BIT, 50);
    if (checked(error, "jaw timer init") != ESP_OK) return error;
    last_jaw_duty = jaw_duty(0);
    error = configure_channel(23, jaw_channel, LEDC_TIMER_0, last_jaw_duty);
    if (checked(error, "jaw channel init") != ESP_OK) return error;
    programmed_jaw_duty = last_jaw_duty;
    previous_jaw = 0;
    smoothed_amplitude = 0;
    last_tick = -1;
    last_breath_started = -1;
    breath_started = -1;
    ready = true;
    test();
    ESP_LOGI(tag, "Jaw GPIO23 calibrated closed, eyes GPIO32/33 active");
    return ESP_OK;
}

void test()
{
    if (!ready || selected_jaw_mode != JawMode::normal) return;
    check_pending = true;
    check_started = -1;
    breath_started = -1;
}

uint32_t diagnostic_duration_ms(unsigned count, bool include_gap)
{
    return static_cast<uint32_t>((static_cast<int64_t>(count) * 2 * blink_half_us + (include_gap ? diagnostic_gap_us : 0)) / 1000);
}

void diagnostic(unsigned count, bool repeat)
{
    diagnostic_count = count;
    diagnostic_repeat = repeat;
    diagnostic_started = -1;
}

Stats stats() { return telemetry; }
uint32_t commanded_jaw_duty() { return programmed_jaw_duty; }

JawMode jaw_mode() { return selected_jaw_mode; }
const char *jaw_mode_name()
{
    switch (selected_jaw_mode) {
        case JawMode::normal: return "normal";
        case JawMode::hold: return "hold";
        case JawMode::off: return "off";
    }
    return "unknown";
}

esp_err_t set_jaw_mode(JawMode selected)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    if (selected != JawMode::normal && selected != JawMode::hold && selected != JawMode::off)
        return ESP_ERR_INVALID_ARG;
    selected_jaw_mode = selected;
    check_pending = false;
    check_started = -1;
    breath_started = -1;
    last_breath_started = esp_timer_get_time();
    smoothed_amplitude = 0;
    previous_jaw = 0;
    // Off -> normal/hold must restore the output even if the requested closed
    // position equals the last pulse emitted before ledc_stop().
    last_jaw_duty = 0;
    ESP_LOGI(tag, "Jaw diagnostic mode=%s; automatic motion %s", jaw_mode_name(),
             selected == JawMode::normal ? "enabled" : "disabled");
    return ESP_OK;
}

void tick(uint16_t rms, bool speaking, bool audio_busy, int jaw_override)
{
    if (!lights_ready) return;
    const int64_t now = esp_timer_get_time();
    if (last_tick >= 0 && now - last_tick < 20000) return;
    last_tick = now;
    for (int channel = ready ? 0 : 1; channel < 3; ++channel) {
        update_pending(static_cast<ledc_channel_t>(channel), now);
    }
    if (ready && selected_jaw_mode == JawMode::off) {
        // Keep the eye channels running. Do not replace off with a close pulse
        // for speech, tests, or Dead Stop: the elastic return closes the jaw.
        if (!jaw_pulses_stopped && !update_pending(jaw_channel, now)) {
            if (checked(ledc_stop(mode, jaw_channel, 0), "stop jaw pulses") != ESP_OK) return;
            jaw_pulses_stopped = true;
            programmed_jaw_duty = 0;
            last_jaw_duty = 0;
        }
    } else if (ready) {
        // Start the idle cadence at the first control tick, after potentially long
        // Matter initialization, so boot never jumps into an overdue breath.
        if (last_breath_started < 0) last_breath_started = now;
        int jaw = 0;
        double fractional_jaw = -1;
        if (selected_jaw_mode == JawMode::hold) {
            // A fixed calibrated closed target, independent of all animations.
            jaw = 0;
        } else if (jaw_override >= 0) {
            jaw = std::clamp(jaw_override, 0, jaw_max);
            check_pending = false;
            breath_started = -1;
            last_breath_started = now;
            smoothed_amplitude = 0;
        } else if (speaking) {
            check_pending = false;
            breath_started = -1;
            // Match skull_audio_animator.cpp's legacy amplitude and jaw smoothing.
            smoothed_amplitude = 0.1 * rms + 0.9 * smoothed_amplitude;
            double amplitude = std::min(smoothed_amplitude * 5.0, 15000.0);
            if (amplitude < 1000.0) amplitude = 0;
            const int target = static_cast<int>(amplitude * jaw_max / 15000.0);
            jaw = static_cast<int>(0.2 * target + 0.8 * previous_jaw);
        } else {
            smoothed_amplitude = 0;
            if (audio_busy) {
                // Audio preroll and quiet diagnostic tones also own the jaw. Never
                // resume an interrupted gesture when audio releases it.
                check_pending = false;
                breath_started = -1;
            } else if (check_pending) {
                if (check_started < 0) {
                    check_started = now;
                    ESP_LOGI(tag, "Gentle jaw check 0 -> 30 -> 0 degrees");
                }
                const int64_t elapsed = now - check_started;
                if (elapsed < check_half_us) {
                    fractional_jaw = gesture_angle(elapsed, check_half_us);
                } else if (elapsed < 2 * check_half_us) {
                    fractional_jaw = gesture_angle(2 * check_half_us - elapsed, check_half_us);
                } else {
                    check_pending = false;
                    ESP_LOGI(tag, "Gentle jaw check complete; jaw closed");
                }
            } else {
                if (breath_started < 0 && now - last_breath_started >= breathing_interval_us) {
                    // Start-to-start cadence, without replaying missed breaths
                    // after audio or a delayed tick.
                    breath_started = now;
                    last_breath_started = now;
                    ESP_LOGI(tag, "Idle breath: 2 s open, 100 ms hold, 2 s close");
                }
                if (breath_started >= 0) {
                    const int64_t elapsed = now - breath_started;
                    if (elapsed < breathing_movement_us) {
                        fractional_jaw = gesture_angle(elapsed, breathing_movement_us);
                    } else if (elapsed < breathing_movement_us + breathing_hold_us) {
                        jaw = 30;
                    } else if (elapsed < 2 * breathing_movement_us + breathing_hold_us) {
                        fractional_jaw = gesture_angle(2 * breathing_movement_us + breathing_hold_us - elapsed,
                                                       breathing_movement_us);
                    } else {
                        breath_started = -1;
                    }
                }
            }
        }
        previous_jaw = fractional_jaw >= 0 ? static_cast<int>(fractional_jaw) : jaw;
        const uint32_t duty = fractional_jaw >= 0 ? gesture_duty(fractional_jaw) : jaw_duty(jaw);
        if (duty != last_jaw_duty) {
            const auto result = write_duty(jaw_channel, duty);
            if (result == WriteResult::failed) return;
            if (result == WriteResult::applied) {
                last_jaw_duty = duty;
                programmed_jaw_duty = duty;
                jaw_pulses_stopped = false;
            }
        }
    }
    uint32_t eye_duty = speaking ? bright : dim;
    if (diagnostic_count) {
        if (diagnostic_started < 0) diagnostic_started = now;
        const int64_t duration = static_cast<int64_t>(diagnostic_count) * 2 * blink_half_us;
        int64_t phase = now - diagnostic_started;
        if (diagnostic_repeat) phase %= duration + diagnostic_gap_us;
        if (phase < duration) {
            eye_duty = (phase / blink_half_us) % 2 == 0 ? bright : 0;
        } else if (diagnostic_repeat) {
            eye_duty = 0;
        } else {
            diagnostic_count = 0;
        }
    }
    for (int eye = 0; eye < 2; ++eye) {
        if (eye_duty != last_eye_duty[eye]) {
            const auto channel = eye == 0 ? eye_left_channel : eye_right_channel;
            const auto result = write_duty(channel, eye_duty);
            if (result == WriteResult::failed) return;
            if (result == WriteResult::applied) last_eye_duty[eye] = eye_duty;
        }
    }
}
}
