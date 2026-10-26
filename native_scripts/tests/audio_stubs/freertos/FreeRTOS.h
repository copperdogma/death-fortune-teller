#pragma once
#include <cstdint>
#include <atomic>
inline thread_local unsigned audio_test_publications_since_delay = 0;
inline std::atomic<unsigned> audio_test_max_burst{0};
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) ((x) / 10)
using TickType_t = uint32_t;
