#pragma once
template <typename... Args> inline void actuator_test_log(Args...) {}
#define ESP_LOGI(...) actuator_test_log(__VA_ARGS__)
#define ESP_LOGE(...) actuator_test_log(__VA_ARGS__)
#define ESP_LOGW(...) actuator_test_log(__VA_ARGS__)
