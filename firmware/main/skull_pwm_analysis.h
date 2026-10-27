#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace skull::pwm_probe::analysis {
struct Edge { uint32_t tick; uint32_t rising; int64_t cpu_us; };
struct Range {
    uint32_t minimum = std::numeric_limits<uint32_t>::max(), maximum = 0;
    uint64_t total = 0;
    size_t count = 0;
    void add(uint32_t value) {
        minimum = std::min(minimum, value); maximum = std::max(maximum, value);
        total += value; ++count;
    }
};
struct Cycle { uint32_t rise_tick, fall_tick, period_ticks, high_ticks; int64_t rise_cpu_us; };
struct Summary {
    Range period, high;
    size_t repeated_edges = 0, gaps = 0, invalid_widths = 0;
    uint32_t max_adjacent_high_step = 0;
};

// Capture windows must be shorter than half the 32-bit timer wrap interval.
inline bool before(const Edge &a, const Edge &b) {
    return a.tick != b.tick && static_cast<uint32_t>(b.tick - a.tick) < 0x80000000U;
}

template<class Emit>
Summary inspect(const Edge *edges, size_t count, uint32_t hz, Emit emit) {
    Summary result;
    bool have_rise = false, have_fall = false;
    bool have_previous_high = false;
    uint32_t previous_high = 0;
    Edge rise{}, fall{};
    for (size_t i = 0; i < count; ++i) {
        const Edge &edge = edges[i];
        if (!edge.rising) {
            if (!have_rise || have_fall) { ++result.repeated_edges; continue; }
            fall = edge; have_fall = true; continue;
        }
        if (have_rise) {
            const uint32_t period = edge.tick - rise.tick;
            // A gap can be a missed capture event. It is not proof of missing PWM.
            if (period < hz / 100 || period > hz / 40) { ++result.gaps; have_previous_high = false; }
            else if (have_fall) {
                const uint32_t high = fall.tick - rise.tick;
                if (high == 0 || high >= period) { ++result.invalid_widths; have_previous_high = false; }
                else {
                    result.period.add(period); result.high.add(high);
                    if (have_previous_high) result.max_adjacent_high_step = std::max(result.max_adjacent_high_step,
                        high > previous_high ? high - previous_high : previous_high - high);
                    previous_high = high; have_previous_high = true;
                    emit(Cycle{rise.tick, fall.tick, period, high, rise.cpu_us});
                }
            } else { ++result.repeated_edges; have_previous_high = false; }
        }
        rise = edge; have_rise = true; have_fall = false;
    }
    return result;
}
}
