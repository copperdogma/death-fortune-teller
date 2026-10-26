#pragma once
#include <cstdarg>
inline void startup_test_log(const char *, const char *, ...) {}
#define ESP_LOGE(tag, ...) startup_test_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) startup_test_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) startup_test_log(tag, __VA_ARGS__)
