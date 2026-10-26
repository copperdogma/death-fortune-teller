#pragma once
#include <cstdlib>
#include <atomic>
#include <cstddef>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define MALLOC_CAP_DMA 8
inline std::atomic<int> startup_test_allocations{0};
inline int startup_test_fail_at = 0;
inline int startup_test_last_caps = 0;
inline int startup_test_staging_caps = 0;
inline void *heap_caps_malloc(size_t n, int caps) {
    const int call = ++startup_test_allocations;
    startup_test_last_caps = caps;
    if (call == 2) startup_test_staging_caps = caps;
    return call == startup_test_fail_at ? nullptr : std::malloc(n);
}
inline void heap_caps_free(void *p) { std::free(p); }
