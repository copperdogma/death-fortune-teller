#pragma once
template <typename... Args> inline void storage_test_log(Args...) {}
#define ESP_LOGI(...) storage_test_log(__VA_ARGS__)
#define ESP_LOGE(...) storage_test_log(__VA_ARGS__)
