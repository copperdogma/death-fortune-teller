#pragma once
constexpr int GPIO_NUM_0 = 0, GPIO_NUM_2 = 2, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14,
              GPIO_NUM_15 = 15, GPIO_NUM_NC = -1, GPIO_PULLUP_ONLY = 1;
inline int gpio_set_pull_mode(int, int) { return 0; }
