// Exercises the actual production driver against deterministic hardware/time
// stubs. This proves commanded waveforms and state transitions, not real motion.
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
#include "driver/ledc.h"
#include "esp_private/periph_ctrl.h"
#include "hal/ledc_ll.h"
#include "skull_actuators.h"

actuator_test_ledc_t actuator_test_ledc = {};

namespace {
int64_t now_us = 10000000;
std::vector<ledc_timer_config_t> timers;
std::vector<ledc_channel_config_t> channels;
std::array<uint32_t, 3> duty = {};
unsigned updates = 0;
unsigned resets = 0;
unsigned jaw_stops = 0;
bool jaw_output_enabled = true;
std::array<unsigned, 3> sets = {};
std::array<unsigned, 3> channel_updates = {};
void tick_after(int64_t us, uint16_t rms = 0, bool speaking = false, bool audio_busy = false)
{
    now_us += us;
    skull::actuators::tick(rms, speaking, audio_busy);
}
void tick_at(int64_t us, uint16_t rms = 0, bool speaking = false, bool audio_busy = false)
{
    assert(us >= now_us);
    tick_after(us - now_us, rms, speaking, audio_busy);
}
uint32_t jaw_duty(int degrees)
{
    return ((1400U + std::clamp(degrees, 3, 77) * 200U / 180U) * 65536U + 10000U) / 20000U;
}
void expect_idle_jaw(int degrees)
{
    assert(duty[0] == jaw_duty(degrees));
    assert(duty[1] == 100 && duty[2] == 100);
}
void expect_eyes(uint32_t value)
{
    assert(duty[1] == value && duty[2] == value);
}
}
int64_t esp_timer_get_time() { return now_us; }
void periph_module_reset(periph_module_t module)
{
    assert(module == PERIPH_LEDC_MODULE && timers.empty() && channels.empty());
    ++resets;
    actuator_test_ledc = {};
}
esp_err_t ledc_timer_config(const ledc_timer_config_t *config)
{
    assert(resets == 1);
    // Each timer must precede its own channels; LED-only boot has eye outputs
    // before the jaw timer is deliberately configured after SD succeeds.
    for (const auto &channel : channels) assert(channel.timer_sel != config->timer_num);
    timers.push_back(*config);
    return ESP_OK;
}
esp_err_t ledc_channel_config(const ledc_channel_config_t *config)
{
    bool configured = false;
    for (const auto &timer : timers) if (timer.timer_num == config->timer_sel) configured = true;
    assert(configured);
    channels.push_back(*config);
    duty[config->channel] = config->duty;
    return ESP_OK;
}
esp_err_t ledc_set_duty(ledc_mode_t mode, ledc_channel_t channel, uint32_t value)
{
    assert(mode == LEDC_LOW_SPEED_MODE);
    assert(!actuator_test_ledc.channel_group[mode].channel[channel].conf1.duty_start);
    ++sets[channel];
    duty[channel] = value;
    return ESP_OK;
}
esp_err_t ledc_update_duty(ledc_mode_t mode, ledc_channel_t channel)
{
    assert(mode == LEDC_LOW_SPEED_MODE);
    assert(!actuator_test_ledc.channel_group[mode].channel[channel].conf1.duty_start);
    ++channel_updates[channel];
    ++updates;
    if (channel == LEDC_CHANNEL_0) jaw_output_enabled = true;
    return ESP_OK;
}
esp_err_t ledc_stop(ledc_mode_t mode, ledc_channel_t channel, uint32_t idle_level)
{
    assert(mode == LEDC_LOW_SPEED_MODE && channel == LEDC_CHANNEL_0 && idle_level == 0);
    assert(!actuator_test_ledc.channel_group[mode].channel[channel].conf1.duty_start);
    ++jaw_stops;
    jaw_output_enabled = false;
    return ESP_OK;
}

int main()
{
    using namespace skull::actuators;
    assert(jaw_mode() == JawMode::normal);
    assert(set_jaw_mode(JawMode::off) == ESP_ERR_INVALID_STATE);
    actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start = 1;
    assert(init_lights() == ESP_OK);
    assert(init_lights() == ESP_OK && resets == 1);
    assert(timers.size() == 1 && timers[0].timer_num == LEDC_TIMER_1);
    assert(channels.size() == 2 && channels[0].gpio_num == 32 && channels[1].gpio_num == 33);
    // An SD/config fault can blink indefinitely without setting up the jaw.
    // Even a stale jaw hardware flag cannot be inspected or acted upon here.
    actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start = 1;
    diagnostic(3, true);
    tick_after(20000);
    expect_eyes(255);
    tick_after(200000);
    expect_eyes(0);
    tick_after(200000);
    expect_eyes(255);
    tick_after(4000000);
    assert(channels.size() == 2 && timers.size() == 1);
    assert(sets[0] == 0 && channel_updates[0] == 0 && duty[0] == 0);
    assert(stats().pending_channels == 0 && !stats().stuck);
    assert(actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start == 1);
    actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start = 0;
    const auto boot_eye_duty = duty[1];
    diagnostic(2); // Repaired SD now permits calibrated jaw initialization.
    assert(init() == ESP_OK);
    assert(!actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start);
    assert(init() == ESP_OK && resets == 1); // Repeated init never resets live PWM.
    assert(init_lights() == ESP_OK && resets == 1); // Nor can late light setup reset jaw.
    assert(timers.size() == 2 && channels.size() == 3);
    assert(timers[1].timer_num == LEDC_TIMER_0);
    assert(timers[1].freq_hz == 50 && timers[1].duty_resolution == LEDC_TIMER_16_BIT);
    assert(timers[0].timer_num == LEDC_TIMER_1);
    assert(timers[0].freq_hz == 5000 && timers[0].duty_resolution == LEDC_TIMER_8_BIT);
    for (const auto &timer : timers) {
        assert(timer.speed_mode == LEDC_LOW_SPEED_MODE && timer.clk_cfg == LEDC_USE_APB_CLK);
    }
    assert(channels[2].gpio_num == 23 && channels[2].channel == LEDC_CHANNEL_0);
    assert(channels[2].timer_sel == LEDC_TIMER_0);
    assert(channels[0].gpio_num == 32 && channels[0].channel == LEDC_CHANNEL_1);
    assert(channels[1].gpio_num == 33 && channels[1].channel == LEDC_CHANNEL_2);
    assert(channels[0].timer_sel == LEDC_TIMER_1 && channels[1].timer_sel == LEDC_TIMER_1);
    assert(duty[0] == jaw_duty(0)); // Calibrated safe closed pulse, rounded to 16-bit PWM.
    assert(duty[1] == boot_eye_duty && duty[2] == boot_eye_duty); // Jaw init preserves lights.

    tick_after(40000000); // Late first diagnostic tick still begins at closed.
    const int64_t first_control_tick = now_us;
    assert(duty[0] == jaw_duty(0));
    expect_eyes(255); // Pre-init selection survives and anchors at first tick.
    tick_after(600000);
    assert(duty[0] == jaw_duty(30)); // 30 degrees within the configured pulse range.
    expect_eyes(0); // At 600 ms, the second flash is in its off half.
    const unsigned peak_updates = updates;
    tick_after(19999);
    assert(updates == peak_updates && duty[0] == jaw_duty(30));
    tick_after(1);
    // Ease-out may round the first tiny departure back to the peak PWM tick.
    assert(duty[0] <= jaw_duty(30) && duty[0] > jaw_duty(0));
    tick_after(580000);
    assert(duty[0] == jaw_duty(0)); // Exactly 1.2 seconds from first tick.
    expect_eyes(100); // Delayed tick completes the one-shot without replaying.

    test();
    tick_after(20000);
    tick_after(600000);
    assert(duty[0] == jaw_duty(30));
    tick_after(20000, 0, true); // Speech cancels check, even during silence.
    assert(duty[0] < jaw_duty(30));
    assert(duty[1] == 255 && duty[2] == 255);
    tick_after(20000);
    assert(duty[0] == jaw_duty(0) && duty[1] == 100 && duty[2] == 100);
    tick_after(600000);
    assert(duty[0] == jaw_duty(0)); // Interrupted boot check never resumes.

    const int64_t breath = first_control_tick + 7000000;
    tick_at(breath - 20000);
    expect_idle_jaw(0);
    tick_at(breath);
    expect_idle_jaw(0);
    tick_at(breath + 1000000);
    expect_idle_jaw(15);
    tick_at(breath + 2000000);
    expect_idle_jaw(30);
    const unsigned breathing_peak_updates = updates;
    tick_after(19999);
    assert(updates == breathing_peak_updates);
    tick_after(1);
    expect_idle_jaw(30);
    tick_at(breath + 2080000);
    expect_idle_jaw(30);
    tick_at(breath + 2100000);
    expect_idle_jaw(30);
    tick_at(breath + 3100000);
    expect_idle_jaw(15);
    tick_at(breath + 4100000);
    expect_idle_jaw(0);
    tick_at(breath + 6980000);
    expect_idle_jaw(0);
    tick_at(breath + 7000000); // Seven seconds from start, not completion.
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(15);

    tick_after(20000, 0, true, true); // Speech wins even while audio owns jaw.
    assert(duty[1] == 255 && duty[2] == 255);
    tick_after(20000);
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(0); // No stale mid-breath resume.

    // A long control delay finishes the old breath on the next tick, then
    // starts one new breath from closed instead of trying to catch up.
    tick_at(breath + 14000000);
    tick_after(1000000);
    expect_idle_jaw(15);
    tick_after(60000000);
    expect_idle_jaw(0);
    tick_after(20000);
    const int64_t delayed_breath = now_us;
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(15);
    tick_after(20000, 0, false, true); // Even silent non-speech audio owns jaw.
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(0);
    tick_at(delayed_breath + 7000000, 0, false, true);
    tick_after(4000000, 0, false, true);
    expect_idle_jaw(0);
    tick_after(20000); // Overdue cadence starts a fresh breath on audio end.
    const int64_t after_audio = now_us;
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(15);

    test(); // Manual check interrupts breathing and has priority while pending.
    tick_after(20000);
    expect_idle_jaw(0);
    tick_after(600000);
    expect_idle_jaw(30);
    tick_after(600000);
    expect_idle_jaw(0);
    tick_after(20000);
    expect_idle_jaw(0);
    test();
    tick_after(20000);
    tick_after(600000);
    expect_idle_jaw(30);
    tick_after(20000, 0, false, true); // Preroll also cancels manual/boot check.
    expect_idle_jaw(0);
    tick_after(20000);
    expect_idle_jaw(0);
    tick_after(600000);
    expect_idle_jaw(0);
    tick_at(after_audio + 7000000);
    tick_after(1000000);
    expect_idle_jaw(15);

    // Startup/manual check remains ahead of an already overdue idle cadence.
    test();
    tick_after(10000000);
    expect_idle_jaw(0);
    tick_after(600000);
    expect_idle_jaw(30);
    tick_after(600000);
    expect_idle_jaw(0);
    tick_after(20000);
    expect_idle_jaw(0); // Due breath begins only after the check finishes.
    tick_after(1000000);
    expect_idle_jaw(15);

    for (int i = 0; i < 200; ++i) {
        tick_after(20000, 65535, true);
        assert(duty[0] >= jaw_duty(0) && duty[0] <= jaw_duty(80)); // Restricted logical 3..77 degree range.
    }
    assert(duty[0] > jaw_duty(60)); // Loud input actually opens the jaw.
    tick_after(20000);
    assert(duty[0] == jaw_duty(0) && duty[1] == 100 && duty[2] == 100);

    // Exact on/off phases and flash counts for both role successes and errors.
    // The deliberately late first tick must anchor, not consume, the pattern.
    for (unsigned count : {4U, 2U, 3U, 5U}) {
        diagnostic(count);
        tick_after(2000000);
        expect_eyes(255);
        const int64_t start = now_us;
        unsigned flashes = 1;
        bool was_on = true;
        for (int64_t elapsed = 20000; elapsed < count * 400000LL; elapsed += 20000) {
            tick_at(start + elapsed);
            const bool on = (elapsed / 200000) % 2 == 0;
            expect_eyes(on ? 255 : 0);
            if (on && !was_on) ++flashes;
            was_on = on;
        }
        assert(flashes == count);
        assert(diagnostic_duration_ms(count, false) == count * 400);
        tick_at(start + count * 400000LL);
        expect_eyes(100);
    }

    for (unsigned count : {3U, 5U}) {
        diagnostic(count, true);
        tick_after(20000);
        const int64_t start = now_us;
        const int64_t duration = count * 400000LL;
        const int64_t cycle = duration + 2000000;
        assert(diagnostic_duration_ms(count) * 1000LL == cycle);
        for (int64_t elapsed = 0; elapsed <= 2 * cycle; elapsed += 20000) {
            // Start was already ticked; all remaining samples are 20 ms apart.
            if (elapsed) tick_at(start + elapsed);
            const int64_t phase = elapsed % cycle;
            const bool on = phase < duration && (phase / 200000) % 2 == 0;
            expect_eyes(on ? 255 : 0);
        }
        diagnostic(0);
        tick_after(20000);
        expect_eyes(100);
    }

    diagnostic(3, true);
    tick_after(20000, 65535, true);
    const int64_t speaking_start = now_us;
    expect_eyes(255);
    tick_at(speaking_start + 200000, 65535, true);
    expect_eyes(0); // Speech brightness cannot erase a diagnostic off phase.
    assert(duty[0] > jaw_duty(0)); // Jaw speech control continues independently.
    tick_at(speaking_start + 1080000, 65535, true);
    expect_eyes(0); // Delayed tick lands in phase 5, with no catch-up pulses.
    tick_at(speaking_start + 3200000 + 480000, 65535, true);
    expect_eyes(255); // Repeated cycle, phase 2, still active after speech.
    diagnostic(0);
    tick_after(20000, 65535, true);
    expect_eyes(255); // Clear resumes current speaking brightness.
    tick_after(20000);
    expect_eyes(100);

    diagnostic(2);
    tick_after(20000, 65535, true);
    tick_after(800000, 65535, true);
    expect_eyes(255); // One-shot completion also resumes speaking brightness.
    tick_after(20000);
    expect_eyes(100);
    // Pending jaw updates must not touch either duty API or prevent eye work.
    auto &hw = actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE];
    hw.channel[0].conf1.duty_start = 1;
    const auto jaw_sets = sets[0];
    const auto jaw_updates = channel_updates[0];
    const auto jaw_before = duty[0];
    test();
    tick_after(20000);
    tick_after(600000, 65535, true);
    assert(sets[0] == jaw_sets && duty[0] == jaw_before);
    assert(channel_updates[0] == jaw_updates);
    expect_eyes(255);
    assert(stats().deferred_updates > 0 && stats().pending_channels == 1);
    assert(stats().stuck && stats().max_pending_us >= 600000);
    hw.channel[0].conf1.duty_start = 0;
    tick_after(20000, 65535, true);
    assert(sets[0] > jaw_sets); // Deferred duty wasn't falsely cached as applied.
    assert(!stats().stuck && stats().pending_channels == 0);

    // Independent eye caches: a pending left eye doesn't suppress the right.
    hw.channel[1].conf1.duty_start = 1;
    const auto left_sets = sets[1];
    tick_after(20000);
    assert(sets[1] == left_sets && duty[1] == 255 && duty[2] == 100);
    hw.channel[1].conf1.duty_start = 0;
    tick_after(20000);
    expect_eyes(100);
    assert(sets[1] == left_sets + 1 && resets == 1);
    // Explicit Death jaw hold wins over high RMS, checks and overdue breathing.
    skull::actuators::tick(65535, true, true, 80);
    // Previous call used this timestamp; permit the next rate-gated tick.
    now_us += 20000;
    skull::actuators::tick(65535, true, true, 80);
    assert(duty[0] == jaw_duty(80));
    now_us += 20000;
    skull::actuators::tick(65535, true, true, 0);
    assert(duty[0] == jaw_duty(0));
    tick_after(20000); // Released hold starts closed, never resumes a stale breath.
    assert(duty[0] == jaw_duty(0));
    assert(configure(2500, 900, false) == ESP_ERR_INVALID_ARG);
    assert(configure(499, 1600, false) == ESP_ERR_INVALID_ARG);
    assert(configure(900, 2200, false) == ESP_OK);
    now_us += 20000;
    skull::actuators::tick(0, false, true, 80);
    const unsigned forward = ((900U + 77U * 1300U / 180U) * 65536U + 10000U) / 20000U;
    assert(duty[0] == forward);
    assert(configure(900, 2200, true) == ESP_OK);
    now_us += 20000;
    skull::actuators::tick(0, false, true, 0);
    const unsigned reversed = ((900U + 177U * 1300U / 180U) * 65536U + 10000U) / 20000U;
    assert(duty[0] == reversed); // Reverse preserves ServoESP32 180-angle semantics.
    // Fixed closed hold suppresses checks, speech and explicit open overrides.
    assert(set_jaw_mode(JawMode::hold) == ESP_OK);
    test();
    now_us += 20000;
    skull::actuators::tick(65535, true, true, 80);
    assert(duty[0] == reversed && jaw_output_enabled);
    expect_eyes(255);
    tick_after(10000000);
    assert(duty[0] == reversed); // Overdue breathing cannot escape hold.
    expect_eyes(100);

    // Off waits for an in-flight hardware update, then stops exactly once.
    assert(set_jaw_mode(JawMode::off) == ESP_OK);
    hw.channel[0].conf1.duty_start = 1;
    tick_after(20000, 65535, true);
    assert(jaw_stops == 0 && jaw_output_enabled);
    assert(commanded_jaw_duty() == duty[0]);
    expect_eyes(255);
    hw.channel[0].conf1.duty_start = 0;
    tick_after(20000);
    assert(jaw_stops == 1 && !jaw_output_enabled);
    assert(commanded_jaw_duty() == 0);
    expect_eyes(100);
    const auto off_sets = sets[0];
    test();
    now_us += 20000;
    skull::actuators::tick(65535, true, true, 0); // Dead Stop's close request.
    assert(jaw_stops == 1 && sets[0] == off_sets && !jaw_output_enabled);
    expect_eyes(255);
    tick_after(10000000);
    assert(jaw_stops == 1 && sets[0] == off_sets && !jaw_output_enabled);

    // Resume restores pulses even when the desired closed duty is unchanged.
    // A pending update must defer restoration without falsely filling cache.
    assert(set_jaw_mode(JawMode::hold) == ESP_OK);
    hw.channel[0].conf1.duty_start = 1;
    tick_after(20000);
    assert(sets[0] == off_sets && !jaw_output_enabled);
    assert(commanded_jaw_duty() == 0); // Pending resume is not a programmed pulse.
    hw.channel[0].conf1.duty_start = 0;
    tick_after(20000);
    assert(sets[0] == off_sets + 1 && duty[0] == reversed && jaw_output_enabled);
    assert(commanded_jaw_duty() == reversed);
    assert(set_jaw_mode(JawMode::off) == ESP_OK);
    tick_after(20000);
    assert(jaw_stops == 2 && !jaw_output_enabled);
    assert(set_jaw_mode(JawMode::normal) == ESP_OK);
    tick_after(20000);
    assert(jaw_output_enabled && duty[0] == reversed);
    const int64_t resumed = now_us;
    tick_at(resumed + 2000000);
    assert(duty[0] == reversed); // Neither old check nor old breath is replayed.
    tick_at(resumed + 8000000);
    tick_after(1000000);
    assert(duty[0] < reversed); // Normal breathing resumes on a fresh cadence.

    // Sample every physical PWM frame using the verified card calibration.
    // Motion stays monotonic and bounded, but intermediate pulses need not land
    // on the old whole-degree stair steps. Exact calibrated endpoints survive.
    assert(configure(1750, 2100, true) == ESP_OK);
    const auto calibrated = [](int degrees) {
        const unsigned angle = 180 - std::clamp(degrees, 3, 77);
        return ((1750U + angle * 350U / 180U) * 65536U + 10000U) / 20000U;
    };
    assert(set_jaw_mode(JawMode::normal) == ESP_OK);
    tick_after(20000);
    tick_after(7000000);
    uint32_t previous = duty[0];
    assert(previous == calibrated(0));
    unsigned fractional_samples = 0;
    unsigned sub_degree_steps = 0;
    for (int frame = 1; frame <= 100; ++frame) {
        tick_after(20000);
        const uint32_t actual = duty[0];
        assert(actual <= previous && actual >= calibrated(30));
        if (actual < previous && previous - actual < 6) ++sub_degree_steps;
        bool whole_degree = false;
        for (int angle = 0; angle <= 30; ++angle)
            if (actual == calibrated(angle)) whole_degree = true;
        if (!whole_degree) ++fractional_samples;
        previous = actual;
    }
    assert(duty[0] == calibrated(30));
    assert(fractional_samples > 30 && sub_degree_steps > 30);
    for (int frame = 0; frame < 5; ++frame) {
        tick_after(20000);
        assert(duty[0] == calibrated(30)); // Existing 100 ms hold unchanged.
    }
    for (int frame = 1; frame <= 100; ++frame) {
        tick_after(20000);
        assert(duty[0] >= previous && duty[0] <= calibrated(0));
        previous = duty[0];
    }
    assert(duty[0] == calibrated(0));

    // Gentle boot/manual check uses the same fractional/eased trajectory while
    // preserving its 600 ms halves and calibrated 30-degree peak.
    test();
    tick_after(20000);
    previous = duty[0];
    for (int frame = 1; frame <= 30; ++frame) {
        tick_after(20000);
        assert(duty[0] <= previous && duty[0] >= calibrated(30));
        previous = duty[0];
    }
    assert(duty[0] == calibrated(30));
    for (int frame = 1; frame <= 30; ++frame) {
        tick_after(20000);
        assert(duty[0] >= previous && duty[0] <= calibrated(0));
        previous = duty[0];
    }
    assert(duty[0] == calibrated(0));
    std::puts("Actuator command tests passed");
}
