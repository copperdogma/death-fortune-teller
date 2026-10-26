#pragma once
constexpr int GPIO_NUM_15 = 15, GPIO_NUM_2 = 2, GPIO_PULLUP_ONLY = 1;
inline int gpio_set_pull_mode(int, int) { return 0; }
