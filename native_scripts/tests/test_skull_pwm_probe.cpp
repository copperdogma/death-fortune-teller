#include "skull_pwm_analysis.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

using namespace skull::pwm_probe::analysis;

int main()
{
    constexpr uint32_t hz = 80000000, period = 1600000, high = 167520;
    std::vector<Edge> edges{{0,1,100}, {high,0,200}, {period,1,300},
                            {period+high+80,0,400}, {2*period,1,500}};
    std::vector<Cycle> cycles;
    auto summary = inspect(edges.data(), edges.size(), hz, [&](Cycle c){cycles.push_back(c);});
    assert(summary.period.count == 2 && summary.period.minimum == period && summary.period.maximum == period);
    assert(summary.high.minimum == high && summary.high.maximum == high+80);
    assert(summary.gaps == 0 && summary.repeated_edges == 0 && summary.invalid_widths == 0);
    assert(summary.max_adjacent_high_step == 80);
    assert(cycles[1].rise_cpu_us == 300 && cycles[1].high_ticks == high+80);

    // ISR dispatch order need not match hardware edge order. The timestamp sort
    // also works across the 32-bit wrap within the bounded half-wrap window.
    constexpr uint32_t origin = 0xffff0000U;
    for (auto &edge : edges) edge.tick += origin;
    std::swap(edges[0], edges[1]); std::swap(edges[2], edges[3]);
    std::sort(edges.begin(), edges.end(), before);
    summary = inspect(edges.data(), edges.size(), hz, [](Cycle){});
    assert(summary.period.count == 2 && summary.period.minimum == period);
    assert(summary.high.minimum == high && summary.high.maximum == high+80);

    // Lost-latch/capture gaps must be diagnosed separately rather than folded
    // into the normal period jitter range or called missing physical pulses.
    edges = {{0,1,0}, {high,0,0}, {2*period,1,0}, {2*period+high,0,0}, {3*period,1,0}};
    summary = inspect(edges.data(), edges.size(), hz, [](Cycle){});
    assert(summary.gaps == 1 && summary.period.count == 1 && summary.period.minimum == period);

    edges = {{0,0,0}, {100,1,0}, {200,1,0}, {300,0,0}, {400,0,0}};
    summary = inspect(edges.data(), edges.size(), hz, [](Cycle){});
    assert(summary.repeated_edges >= 2 && summary.period.count == 0);
    summary = inspect(nullptr, 0, hz, [](Cycle){});
    assert(summary.period.count == 0);
    std::puts("PWM capture analysis: widths, periods, ISR reorder, timer wrap, capture gaps PASS");
}
