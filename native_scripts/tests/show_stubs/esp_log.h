#pragma once
// Keep arguments evaluated so host builds also detect invalid logging expressions.
template <typename... Args> inline void show_test_log(const char *, const char *, Args...) {}
#define ESP_LOGI(...) show_test_log(__VA_ARGS__)
#define ESP_LOGW(...) show_test_log(__VA_ARGS__)
